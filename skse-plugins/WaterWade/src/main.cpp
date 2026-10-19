// WaterWade: actors wading or swimming are slowed by how deep they are in the water (armor adds to the swim penalty), and the
// splash rings around the player grow with how fast the player moves through the water.
#include "PCH.h"

namespace
{
	constexpr auto kIniPath = "Data/SKSE/Plugins/WaterWade.ini";

	struct Config
	{
		bool  slowEnabled{ true };
		bool  playerOnly{ false };
		float smallSlowdown{ 8.0f };      // percent at ankle depth (submerged level 0.2)
		float largeSlowdown{ 25.0f };     // percent at about waist depth (submerged level 0.55)
		float swimSlowdown{ 10.0f };      // percent while swimming (submerged level >= swimLevel), before armor
		float swimLevel{ 0.69f };
		float minSpeed{ 0.10f };          // the multiplier never goes below this
		float armorWadeFactor{ 0.0f };    // share of the swim armor penalty that also applies while wading (0 = swimming only)
		float heavy[4]{ 0.14f, 0.07f, 0.05f, 0.02f };   // body, feet, hands, head
		float light[4]{ 0.07f, 0.03f, 0.02f, 0.01f };
		float cloth[4]{ 0.023f, 0.01f, 0.007f, 0.003f };

		bool  wakeEnabled{ false };       // extra rain-style ring stamps (AddRipple): they come out as long streaks, off by default
		bool  quadEnabled{ true };        // grow the engine's own wading wake patch (fWadingWaterQuadSize) with the player's speed
		bool  splashEnabled{ true };      // play the engine's water splash at the player: entering water fast, and while running through it
		float splashEntrySpeed{ 180.0f }; // units per second (any direction) at which stepping into the water makes a splash
		float splashRunSpeed{ 280.0f };   // horizontal speed above which running through water keeps making splashes
		float splashInterval{ 0.45f };    // seconds between splashes while running
		float splashPickLength{ 600.0f }; // how far below the node the engine looks for the water surface
		std::string   splashModel{ "Effects\\waterSplash.nif" };  // the mesh the engine uses for a water splash
		float         splashScaleMin{ 0.6f };           // mesh scale at the entry / running speed thresholds
		float         splashScaleMax{ 2.0f };           // mesh scale at 600 units per second and faster
		float         splashLifetime{ 1.5f };           // seconds the mesh lives
		std::uint32_t splashSmallForm{ 0x02BCA5 };       // MoveableStatic in Skyrim.esm: FXSplashSmallParticles (the mesh is the one CS Water Mod re-skins)
		std::uint32_t splashBigForm{ 0x0FD0D1 };         // FXSplashHeavyWater
		float         splashBigSpeed{ 450.0f };          // speed at which the heavy splash replaces the small one
		int           splashFlags{ 7 };
		int           splashDebug{ 0 };                 // 1 = every second, spawn the effect 150 units in front of the player at head height, in or out of water (is anything visible at all?)
		bool  forceEnabled{ true };       // raise the displacement force of the water type the player stands in with the player's speed (wake height)
		float forceMaxScale{ 3.0f };      // force at the maximum speed, relative to the water type's own value
		float sizeMaxScale{ 1.0f };       // starting size at the maximum speed, relative to the water type's own value
		float quadMaxScale{ 2.0f };       // patch size at the maximum speed, relative to the game's own value
		float wakeMinSpeed{ 60.0f };      // game units per second below which no extra rings are placed
		float wakeMaxSpeed{ 450.0f };     // speed at which the rings reach their maximum size
		float wakeScaleMin{ 0.08f };
		float wakeScaleMax{ 0.5f };
		float wakeInterval{ 0.25f };      // seconds between rings at the minimum speed (shorter at higher speed)
		float wakeMinDepth{ 0.12f };      // submerged level the player needs before rings are placed
		int   logSeconds{ 0 };
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
			if (key == "bSlowdown") c.slowEnabled = Truthy(val);
			else if (key == "bPlayerOnly") c.playerOnly = Truthy(val);
			else if (key == "fSmallSlowdownPercent") c.smallSlowdown = f;
			else if (key == "fLargeSlowdownPercent") c.largeSlowdown = f;
			else if (key == "fSwimSlowdownPercent") c.swimSlowdown = f;
			else if (key == "fSwimLevel") c.swimLevel = f;
			else if (key == "fMinSpeedMultiplier") c.minSpeed = f;
			else if (key == "fArmorWadeFactor") c.armorWadeFactor = f;
			else if (key == "fHeavyBody") c.heavy[0] = f;
			else if (key == "fHeavyFeet") c.heavy[1] = f;
			else if (key == "fHeavyHands") c.heavy[2] = f;
			else if (key == "fHeavyHead") c.heavy[3] = f;
			else if (key == "fLightBody") c.light[0] = f;
			else if (key == "fLightFeet") c.light[1] = f;
			else if (key == "fLightHands") c.light[2] = f;
			else if (key == "fLightHead") c.light[3] = f;
			else if (key == "fClothBody") c.cloth[0] = f;
			else if (key == "fClothFeet") c.cloth[1] = f;
			else if (key == "fClothHands") c.cloth[2] = f;
			else if (key == "fClothHead") c.cloth[3] = f;
			else if (key == "bWake") c.wakeEnabled = Truthy(val);
			else if (key == "bWakePatch") c.quadEnabled = Truthy(val);
			else if (key == "fWakePatchMaxScale") c.quadMaxScale = f;
			else if (key == "bSplash") c.splashEnabled = Truthy(val);
			else if (key == "fSplashEntrySpeed") c.splashEntrySpeed = f;
			else if (key == "fSplashRunSpeed") c.splashRunSpeed = f;
			else if (key == "fSplashInterval") c.splashInterval = std::clamp(f, 0.1f, 5.0f);
			else if (key == "fSplashPickLength") c.splashPickLength = std::clamp(f, 50.0f, 4000.0f);
			else if (key == "sSplashModel") c.splashModel = val;
			else if (key == "fSplashScaleMin") c.splashScaleMin = std::clamp(f, 0.05f, 20.0f);
			else if (key == "fSplashScaleMax") c.splashScaleMax = std::clamp(f, 0.05f, 20.0f);
			else if (key == "fSplashLifetime") c.splashLifetime = std::clamp(f, 0.2f, 10.0f);
			else if (key == "iSplashFlags") c.splashFlags = std::atoi(val.c_str());
			else if (key == "sSplashSmallForm") c.splashSmallForm = static_cast<std::uint32_t>(std::strtoul(val.c_str(), nullptr, 16));
			else if (key == "sSplashBigForm") c.splashBigForm = static_cast<std::uint32_t>(std::strtoul(val.c_str(), nullptr, 16));
			else if (key == "fSplashBigSpeed") c.splashBigSpeed = f;
			else if (key == "iSplashDebug") c.splashDebug = std::atoi(val.c_str());
			else if (key == "bWakeHeight") c.forceEnabled = Truthy(val);
			else if (key == "fWakeHeightMaxScale") c.forceMaxScale = f;
			else if (key == "fWakeSizeMaxScale") c.sizeMaxScale = f;
			else if (key == "fWakeMinSpeed") c.wakeMinSpeed = f;
			else if (key == "fWakeMaxSpeed") c.wakeMaxSpeed = f;
			else if (key == "fWakeScaleMin") c.wakeScaleMin = f;
			else if (key == "fWakeScaleMax") c.wakeScaleMax = f;
			else if (key == "fWakeInterval") c.wakeInterval = f;
			else if (key == "fWakeMinDepth") c.wakeMinDepth = f;
			else if (key == "iLogSeconds") c.logSeconds = std::max(0, std::atoi(val.c_str()));
		}
		c.swimLevel = std::clamp(c.swimLevel, 0.3f, 0.99f);
		c.minSpeed = std::clamp(c.minSpeed, 0.0f, 1.0f);
		c.wakeMaxSpeed = std::max(c.wakeMaxSpeed, c.wakeMinSpeed + 1.0f);
		c.wakeInterval = std::clamp(c.wakeInterval, 0.03f, 5.0f);
		c.quadMaxScale = std::clamp(c.quadMaxScale, 0.25f, 6.0f);
		c.forceMaxScale = std::clamp(c.forceMaxScale, 0.1f, 20.0f);
		c.sizeMaxScale = std::clamp(c.sizeMaxScale, 0.1f, 5.0f);
		return c;
	}

	std::shared_ptr<const Config> g_config = std::make_shared<Config>();
	std::atomic<float>            g_lastPlayerMult{ 1.0f };
	std::atomic<float>            g_lastPlayerLevel{ 0.0f };
	std::atomic<float>            g_lastPlayerOriginal{ 0.0f };
	std::atomic<float>            g_lastPlayerResult{ 0.0f };
	std::atomic<std::uint32_t>    g_playerCalls{ 0 };
	std::atomic<float>            g_quadNow{ 0.0f };
	std::atomic<float>            g_forceMul{ 1.0f };
	std::atomic<float>            g_quadSeen{ 0.0f };   // what the setting held at the start of the tick, before this plugin wrote it

	std::shared_ptr<const Config> Cfg() { return std::atomic_load(&g_config); }

	float Smooth(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	float ArmorPenalty(RE::Actor* a_actor, const Config& c)
	{
		static constexpr RE::BIPED_MODEL::BipedObjectSlot slots[4] = { RE::BIPED_MODEL::BipedObjectSlot::kBody, RE::BIPED_MODEL::BipedObjectSlot::kFeet,
			RE::BIPED_MODEL::BipedObjectSlot::kHands, RE::BIPED_MODEL::BipedObjectSlot::kHead };
		float total = 0.0f;
		for (int i = 0; i < 4; ++i) {
			auto* armor = a_actor->GetWornArmor(slots[i]);
			if (!armor) {
				continue;
			}
			switch (armor->GetArmorType()) {
			case RE::BGSBipedObjectForm::ArmorType::kHeavyArmor:
				total += c.heavy[i];
				break;
			case RE::BGSBipedObjectForm::ArmorType::kLightArmor:
				total += c.light[i];
				break;
			case RE::BGSBipedObjectForm::ArmorType::kClothing:
				total += c.cloth[i];
				break;
			default:
				break;
			}
		}
		return total;
	}

	// speed multiplier for an actor standing in water of the given submerged level (0 dry .. 1 over the head)
	float WaterMultiplier(RE::Actor* a_actor, float a_level, const Config& c)
	{
		if (a_level < 0.02f) {
			return 1.0f;
		}
		float slow;  // fraction
		if (a_level >= c.swimLevel) {
			slow = c.swimSlowdown * 0.01f;
			slow += ArmorPenalty(a_actor, c);
		} else {
			const float shallow = c.smallSlowdown * 0.01f;
			const float deep = c.largeSlowdown * 0.01f;
			if (a_level < 0.2f) {
				slow = shallow * Smooth(a_level / 0.2f);
			} else {
				slow = shallow + (deep - shallow) * Smooth((a_level - 0.2f) / 0.35f);
			}
			if (c.armorWadeFactor > 0.0f) {
				slow += ArmorPenalty(a_actor, c) * c.armorWadeFactor * Smooth((a_level - 0.2f) / (c.swimLevel - 0.2f));
			}
		}
		return std::clamp(1.0f - slow, c.minSpeed, 1.0f);
	}

	struct SpeedHook
	{
		static float thunk(RE::Actor* a_actor)
		{
			const float original = func(a_actor);
			if (!a_actor) {
				return original;
			}
			const auto c = Cfg();
			if (a_actor->IsPlayerRef()) {
				g_lastPlayerOriginal = original;
			}
			if (!c->slowEnabled || (c->playerOnly && !a_actor->IsPlayerRef())) {
				return original;
			}
			const float level = a_actor->GetSubmergedLevel(a_actor->GetPositionZ(), a_actor->GetParentCell());
			const float mult = WaterMultiplier(a_actor, level, *c);
			if (a_actor->IsPlayerRef()) {
				g_lastPlayerMult = mult;
				g_lastPlayerLevel = level;
				g_lastPlayerOriginal = original;
				g_lastPlayerResult = original * mult;
				++g_playerCalls;
			}
			return original * mult;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void InstallHook()
	{
		const auto addr = REL::RelocationID(37013, 37943).address() + REL::Relocate(0x1A, 0x51);
		if (*reinterpret_cast<const std::uint8_t*>(addr) != 0xE8) {
			logger::error("movement speed call site at {:#x} is not a call instruction; the slowdown is NOT installed", addr);
			return;
		}
		auto& trampoline = SKSE::GetTrampoline();
		SKSE::AllocTrampoline(14);
		SpeedHook::func = trampoline.write_call<5>(addr, SpeedHook::thunk);
		logger::info("movement speed hook installed at {:#x}", addr);
	}

	// ---------------------------------------------------------------------------------------------------------------- wake rings
	// placed splash objects: base object placed at the water surface, deleted after a short life
	struct Placed
	{
		RE::ObjectRefHandle handle;
		float               left;
	};
	std::vector<Placed> g_placed;

	bool PlaceSplash(RE::PlayerCharacter* a_player, std::uint32_t a_formId, const RE::NiPoint3& a_at, float a_scale, float a_life)
	{
		auto* base = RE::TESForm::LookupByID<RE::TESBoundObject>(a_formId);
		if (!base) {
			return false;
		}
		auto ref = a_player->PlaceObjectAtMe(base, false);
		if (!ref) {
			return false;
		}
		ref->SetPosition(a_at);
		ref->SetScale(a_scale);
		g_placed.push_back({ ref->GetHandle(), a_life });
		return true;
	}

	void ExpireSplashes(float a_dt)
	{
		for (auto it = g_placed.begin(); it != g_placed.end();) {
			it->left -= a_dt;
			if (it->left <= 0.0f) {
				if (auto ref = it->handle.get()) {
					ref->Disable();
					ref->SetDelete(true);
				}
				it = g_placed.erase(it);
			} else {
				++it;
			}
		}
	}

	std::atomic<bool> g_pending{ false };

	void Update(float a_dt)
	{
		struct Reset
		{
			~Reset() { g_pending = false; }
		} reset;

		ExpireSplashes(a_dt);

		static float         lastX = 0, lastY = 0;
		static bool          haveLast = false;
		static float         sinceRing = 0.0f;
		static float         sinceLog = 0.0f;
		static float         smoothedSpeed = 0.0f;
		static std::uint64_t rings = 0;

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* ws = RE::TESWaterSystem::GetSingleton();
		auto* ui = RE::UI::GetSingleton();
		if (!player || !ws || !player->Is3DLoaded() || (ui && ui->GameIsPaused())) {
			haveLast = false;
			return;
		}
		const auto c = Cfg();
		const auto pos = player->GetPosition();
		float      speed = 0.0f;
		if (haveLast && a_dt > 0.001f) {
			speed = std::hypot(pos.x - lastX, pos.y - lastY) / a_dt;
			if (speed > 2500.0f) {  // teleport / cell change, not movement
				speed = 0.0f;
			}
		}
		lastX = pos.x;
		lastY = pos.y;
		haveLast = true;
		smoothedSpeed += (speed - smoothedSpeed) * std::min(1.0f, a_dt * 6.0f);

		const float level = player->GetSubmergedLevel(pos.z, player->GetParentCell());
		sinceRing += a_dt;
		sinceLog += a_dt;

		{
			// splash: the engine's impact effect for a water hit (the arrow-into-water splash), played at the player
			static float prevLevel = 0.0f;
			static float prevZ = 0.0f;
			static float sinceSplash = 10.0f;
			static int   attempts = 0;
			sinceSplash += a_dt;
			const float vz = a_dt > 0.001f ? std::fabs(pos.z - prevZ) / a_dt : 0.0f;
			const float total = std::sqrt(smoothedSpeed * smoothedSpeed + vz * vz);
			bool        play = false;
			if (c->splashEnabled && level >= 0.08f) {
				if (prevLevel < 0.08f && std::max(speed, vz) >= c->splashEntrySpeed) {
					play = true;  // stepping or jumping into the water
				} else if (level <= 0.75f && smoothedSpeed >= c->splashRunSpeed && sinceSplash >= c->splashInterval) {
					play = true;  // running through it
				}
			}
			prevLevel = level;
			prevZ = pos.z;
			if (c->splashDebug > 0) {
				static float sinceDebug = 10.0f;
				sinceDebug += a_dt;
				if (sinceDebug >= 1.0f) {
					sinceDebug = 0.0f;
					const float  yaw = player->GetAngleZ();
					RE::NiPoint3 at{ pos.x + std::sin(yaw) * 150.0f, pos.y + std::cos(yaw) * 150.0f, pos.z + 100.0f };
					const bool   ok = PlaceSplash(player, c->splashSmallForm, at, 1.0f, 4.0f);
					logger::info("splash DEBUG: form {:#x} at ({:.0f},{:.0f},{:.0f}) player at ({:.0f},{:.0f},{:.0f}) -> {}", c->splashSmallForm, at.x, at.y, at.z, pos.x, pos.y, pos.z, ok ? "placed" : "NOT placed");
				}
			}
			if (play && sinceSplash >= 0.15f) {
				float waterZ = player->GetWaterHeight();
				if (!std::isfinite(waterZ) || std::fabs(waterZ - pos.z) > 4000.0f) {
					waterZ = pos.z;
				}
				const float  speedT = std::clamp((total - c->splashEntrySpeed) / std::max(1.0f, 600.0f - c->splashEntrySpeed), 0.0f, 1.0f);
				const float  scale = c->splashScaleMin + (c->splashScaleMax - c->splashScaleMin) * speedT;
				RE::NiPoint3 at{ pos.x, pos.y, waterZ };
				const bool   big = total >= c->splashBigSpeed;
				const bool   ok = PlaceSplash(player, big ? c->splashBigForm : c->splashSmallForm, at, scale, c->splashLifetime);
				sinceSplash = 0.0f;
				if (attempts < 40) {
					++attempts;
					logger::info("splash: speed={:.0f} fall={:.0f} level={:.2f} {} scale={:.2f} at water z={:.0f} (player z={:.0f}) -> {}", total, vz, level, big ? "heavy" : "small", scale, waterZ, pos.z, ok ? "placed" : "NOT placed");
				}
			}
		}

		{
			// wake height: the displacement simulator values of the water types. The engine's "current water type" pointer is not
			// reliable while wading, so every water type is scaled together; only the one the player is in matters.
			struct Disp
			{
				RE::TESWaterForm* form;
				float             size;
				float             force;
			};
			static std::vector<Disp> types;
			static bool              captured = false;
			if (!captured) {
				if (auto* dh = RE::TESDataHandler::GetSingleton()) {
					for (auto* wf : dh->GetFormArray<RE::TESWaterForm>()) {
						if (wf) {
							types.push_back({ wf, wf->data.displacementSize, wf->data.displacementForce });
						}
					}
					captured = true;
					logger::info("wake height: {} water types captured (DefaultWater-like types have size 0.70, force 0.100)", types.size());
				}
			}
			const float tt = c->forceEnabled && level >= c->wakeMinDepth ? std::clamp((smoothedSpeed - c->wakeMinSpeed) / (c->wakeMaxSpeed - c->wakeMinSpeed), 0.0f, 1.0f) : 0.0f;
			const float forceMul = 1.0f + (c->forceMaxScale - 1.0f) * tt;
			const float sizeMul = 1.0f + (c->sizeMaxScale - 1.0f) * tt;
			for (auto& d : types) {
				d.form->data.displacementForce = d.force * forceMul;
				d.form->data.displacementSize = d.size * sizeMul;
			}
			g_forceMul = forceMul;
		}

		{
			static RE::Setting* quad = nullptr;
			static float        quadBase = 0.0f;
			static bool         quadLooked = false;
			if (!quadLooked) {
				quadLooked = true;
				const char* where = "game settings";
				if (auto* gs = RE::GameSettingCollection::GetSingleton()) {
					quad = gs->GetSetting("fWadingWaterQuadSize");
				}
				if (!quad) {
					where = "ini settings";
					if (auto* ini = RE::INISettingCollection::GetSingleton()) {
						quad = ini->GetSetting("fWadingWaterQuadSize:Water");
					}
				}
				if (!quad) {
					where = "ini prefs";
					if (auto* prefs = RE::INIPrefSettingCollection::GetSingleton()) {
						quad = prefs->GetSetting("fWadingWaterQuadSize:Water");
					}
				}
				if (quad) {
					quadBase = quad->GetFloat();
					logger::info("fWadingWaterQuadSize is {:.1f} (found in the {})", quadBase, where);
				} else {
					logger::info("fWadingWaterQuadSize not found in the game, ini or prefs settings: the wake patch cannot be scaled");
				}
			}
			if (quad && quadBase > 0.0f) {
				float target = quadBase;
				if (c->quadEnabled && level >= c->wakeMinDepth) {
					const float t = std::clamp((smoothedSpeed - c->wakeMinSpeed) / (c->wakeMaxSpeed - c->wakeMinSpeed), 0.0f, 1.0f);
					target = quadBase * (1.0f + (c->quadMaxScale - 1.0f) * t);
				}
				g_quadSeen = quad->GetFloat();
				if (std::fabs(quad->GetFloat() - target) > 0.01f) {
					quad->data.f = target;
				}
				g_quadNow = target;
			}
		}

		if (c->wakeEnabled && level >= c->wakeMinDepth && smoothedSpeed >= c->wakeMinSpeed) {
			const float t = std::clamp((smoothedSpeed - c->wakeMinSpeed) / (c->wakeMaxSpeed - c->wakeMinSpeed), 0.0f, 1.0f);
			const float interval = c->wakeInterval * (1.0f - 0.6f * t);
			if (sinceRing >= interval) {
				sinceRing = 0.0f;
				const float  scale = c->wakeScaleMin + (c->wakeScaleMax - c->wakeScaleMin) * t;
				float        waterZ = player->GetWaterHeight();
				if (!std::isfinite(waterZ) || std::fabs(waterZ - pos.z) > 4000.0f) {
					waterZ = pos.z;  // lookup failed: the ripple only needs the position on the water plane
				}
				RE::NiPoint3 at{ pos.x, pos.y, waterZ };
				ws->AddRipple(at, scale);
				++rings;
			}
		}

		if (c->logSeconds > 0 && sinceLog >= static_cast<float>(c->logSeconds)) {
			sinceLog = 0.0f;
			logger::info("{}: level={:.2f} speedMult={:.2f} engineValue={:.2f} -> {:.2f} calls={} moving={:.0f} u/s rings so far={} wakePatch={:.0f} (read back {:.0f}) heightX={:.2f}", level > 0.02f ? "wading" : "dry", level, g_lastPlayerMult.load(),
				g_lastPlayerOriginal.load(), g_lastPlayerResult.load(), g_playerCalls.exchange(0), smoothedSpeed, rings, g_quadNow.load(), g_quadSeen.load(), g_forceMul.load());
		}
	}

	void SetupLog()
	{
		auto folder = logger::log_directory();
		if (!folder) {
			return;
		}
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*folder / "WaterWade.log").string(), true);
		auto log = std::make_shared<spdlog::logger>("log", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S.%e] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
			logger::info("WaterWade 0.1 on game {}", REL::Module::get().version().string());
			std::thread([]() {
				auto prev = std::chrono::steady_clock::now();
				auto lastCfg = prev;
				for (;;) {
					std::this_thread::sleep_for(50ms);
					const auto now = std::chrono::steady_clock::now();
					if (now - lastCfg >= 1s) {
						lastCfg = now;
						std::atomic_store(&g_config, std::make_shared<const Config>(ReadConfig()));
					}
					const float dt = std::chrono::duration<float>(now - prev).count();
					prev = now;
					if (!g_pending.exchange(true)) {
						SKSE::GetTaskInterface()->AddTask([dt]() { Update(dt); });
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
	logger::info("WaterWade loaded");
	std::atomic_store(&g_config, std::make_shared<const Config>(ReadConfig()));
	InstallHook();
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
