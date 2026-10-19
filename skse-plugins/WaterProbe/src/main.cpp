// WaterProbe: a read-mostly diagnostic for how Skyrim drives water.
//
// Questions it answers (see WaterProbe.log, one block per second):
//   1. Does the weather's wind change anything about the water material (scroll rate, amplitude)?
//   2. Are the material fields (BSWaterShaderMaterial) recomputed every frame, or do they keep what we write?
//   3. Does editing the water record (TESWaterForm data + needUpdate) reach the material?
//
// Everything is logged from the game thread. The optional experiments (iExperiment in WaterProbe.ini, re-read every second)
// touch only the water type the player is currently in and are undone when iExperiment goes back to 0.

#include "PCH.h"

namespace
{
	constexpr auto kIniPath = "Data/SKSE/Plugins/WaterProbe.ini";

	struct Config
	{
		int  experiment{ 0 };       // 0 = log only, 1 = write the material directly, 2 = write the water record and set needUpdate
		bool allTypes{ true };      // log every water type with an active water object, not only the current one
		int  maxTypes{ 10 };
	};

	Config ReadConfig()
	{
		Config cfg;
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
			if (key == "iExperiment") {
				cfg.experiment = std::atoi(val.c_str());
			} else if (key == "bAllTypes") {
				cfg.allTypes = val != "0";
			} else if (key == "iMaxTypes") {
				cfg.maxTypes = std::max(1, std::atoi(val.c_str()));
			}
		}
		return cfg;
	}

	std::string Name(const RE::TESForm* a_form)
	{
		if (!a_form) {
			return "none";
		}
		const char* edid = a_form->GetFormEditorID();
		const auto* file = a_form->GetFile(0);
		return std::format("{:08X} {} [{}]", a_form->GetFormID(), (edid && *edid) ? edid : "?", file ? file->GetFilename() : "?"sv);
	}

	struct Previous
	{
		double       t{ -1.0 };
		RE::NiPoint2 scroll[3]{};
	};
	std::map<const RE::TESWaterForm*, Previous> g_prev;

	// experiment bookkeeping
	struct Saved
	{
		int                 mode{ 0 };
		RE::TESWaterForm*   form{ nullptr };
		RE::BSWaterShaderMaterial* material{ nullptr };
		float               amplitude[3]{};
		float               windSpeed[3]{};
	} g_saved;

	// diff of two scroll offsets that also copes with the offset wrapping around 1.0
	float Wrap(float d)
	{
		if (d > 0.5f) {
			d -= 1.0f;
		} else if (d < -0.5f) {
			d += 1.0f;
		}
		return d;
	}

	void RestoreExperiment()
	{
		if (g_saved.mode == 0 || !g_saved.form) {
			g_saved = {};
			return;
		}
		auto* form = g_saved.form;
		if (g_saved.mode == 1) {
			if (auto* mat = g_saved.material ? g_saved.material : form->waterShaderMaterial) {
				for (int i = 0; i < 3; ++i) {
					mat->amplitudeA[i] = g_saved.amplitude[i];
				}
			}
			logger::info("EXPERIMENT 1 undone for {}", Name(form));
		} else {
			for (int i = 0; i < 3; ++i) {
				form->data.noiseWindSpeedA[i] = g_saved.windSpeed[i];
			}
			form->needUpdate = true;
			logger::info("EXPERIMENT 2 undone for {}", Name(form));
		}
		g_saved = {};
	}

	void ApplyExperiment(const Config& a_cfg, RE::TESWaterForm* a_current, RE::BSWaterShaderMaterial* a_liveMaterial)
	{
		if (a_cfg.experiment == 0 || !a_current) {
			if (g_saved.mode != 0) {
				RestoreExperiment();
			}
			return;
		}
		if (g_saved.mode != a_cfg.experiment || g_saved.form != a_current) {
			RestoreExperiment();
			g_saved.mode = a_cfg.experiment;
			g_saved.form = a_current;
			if (a_cfg.experiment == 1) {
				auto* mat = a_liveMaterial ? a_liveMaterial : a_current->waterShaderMaterial;
				if (!mat) {
					logger::info("EXPERIMENT 1 skipped: {} has no material yet", Name(a_current));
					g_saved = {};
					return;
				}
				g_saved.material = mat;
				for (int i = 0; i < 3; ++i) {
					g_saved.amplitude[i] = mat->amplitudeA[i];
					mat->amplitudeA[i] *= 3.0f;
				}
				logger::info("EXPERIMENT 1: material amplitudeA of {} x3 ({:.3f},{:.3f},{:.3f}) -> ({:.3f},{:.3f},{:.3f})", Name(a_current),
					g_saved.amplitude[0], g_saved.amplitude[1], g_saved.amplitude[2], mat->amplitudeA[0], mat->amplitudeA[1], mat->amplitudeA[2]);
			} else {
				for (int i = 0; i < 3; ++i) {
					g_saved.windSpeed[i] = a_current->data.noiseWindSpeedA[i];
					a_current->data.noiseWindSpeedA[i] *= 5.0f;
				}
				a_current->needUpdate = true;
				logger::info("EXPERIMENT 2: record noiseWindSpeedA of {} x5 ({:.3f},{:.3f},{:.3f}) -> ({:.3f},{:.3f},{:.3f}), needUpdate set", Name(a_current),
					g_saved.windSpeed[0], g_saved.windSpeed[1], g_saved.windSpeed[2], a_current->data.noiseWindSpeedA[0], a_current->data.noiseWindSpeedA[1], a_current->data.noiseWindSpeedA[2]);
			}
		}
	}

	void LogType(const RE::TESWaterForm* a_form, const RE::BSWaterShaderMaterial* a_liveMaterial, double a_now, bool a_current)
	{
		const auto& d = a_form->data;
		logger::info("  WATER {}{}  flags={:#x} needUpdate={} alpha={}", Name(a_form), a_current ? "  <-- player" : "", a_form->flags.underlying(), a_form->needUpdate, a_form->alpha);
		logger::info("    record: windSpeed=({:.4f},{:.4f},{:.4f}) windDir=({:.1f},{:.1f},{:.1f}) uvScale=({:.0f},{:.0f},{:.0f}) amplitude=({:.3f},{:.3f},{:.3f}) noiseFalloff={:.1f} flowmapScale={:.3f}",
			d.noiseWindSpeedA[0], d.noiseWindSpeedA[1], d.noiseWindSpeedA[2], d.noiseWindDirectionA[0], d.noiseWindDirectionA[1], d.noiseWindDirectionA[2],
			d.uvScaleA[0], d.uvScaleA[1], d.uvScaleA[2], d.amplitudeA[0], d.amplitudeA[1], d.amplitudeA[2], d.noiseFalloff, d.flowmapScale);
		logger::info("    record: texScroll=({:.4f},{:.4f}) ({:.4f},{:.4f}) ({:.4f},{:.4f}) linVel=({:.2f},{:.2f},{:.2f}) angVel=({:.2f},{:.2f},{:.2f})",
			a_form->texScroll[0].red, a_form->texScroll[0].green, a_form->texScroll[1].red, a_form->texScroll[1].green, a_form->texScroll[2].red, a_form->texScroll[2].green,
			a_form->linearVelocity.x, a_form->linearVelocity.y, a_form->linearVelocity.z, a_form->angularVelocity.x, a_form->angularVelocity.y, a_form->angularVelocity.z);

		const auto* mat = a_liveMaterial ? a_liveMaterial : a_form->waterShaderMaterial;
		if (!mat) {
			logger::info("    material: none (no water object with a material, record pointer empty)");
			return;
		}
		logger::info("    material source: {}", a_liveMaterial ? "water object -> shape -> shader property" : "water record pointer");
		auto& prev = g_prev[a_form];
		const double dt = prev.t >= 0.0 ? a_now - prev.t : 0.0;
		const RE::NiPoint2 s[3] = { mat->normalScroll1, mat->normalScroll2, mat->normalScroll3 };
		float rate[3][2]{};
		if (dt > 0.0) {
			for (int i = 0; i < 3; ++i) {
				rate[i][0] = static_cast<float>(Wrap(s[i].x - prev.scroll[i].x) / dt);
				rate[i][1] = static_cast<float>(Wrap(s[i].y - prev.scroll[i].y) / dt);
			}
		}
		for (int i = 0; i < 3; ++i) {
			prev.scroll[i] = s[i];
		}
		prev.t = a_now;
		logger::info("    material: scroll=({:.4f},{:.4f}) ({:.4f},{:.4f}) ({:.4f},{:.4f})  RATE/s=({:+.5f},{:+.5f}) ({:+.5f},{:+.5f}) ({:+.5f},{:+.5f})",
			s[0].x, s[0].y, s[1].x, s[1].y, s[2].x, s[2].y, rate[0][0], rate[0][1], rate[1][0], rate[1][1], rate[2][0], rate[2][1]);
		logger::info("    material: uvScale=({:.0f},{:.0f},{:.0f}) amplitude=({:.3f},{:.3f},{:.3f}) noiseFalloff={:.1f} flowmapScale={:.3f} reflMag={:.3f} refrMag={:.3f} alpha={:.3f}",
			mat->uvScaleA[0], mat->uvScaleA[1], mat->uvScaleA[2], mat->amplitudeA[0], mat->amplitudeA[1], mat->amplitudeA[2], mat->noiseFalloff, mat->flowmapScale,
			mat->reflectionMagnitude, mat->refractionMagnitude, mat->alpha);
		const RE::NiPointer<RE::NiSourceTexture>* tex[4] = { &mat->normalTexture1, &mat->normalTexture2, &mat->normalTexture3, &mat->normalTexture4 };
		for (int i = 0; i < 4; ++i) {
			const auto* t = tex[i]->get();
			const auto* r = t ? t->rendererTexture : nullptr;
			logger::info("    material: normalTexture{} = {:p} name='{}' renderer={:p} {}x{} mips={} fmt={}", i + 1, static_cast<const void*>(t), t ? t->name.c_str() : "", static_cast<const void*>(r),
				r ? r->width : 0, r ? r->height : 0, r ? r->mips : 0, r ? r->format : 0);
		}
		logger::info("    material: sparkle={:.2f} sparklePower={:.0f} sunSpecPower={:.0f} reflAmount={:.3f} fresnel={:.3f} specPower={:.0f} depth(refl,refr,norm,spec)=({:.2f},{:.2f},{:.2f},{:.2f})",
			mat->sunSparkleMagnitude, mat->sunSparklePower, mat->sunSpecularPower, mat->reflectionAmount, mat->fresnelAmount, mat->specularPower, mat->depthProperties.reflections,
			mat->depthProperties.refraction, mat->depthProperties.normals, mat->depthProperties.specularLighting);
	}

	// the water objects closest to the player: which water type each one is, whether the player's position lies inside it
	void LogNearbyObjects(RE::TESWaterSystem& a_ws, const RE::NiPoint3& a_pos)
	{
		struct Near
		{
			const RE::TESWaterObject* obj;
			float                     dist;
			bool                      inside;
		};
		std::vector<Near> found;
		for (auto& obj : a_ws.waterObjects) {
			if (!obj || !obj->waterType) {
				continue;
			}
			bool  inside = false;
			float best = 1.0e9f;
			for (auto& box : obj->multiBounds) {
				if (!box) {
					continue;
				}
				const float dx = std::max(0.0f, std::fabs(a_pos.x - box->center.x) - box->size.x);
				const float dy = std::max(0.0f, std::fabs(a_pos.y - box->center.y) - box->size.y);
				best = std::min(best, std::hypot(dx, dy));
				inside = inside || (dx == 0.0f && dy == 0.0f);
			}
			if (obj->multiBounds.empty() && obj->shape) {
				const auto& b = obj->shape->worldBound;
				best = std::max(0.0f, std::hypot(a_pos.x - b.center.x, a_pos.y - b.center.y) - b.radius);
				inside = best == 0.0f;
			}
			found.push_back({ obj.get(), best, inside });
		}
		std::sort(found.begin(), found.end(), [](const Near& a, const Near& b) { return a.dist < b.dist; });
		logger::info("  NEARBY water objects (closest first, XY distance from the player to the object's bounds):");
		for (std::size_t i = 0; i < found.size() && i < 6; ++i) {
			const auto* o = found[i].obj;
			const auto* t = o->waterType;
			const auto* prop = o->shape ? netimmerse_cast<RE::BSWaterShaderProperty*>(o->shape->GetGeometryRuntimeData().shaderProperty.get()) : nullptr;
			const auto* wm = prop && prop->material && prop->material->GetType() == RE::BSShaderMaterial::Type::kWater ? static_cast<const RE::BSWaterShaderMaterial*>(prop->material) : nullptr;
			logger::info("    #{} {}  type flags={:#x}  inside={}  dist={:.0f}  plane z={:.0f}  boxes={}  material={:p}  property flags={:#x} waterFlags={:#x} cell=({},{}) flow=({},{}) scroll1=({:.4f},{:.4f})", i, Name(t), t->flags.underlying(),
				found[i].inside, found[i].dist, o->plane.constant, o->multiBounds.size(), static_cast<const void*>(prop ? prop->material : nullptr),
				prop ? static_cast<std::uint32_t>(prop->flags.underlying()) : 0u,
				prop ? prop->waterFlags.underlying() : 0u, prop ? prop->cellX : 0, prop ? prop->cellY : 0, prop ? prop->flowX : 0, prop ? prop->flowY : 0,
				wm ? wm->normalScroll1.x : -1.0f, wm ? wm->normalScroll1.y : -1.0f);
		}
	}

	void Sample()
	{
		static const auto t0 = std::chrono::steady_clock::now();
		static double     lastSample = -10.0;
		const double      now = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		if (now - lastSample < 0.8) {  // tasks queued during loading screens arrive in bursts; keep one per second
			return;
		}
		lastSample = now;

		auto* ws = RE::TESWaterSystem::GetSingleton();
		auto* sky = RE::Sky::GetSingleton();
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!ws || !sky || !player || !player->Is3DLoaded()) {
			return;
		}
		const auto cfg = ReadConfig();

		auto* weather = sky->currentWeather;
		auto* cell = player->GetParentCell();
		const auto pos = player->GetPosition();
		logger::info("t={:.1f}s  WIND speed={:.4f} angle={:.1f}  weather={} (record wind {}) pct={:.2f}  cell={}  pos=({:.0f},{:.0f}) objects={} experiment={}",
			now, sky->windSpeed, sky->windAngle, Name(weather), weather ? weather->data.windSpeed : 0, sky->currentWeatherPct, Name(cell), pos.x, pos.y,
			ws->waterObjects.size(), cfg.experiment);

		auto* current = ws->currentWaterType;

		// the live material of each water type: water object -> its shape -> shader property -> material
		std::map<const RE::TESWaterForm*, RE::BSWaterShaderMaterial*> liveMaterial;
		for (auto& obj : ws->waterObjects) {
			if (!obj || !obj->waterType || !obj->shape || liveMaterial.contains(obj->waterType)) {
				continue;
			}
			auto* prop = netimmerse_cast<RE::BSWaterShaderProperty*>(obj->shape->GetGeometryRuntimeData().shaderProperty.get());
			if (prop && prop->material && prop->material->GetType() == RE::BSShaderMaterial::Type::kWater) {
				liveMaterial[obj->waterType] = static_cast<RE::BSWaterShaderMaterial*>(prop->material);
			}
		}
		ApplyExperiment(cfg, current, liveMaterial.contains(current) ? liveMaterial[current] : nullptr);

		std::vector<const RE::TESWaterForm*> types;
		if (current) {
			types.push_back(current);
		}
		if (cfg.allTypes) {
			for (auto& obj : ws->waterObjects) {
				if (obj && obj->waterType && std::find(types.begin(), types.end(), obj->waterType) == types.end() && static_cast<int>(types.size()) < cfg.maxTypes) {
					types.push_back(obj->waterType);
				}
			}
		}
		for (auto* form : types) {
			auto it = liveMaterial.find(form);
			LogType(form, it != liveMaterial.end() ? it->second : nullptr, now, form == current);
		}
		LogNearbyObjects(*ws, pos);
	}

	void SetupLog()
	{
		auto folder = logger::log_directory();
		if (!folder) {
			return;
		}
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*folder / "WaterProbe.log").string(), true);
		auto log = std::make_shared<spdlog::logger>("log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			logger::info("WaterProbe running on game {}. Writing a sample block every second once a game is loaded.", REL::Module::get().version().string());
			std::thread([]() {
				for (;;) {
					std::this_thread::sleep_for(1s);
					SKSE::GetTaskInterface()->AddTask([]() { Sample(); });
				}
			}).detach();
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();
	logger::info("WaterProbe loaded");
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
