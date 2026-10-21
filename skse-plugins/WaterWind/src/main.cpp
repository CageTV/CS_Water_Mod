// WaterWind: makes open water respond to the weather's wind.
//
// Verified with WaterProbe on Skyrim 1.7.104:
//  * Vanilla water ignores the weather. Each water type (WATR) scrolls its three normal-map layers at the record's noise "wind speed";
//    the engine copies that record value into the live material every frame, so writing the RECORD changes the scroll rate within a second.
//  * The live material's wave amplitude (BSWaterShaderMaterial::amplitudeA, what the shader blends the three normal layers with) is NOT
//    rewritten by the engine, so writing the MATERIAL of each water object sticks.
//
// Each tick this plugin
//    sets   record.noiseWindSpeedA[i]   = base[i] * speedScale(wind)      for every water type
//    sets   material.amplitudeA[i]      = record.amplitudeA[i] * ampScale(wind)   for every active water object
// The scale is 1 at a "normal breeze" (fReferenceWind), falls to the calm value in no wind and rises to the storm value in a gale.
// Works with any water renderer (vanilla, Community Shaders, Open Shaders, ENB): it edits water records and water materials only.

#include "PCH.h"
#include "Menu.h"

namespace
{
	constexpr auto kIniPath = "Data/SKSE/Plugins/WaterWind.ini";

	struct Config
	{
		bool  enabled{ true };
		int   windSource{ 0 };               // 0 = weather records (steady, follows weather changes), 1 = live sky wind (includes gusts)
		float calmWind{ 0.05f };             // wind (0..1) at or below which the calm scales apply
		float referenceWind{ 0.12f };        // wind at which everything keeps the water's own values (scale 1)
		float stormWind{ 0.40f };            // wind at or above which the storm scales apply
		float precipitationBonus{ 0.10f };   // added to the wind of rainy / snowy weathers (their records have low wind values)
		float calmSpeedScale{ 0.40f };
		float stormSpeedScale{ 2.0f };
		float calmAmplitudeScale{ 0.55f };
		float stormAmplitudeScale{ 3.0f };
		float smoothingSeconds{ 8.0f };      // time constant of the smoothing applied to the wind
		bool  recordAmplitude{ false };      // also scale the water RECORD's amplitude (test: does the shader read it from there?)
		float depthNormals{ -1.0f };         // test: >= 0 forces the depth-normals factor on every water record and material
		bool  textureFix{ false };           // test: water materials with no normal textures borrow them from a loaded material that has them
		bool  driveStalled{ true };          // water materials whose scroll never moves (never set up by the engine) are scrolled here
		bool  affectFlowmapWater{ false };   // water types with Enable Flowmap (rivers) keep their values unless this is set
		int   logSeconds{ 30 };
	};

	bool Truthy(const std::string& v) { return v != "0" && v != "false" && v != "False"; }

	Config ReadConfig()
	{
		Config c;
		std::ifstream in(kIniPath);
		std::string   line;
		while (std::getline(in, line)) {
			auto eq = line.find('=');
			if (eq == std::string::npos || line[0] == ';' || line[0] == '[') {
				continue;
			}
			auto key = line.substr(0, eq);
			auto val = line.substr(eq + 1);
			auto trim = [](std::string& s) {
				s.erase(0, s.find_first_not_of(" \t\r"));
				s.erase(s.find_last_not_of(" \t\r") + 1);
			};
			trim(key);
			trim(val);
			const float f = static_cast<float>(std::atof(val.c_str()));
			if (key == "bEnabled") c.enabled = Truthy(val);
			else if (key == "iWindSource") c.windSource = std::atoi(val.c_str());
			else if (key == "fCalmWind") c.calmWind = f;
			else if (key == "fReferenceWind") c.referenceWind = f;
			else if (key == "fStormWind") c.stormWind = f;
			else if (key == "fPrecipitationWindBonus") c.precipitationBonus = f;
			else if (key == "fCalmSpeedScale") c.calmSpeedScale = f;
			else if (key == "fStormSpeedScale") c.stormSpeedScale = f;
			else if (key == "fCalmAmplitudeScale") c.calmAmplitudeScale = f;
			else if (key == "fStormAmplitudeScale") c.stormAmplitudeScale = f;
			else if (key == "fSmoothingSeconds") c.smoothingSeconds = f;
			else if (key == "bRecordAmplitude") c.recordAmplitude = Truthy(val);
			else if (key == "fDepthNormals") c.depthNormals = f;
			else if (key == "bTextureFix") c.textureFix = Truthy(val);
			else if (key == "bDriveStalledScroll") c.driveStalled = Truthy(val);
			else if (key == "bAffectFlowmapWater") c.affectFlowmapWater = Truthy(val);
			else if (key == "iLogSeconds") c.logSeconds = std::max(1, std::atoi(val.c_str()));
		}
		c.calmWind = std::clamp(c.calmWind, 0.0f, 0.9f);
		c.referenceWind = std::clamp(c.referenceWind, c.calmWind + 0.01f, 0.95f);
		c.stormWind = std::clamp(c.stormWind, c.referenceWind + 0.01f, 1.0f);
		c.precipitationBonus = std::clamp(c.precipitationBonus, 0.0f, 0.5f);
		c.calmSpeedScale = std::clamp(c.calmSpeedScale, 0.0f, 20.0f);
		c.stormSpeedScale = std::clamp(c.stormSpeedScale, 0.0f, 20.0f);
		c.calmAmplitudeScale = std::clamp(c.calmAmplitudeScale, 0.0f, 8.0f);
		c.stormAmplitudeScale = std::clamp(c.stormAmplitudeScale, 0.0f, 8.0f);
		c.smoothingSeconds = std::clamp(c.smoothingSeconds, 0.0f, 120.0f);
		return c;
	}

	struct Base
	{
		RE::TESWaterForm* form{};
		float             speed[3]{};
		float             amp[3]{};
		bool              flowmap{};
	};
	std::vector<Base> g_bases;
	std::set<const RE::TESWaterForm*> g_fixLogged;

	struct Stall
	{
		float prev[6]{};     // scroll as seen at the end of the previous tick (after our own write, if any)
		int   still{ 0 };
		bool  driven{ false };
		bool  logged{ false };
	};
	std::map<const RE::BSWaterShaderMaterial*, Stall> g_stalls;

	std::string Name(const RE::TESForm* a_form)
	{
		const auto* file = a_form ? a_form->GetFile(0) : nullptr;
		return a_form ? std::format("{:08X} [{}]", a_form->GetFormID(), file ? file->GetFilename() : "?"sv) : "none";
	}

	void CaptureBases()
	{
		g_bases.clear();
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			return;
		}
		for (auto* form : dh->GetFormArray<RE::TESWaterForm>()) {
			if (!form || form->IsDeleted()) {
				continue;
			}
			Base b;
			b.form = form;
			b.flowmap = form->flags.any(RE::TESWaterForm::Flag::kEnableFlowmap);
			bool ok = true;
			for (int i = 0; i < 3; ++i) {
				b.speed[i] = form->data.noiseWindSpeedA[i];
				b.amp[i] = form->data.amplitudeA[i];
				ok = ok && std::isfinite(b.speed[i]);
			}
			if (ok) {
				g_bases.push_back(b);
			}
		}
		const auto flow = std::count_if(g_bases.begin(), g_bases.end(), [](const Base& b) { return b.flowmap; });
		logger::info("captured the base noise wind speed of {} water types ({} with Enable Flowmap)", g_bases.size(), flow);
	}

	float WeatherWind(const RE::TESWeather* a_weather, float a_precipitationBonus)
	{
		float v = a_weather->data.windSpeed / 255.0f;
		if (a_weather->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy, RE::TESWeather::WeatherDataFlag::kSnow)) {
			v += a_precipitationBonus;
		}
		return v;
	}

	// 0..1 wind from the weather records (blended over a weather change) or the live sky value
	float CurrentWind(const RE::Sky& a_sky, const Config& a_cfg)
	{
		const auto* cur = a_sky.currentWeather;
		if (a_cfg.windSource == 1 || !cur) {
			float v = a_sky.windSpeed;
			if (cur && cur->data.flags.any(RE::TESWeather::WeatherDataFlag::kRainy, RE::TESWeather::WeatherDataFlag::kSnow)) {
				v += a_cfg.precipitationBonus;
			}
			return std::clamp(v, 0.0f, 1.0f);
		}
		const float curWind = WeatherWind(cur, a_cfg.precipitationBonus);
		const auto* last = a_sky.lastWeather;
		const float lastWind = last ? WeatherWind(last, a_cfg.precipitationBonus) : curWind;
		const float pct = std::clamp(a_sky.currentWeatherPct, 0.0f, 1.0f);
		return std::clamp(lastWind + (curWind - lastWind) * pct, 0.0f, 1.0f);
	}

	float Smoothstep(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// 1 at the reference wind, calmScale in no wind, stormScale in a gale
	float Curve(float a_wind, const Config& c, float a_calmScale, float a_stormScale)
	{
		if (a_wind <= c.referenceWind) {
			return a_calmScale + (1.0f - a_calmScale) * Smoothstep((a_wind - c.calmWind) / (c.referenceWind - c.calmWind));
		}
		return 1.0f + (a_stormScale - 1.0f) * Smoothstep((a_wind - c.referenceWind) / (c.stormWind - c.referenceWind));
	}

	// the displacement patch the engine draws around an actor wading in water has its own geometry and water material
	void CollectWaterMaterials(RE::NiAVObject* a_obj, std::vector<RE::BSWaterShaderMaterial*>& a_out, int a_depth = 0)
	{
		if (!a_obj || a_depth > 6) {
			return;
		}
		if (auto* geom = netimmerse_cast<RE::BSGeometry*>(a_obj)) {
			auto* prop = netimmerse_cast<RE::BSWaterShaderProperty*>(geom->GetGeometryRuntimeData().shaderProperty.get());
			if (prop && prop->material && prop->material->GetType() == RE::BSShaderMaterial::Type::kWater) {
				a_out.push_back(static_cast<RE::BSWaterShaderMaterial*>(prop->material));
			}
			return;
		}
		if (auto* node = netimmerse_cast<RE::NiNode*>(a_obj)) {
			for (auto& child : node->GetChildren()) {
				CollectWaterMaterials(child.get(), a_out, a_depth + 1);
			}
		}
	}

	// diagnostic: find the LOD water (terrain LOD water shapes) and log whether the engine scrolls its material
	void ScanLodWater(RE::NiAVObject* a_obj, const char* a_root, int a_depth, int& a_found, const RE::TESWaterSystem& a_ws)
	{
		if (!a_obj || a_depth > 10 || a_found > 24) {
			return;
		}
		if (auto* geom = netimmerse_cast<RE::BSGeometry*>(a_obj)) {
			auto* prop = netimmerse_cast<RE::BSWaterShaderProperty*>(geom->GetGeometryRuntimeData().shaderProperty.get());
			if (!prop || !prop->material || prop->material->GetType() != RE::BSShaderMaterial::Type::kWater) {
				return;
			}
			auto*                     m = static_cast<RE::BSWaterShaderMaterial*>(prop->material);
			static std::map<const void*, RE::NiPoint2> prev;
			bool                                       isObject = false;
			for (auto& o : a_ws.waterObjects) {
				isObject = isObject || (o && o->shape.get() == a_obj);
			}
			auto&      p = prev[m];
			const bool moving = std::fabs(m->normalScroll1.x - p.x) > 1e-7f || std::fabs(m->normalScroll1.y - p.y) > 1e-7f;
			p = m->normalScroll1;
			++a_found;
			logger::info("LODSCAN [{}] depth {} shape '{}' material {:p} inWaterObjects={} tex1={} tex2={} scroll1=({:.4f},{:.4f}) moving={} amp=({:.2f},{:.2f},{:.2f}) uvScale=({:.0f},{:.0f},{:.0f}) propFlags={:#x} center=({:.0f},{:.0f},{:.0f}) radius={:.0f} distToPlayer={:.0f}",
				a_root, a_depth, a_obj->name.c_str(), static_cast<const void*>(m), isObject, m->normalTexture1 ? m->normalTexture1->name.c_str() : "NULL",
				m->normalTexture2 ? m->normalTexture2->name.c_str() : "NULL", m->normalScroll1.x, m->normalScroll1.y, moving, m->amplitudeA[0], m->amplitudeA[1], m->amplitudeA[2],
				m->uvScaleA[0], m->uvScaleA[1], m->uvScaleA[2], prop->flags.underlying(), geom->worldBound.center.x, geom->worldBound.center.y, geom->worldBound.center.z, geom->worldBound.radius,
				[&]() {
					const auto* pl = RE::PlayerCharacter::GetSingleton();
					if (!pl) {
						return 0.0f;
					}
					const auto p = pl->GetPosition();
					const float dx = geom->worldBound.center.x - p.x, dy = geom->worldBound.center.y - p.y;
					return std::sqrt(dx * dx + dy * dy);
				}());
			return;
		}
		if (auto* node = netimmerse_cast<RE::NiNode*>(a_obj)) {
			for (auto& child : node->GetChildren()) {
				ScanLodWater(child.get(), a_root, a_depth + 1, a_found, a_ws);
			}
		}
	}

	std::atomic<bool> g_pending{ false };

	void Update()
	{
		struct Reset
		{
			~Reset() { g_pending = false; }
		} reset;

		static auto   last = std::chrono::steady_clock::now();
		static float  smoothed = -1.0f;
		static double sinceLog = 0.0;
		const auto    nowTime = std::chrono::steady_clock::now();
		const float   dt = std::chrono::duration<float>(nowTime - last).count();
		last = nowTime;

		const auto cfg = ReadConfig();
		auto*      sky = RE::Sky::GetSingleton();
		auto*      ws = RE::TESWaterSystem::GetSingleton();
		auto*      player = RE::PlayerCharacter::GetSingleton();
		{
			static bool  skipping = false;
			static auto  skipSince = nowTime;
			const bool   paused = RE::UI::GetSingleton() && RE::UI::GetSingleton()->GameIsPaused();
			const bool   skip = g_bases.empty() || !sky || !ws || !player || !player->Is3DLoaded() || paused;
			if (skip && !skipping) {
				skipping = true;
				skipSince = nowTime;
				std::string menus;
				if (auto* ui = RE::UI::GetSingleton(); ui && paused) {
					for (auto& [name, entry] : ui->menuMap) {
						if (entry.menu && ui->IsMenuOpen(name)) {
							menus += std::string(name.c_str()) + " ";
						}
					}
				}
				logger::info("ticks paused: bases={} sky={} waterSystem={} player={} 3dLoaded={} gamePaused={} openMenus=[{}]", !g_bases.empty(), sky != nullptr, ws != nullptr,
					player != nullptr, player && player->Is3DLoaded(), paused, menus);
			} else if (!skip && skipping) {
				skipping = false;
				const float secs = std::chrono::duration<float>(nowTime - skipSince).count();
				if (secs >= 0.5f) {
					logger::info("ticks resumed after {:.1f}s without updates", secs);
				}
			}
			if (skip) {
				return;
			}
		}

		std::vector<std::pair<RE::BSWaterShaderMaterial*, const RE::TESWaterForm*>> extras;
		{
			static std::set<const void*> seen;
			auto add = [&](RE::NiAVObject* a_geom, const RE::TESWaterForm* a_type, const char* a_where) {
				if (!a_geom || !a_type) {
					return;
				}
				std::vector<RE::BSWaterShaderMaterial*> found;
				CollectWaterMaterials(a_geom, found);
				for (auto* m : found) {
					bool dup = false;
					for (auto& e : extras) {
						dup = dup || e.first == m;
					}
					if (dup) {
						continue;
					}
					extras.emplace_back(m, a_type);
					if (seen.insert(m).second && seen.size() < 40) {
						logger::info("displacement water material {:p} ({}, {}): normalTexture1={} scroll1=({:.3f},{:.3f})", static_cast<const void*>(m), Name(a_type), a_where,
							m->normalTexture1 ? m->normalTexture1->name.c_str() : "NULL", m->normalScroll1.x, m->normalScroll1.y);
					}
				}
			};
			for (auto& d : ws->waterDisplacement) {
				if (d) {
					add(d->displacementGeometry.get(), d->waterType, "displacement list");
				}
			}
			for (auto& o : ws->waterObjects) {
				if (o && o->displacement) {
					add(o->displacement->displacementGeometry.get(), o->displacement->waterType, "object displacement");
				}
			}
			if (ws->defaultDisplacementWaterObject && ws->currentWaterType) {
				add(ws->defaultDisplacementWaterObject.get(), ws->currentWaterType, "default displacement object");
				}
				// The terrain LOD water (the world's LodWater type). Its shapes have no ZBufferTest flag, so the per-object loops below treat them
				// like dummies and never repair or drive them: one shape came up with no textures and a scroll stuck at 0 (LODSCAN, 2026-10-20).
				if (auto* tes = RE::TES::GetSingleton(); tes && tes->objLODWaterRoot) {
					if (const auto* wsp = player->GetWorldspace(); wsp && wsp->lodWater) {
						add(tes->objLODWaterRoot, wsp->lodWater, "LOD water");
					}
			}
		}

		{
			static double sinceScan = 0.0;
			static int    scans = 0;
			sinceScan += dt;
			if (sinceScan >= (scans < 20 ? 3.0 : 20.0) && scans < 80) {
				sinceScan = 0.0;
				++scans;
				int  found = 0;
				auto* tes = RE::TES::GetSingleton();
				logger::info("LODSCAN pass {}: waterObjects={} objLODWaterRoot={} lodLandRoot={} waterRoot={}", scans, ws->waterObjects.size(), tes && tes->objLODWaterRoot != nullptr,
					tes && tes->lodLandRoot != nullptr, ws->waterRoot != nullptr);
				if (tes) {
					ScanLodWater(tes->objLODWaterRoot, "objLODWaterRoot", 0, found, *ws);
					ScanLodWater(tes->lodLandRoot, "lodLandRoot", 0, found, *ws);
				}
				ScanLodWater(ws->waterRoot.get(), "waterRoot", 0, found, *ws);
				logger::info("LODSCAN pass {}: {} water shapes found", scans, found);
			}
		}

		float speedScale = 1.0f;
		float ampScale = 1.0f;
		float wind = 0.0f;
		auto* cell = player->GetParentCell();
		const bool exterior = cell && !cell->IsInteriorCell();
		if (cfg.enabled && exterior) {
			const float target = CurrentWind(*sky, cfg);
			if (smoothed < 0.0f || cfg.smoothingSeconds <= 0.0f) {
				smoothed = target;
			} else {
				smoothed += (target - smoothed) * (1.0f - std::exp(-std::clamp(dt, 0.0f, 5.0f) / cfg.smoothingSeconds));
			}
			wind = smoothed;
			speedScale = Curve(wind, cfg, cfg.calmSpeedScale, cfg.stormSpeedScale);
			ampScale = Curve(wind, cfg, cfg.calmAmplitudeScale, cfg.stormAmplitudeScale);
		}

		// scroll speed: the record, which the engine copies into the material every frame
		for (auto& b : g_bases) {
			const bool skip = b.flowmap && !cfg.affectFlowmapWater;
			for (int i = 0; i < 3; ++i) {
				b.form->data.noiseWindSpeedA[i] = skip ? b.speed[i] : b.speed[i] * speedScale;
			}
		}

		if (cfg.depthNormals >= 0.0f) {
			for (auto& b : g_bases) {
				b.form->data.depthProperties.normals = cfg.depthNormals;
			}
		}
		if (cfg.recordAmplitude) {
			for (auto& b : g_bases) {
				const bool skip = b.flowmap && !cfg.affectFlowmapWater;
				for (int i = 0; i < 3; ++i) {
					b.form->data.amplitudeA[i] = skip ? b.amp[i] : b.amp[i] * ampScale;
				}
			}
		}

		if (cfg.textureFix) {
			// a water object can come up with a material that has no normal textures (flat water). Give it the textures of another live
			// material of the SAME water type, so every type keeps the textures its own record names.
			std::map<const RE::TESWaterForm*, RE::BSWaterShaderMaterial*> donors;
			auto materialOf = [](RE::TESWaterObject& a_obj) -> RE::BSWaterShaderMaterial* {
				if (!a_obj.shape) {
					return nullptr;
				}
				auto* prop = netimmerse_cast<RE::BSWaterShaderProperty*>(a_obj.shape->GetGeometryRuntimeData().shaderProperty.get());
				if (!prop || !prop->material || prop->material->GetType() != RE::BSShaderMaterial::Type::kWater) {
					return nullptr;
				}
				return static_cast<RE::BSWaterShaderMaterial*>(prop->material);
			};
			for (auto& obj : ws->waterObjects) {
				auto* m = obj && obj->waterType ? materialOf(*obj) : nullptr;
				if (m && m->normalTexture1 && m->normalTexture2 && m->normalTexture3) {
					donors.emplace(obj->waterType, m);
				}
			}
			for (auto& obj : ws->waterObjects) {
				auto* m = obj && obj->waterType ? materialOf(*obj) : nullptr;
				if (!m || m->normalTexture1) {
					continue;
				}
				const auto it = donors.find(obj->waterType);
				if (it == donors.end()) {
					continue;
				}
				m->normalTexture1 = it->second->normalTexture1;
				m->normalTexture2 = it->second->normalTexture2;
				m->normalTexture3 = it->second->normalTexture3;
				if (!m->normalTexture4) {
					m->normalTexture4 = it->second->normalTexture4;
				}
				if (g_fixLogged.insert(obj->waterType).second) {
					logger::info("texture fix: {} had a material without normal textures, gave it '{}'", Name(obj->waterType), it->second->normalTexture1->name.c_str());
				}
			}
			for (auto& [m, type] : extras) {
				if (m->normalTexture1) {
					continue;
				}
				const auto it = donors.find(type);
				if (it == donors.end()) {
					continue;
				}
				m->normalTexture1 = it->second->normalTexture1;
				m->normalTexture2 = it->second->normalTexture2;
				m->normalTexture3 = it->second->normalTexture3;
				if (!m->normalTexture4) {
					m->normalTexture4 = it->second->normalTexture4;
				}
				if (g_fixLogged.insert(type).second) {
					logger::info("texture fix: {} displacement material had no normal textures, gave it '{}'", Name(type), it->second->normalTexture1->name.c_str());
				}
			}
		}

		if (cfg.driveStalled) {
			// The engine advances the scroll of the materials it has set up (RATE/s == record speed * (sin dir, cos dir)). A material it never
			// set up keeps scroll 0 forever: flat, frozen water. Detect that (no change for 3 ticks) and advance it here the same way.
			std::set<const RE::BSWaterShaderMaterial*> alive;
			auto driveOne = [&](RE::BSWaterShaderMaterial* m, const RE::TESWaterForm* wt) {
				if (!alive.insert(m).second) {
					return;
				}
			RE::NiPoint2* sc[3] = { &m->normalScroll1, &m->normalScroll2, &m->normalScroll3 };
			auto&         st = g_stalls[m];
			// The engine moves the scroll of a material it has set up every frame, so a tick that sees the scroll exactly where this
			// code left it (or where it was the tick before) means nobody else is moving it: drive it. Decided every tick, so a material the
			// engine takes over (or hands back, e.g. the cell the player stands in) is never advanced twice or left frozen.
			bool moved = false;
			for (int i = 0; i < 3; ++i) {
				moved = moved || std::fabs(sc[i]->x - st.prev[i * 2]) > 1e-7f || std::fabs(sc[i]->y - st.prev[i * 2 + 1]) > 1e-7f;
			}
			{
				static int handoffLogs = 0;
				if (st.driven == moved && st.logged && handoffLogs < 60) {
					++handoffLogs;
					logger::info("water material {:p} ({}): {}", static_cast<const void*>(m), Name(wt), moved ? "engine is moving it again, no longer driven here" : "engine stopped moving it, driving it here");
				}
			}
			st.driven = !moved;
			if (st.driven) {
				if (!st.logged) {
					st.logged = true;
					logger::info("stalled water scroll: {} material {:p} is not scrolling; driving it from the record", Name(wt), static_cast<const void*>(m));
				}
				const float step = std::min(dt, 0.25f);
				for (int i = 0; i < 3; ++i) {
					const float rad = wt->data.noiseWindDirectionA[i] * 0.017453292f;
					const float speed = wt->data.noiseWindSpeedA[i];
					sc[i]->x = std::fmod(sc[i]->x + step * speed * std::sin(rad) + 1.0f, 1.0f);
					sc[i]->y = std::fmod(sc[i]->y + step * speed * std::cos(rad) + 1.0f, 1.0f);
				}
			}
			for (int i = 0; i < 3; ++i) {
				st.prev[i * 2] = sc[i]->x;
				st.prev[i * 2 + 1] = sc[i]->y;
			}
			};
			for (auto& obj : ws->waterObjects) {
				if (!obj || !obj->waterType || !obj->shape) {
					continue;
				}
				auto* prop = netimmerse_cast<RE::BSWaterShaderProperty*>(obj->shape->GetGeometryRuntimeData().shaderProperty.get());
				if (!prop || !prop->material || prop->material->GetType() != RE::BSShaderMaterial::Type::kWater || prop->flags.none(RE::BSShaderProperty::EShaderPropertyFlag::kZBufferTest)) {
					continue;
				}
				driveOne(static_cast<RE::BSWaterShaderMaterial*>(prop->material), obj->waterType);
			}
			for (auto& [m, type] : extras) {
				driveOne(m, type);
			}
			for (auto it = g_stalls.begin(); it != g_stalls.end();) {
				it = alive.contains(it->first) ? std::next(it) : g_stalls.erase(it);
			}
		}

		// wave height: the live material of each active water object (the engine does not rewrite it); the record keeps the original
		std::size_t materials = 0;
		for (auto& obj : ws->waterObjects) {
			if (!obj || !obj->waterType || !obj->shape) {
				continue;
			}
			const auto* type = obj->waterType;
			if (type->flags.any(RE::TESWaterForm::Flag::kEnableFlowmap) && !cfg.affectFlowmapWater) {
				continue;
			}
			auto* prop = netimmerse_cast<RE::BSWaterShaderProperty*>(obj->shape->GetGeometryRuntimeData().shaderProperty.get());
			if (!prop || !prop->material || prop->material->GetType() != RE::BSShaderMaterial::Type::kWater) {
				continue;
			}
			auto* mat = static_cast<RE::BSWaterShaderMaterial*>(prop->material);
			for (int i = 0; i < 3; ++i) {
				mat->amplitudeA[i] = cfg.recordAmplitude ? type->data.amplitudeA[i] : type->data.amplitudeA[i] * ampScale;
			}
			if (cfg.depthNormals >= 0.0f) {
				mat->depthProperties.normals = cfg.depthNormals;
			}
			++materials;
		}

		for (auto& [mat, type] : extras) {
			if (type->flags.any(RE::TESWaterForm::Flag::kEnableFlowmap) && !cfg.affectFlowmapWater) {
				continue;
			}
			for (int i = 0; i < 3; ++i) {
				mat->amplitudeA[i] = cfg.recordAmplitude ? type->data.amplitudeA[i] : type->data.amplitudeA[i] * ampScale;
			}
			if (cfg.depthNormals >= 0.0f) {
				mat->depthProperties.normals = cfg.depthNormals;
			}
			++materials;
		}

		sinceLog += dt;
		if (sinceLog >= cfg.logSeconds) {
			sinceLog = 0.0;
			logger::info("wind={:.3f} (source {}, weather {}) -> speed x{:.2f}, wave height x{:.2f} on {} water types / {} water materials{}", wind,
				cfg.windSource, Name(sky->currentWeather), speedScale, ampScale, g_bases.size(), materials, cfg.enabled ? "" : " [DISABLED in ini: values at base]");
		}
	}

	void SetupLog()
	{
		auto folder = logger::log_directory();
		if (!folder) {
			return;
		}
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*folder / "WaterWind.log").string(), true);
		auto log = std::make_shared<spdlog::logger>("log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			logger::info("WaterWind 1.0.0 on game {}", REL::Module::get().version().string());
			WaterWindMenu::Register();
			CaptureBases();
			std::thread([]() {
				for (;;) {
					std::this_thread::sleep_for(100ms);
					if (!g_pending.exchange(true)) {
						SKSE::GetTaskInterface()->AddTask([]() { Update(); });
					}
				}
			}).detach();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();
	logger::info("WaterWind loaded");
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
