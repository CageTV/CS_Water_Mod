#pragma once
// Ship wakes for Vicn's Animated Ships (and any mesh whose path contains sShipModelMatch): the ships move through animation baked into their
// NIFs, so the reference itself may stay put. The moving 3D root's world bound is read every update; the speed and heading come from how
// that centre moves. While a ship moves on water the engine's own ripple (AddRipple) and the same splash objects WaterWade uses for the
// player are placed at its bow. Nothing in any other mod is edited.
#include "CswMenu.h"

namespace ShipWake
{
	struct Config
	{
		bool        enabled{ true };
		std::string modelMatch{ "animatedship" };  // lower case, matched inside the base model path
		float       radius{ 9000.0f };             // ships farther than this from the player get no wake
		float       minSpeed{ 25.0f };             // units/s below which a ship is treated as still
		float       maxSpeed{ 400.0f };            // units/s at which the wake is at full size
		float       bowFraction{ 0.55f };          // bow offset along the heading, as a share of the bounding radius
		float       rippleInterval{ 0.20f };       // seconds between ripples at the minimum speed (shorter when faster)
		float       rippleScaleMin{ 1.5f };
		float       rippleScaleMax{ 4.0f };
		bool        splash{ true };  // splash objects along the V arms
		float       splashInterval{ 0.7f };
		float       splashScaleMin{ 0.7f };
		float       splashScaleMax{ 1.8f };
		float       splashLifetime{ 1.2f };
		std::uint32_t splashForm{ 0x02BCA5 };
		float       vAngle{ 19.0f };               // half opening angle of the V behind the bow, degrees
		float       vLength{ 1.6f };               // length of each arm, in ship lengths
		int         vPoints{ 7 };                  // ripples per arm
		float       vScale{ 1.6f };                // ripple size on the arms, relative to the bow ripple
		bool        bowSplash{ false };            // the ships already carry their own bow splash and stern bubbles
		bool        ripples{ false };              // the engine ripple at the bow (only shows near the player; streaky)
		bool        mesh{ true };                  // the foam V mesh attached to the ship
		std::string meshPath{ "Effects\\CSWater\\ShipWake.nif" };  // relative to Meshes
		float       meshZ{ 4.0f };                 // height of the mesh above the hull node (the waterline)
		float       meshScale{ 1.0f };
		int         debug{ 0 };                    // 1 = log what is found every few seconds
	};

	inline bool Truthy(const std::string& v) { return v == "1" || CswMenu::Lower(v) == "true"; }

	inline Config ReadConfig(const std::filesystem::path& a_path)
	{
		Config      c;
		const auto  m = CswMenu::IniRead(a_path);
		auto        get = [&](const char* k) -> const std::string* {
            auto it = m.find(std::string("shipwake/") + CswMenu::Lower(k));
            return it == m.end() ? nullptr : &it->second;
		};
		auto f = [&](const char* k, float& out) { if (auto* v = get(k)) out = static_cast<float>(std::atof(v->c_str())); };
		if (auto* v = get("bShipWake")) c.enabled = Truthy(*v);
		if (auto* v = get("sShipModelMatch")) c.modelMatch = CswMenu::Lower(*v);
		f("fShipRadius", c.radius);
		f("fShipMinSpeed", c.minSpeed);
		f("fShipMaxSpeed", c.maxSpeed);
		f("fShipBowFraction", c.bowFraction);
		f("fShipRippleInterval", c.rippleInterval);
		f("fShipRippleScaleMin", c.rippleScaleMin);
		f("fShipRippleScaleMax", c.rippleScaleMax);
		if (auto* v = get("bShipSplash")) c.splash = Truthy(*v);
		f("fShipSplashInterval", c.splashInterval);
		f("fShipSplashScaleMin", c.splashScaleMin);
		f("fShipSplashScaleMax", c.splashScaleMax);
		f("fShipSplashLifetime", c.splashLifetime);
		if (auto* v = get("sShipSplashForm")) c.splashForm = static_cast<std::uint32_t>(std::strtoul(v->c_str(), nullptr, 16));
		f("fShipVAngle", c.vAngle);
		f("fShipVLength", c.vLength);
		if (auto* v = get("iShipVPoints")) c.vPoints = std::clamp(std::atoi(v->c_str()), 0, 20);
		f("fShipVScale", c.vScale);
		if (auto* v = get("bShipBowSplash")) c.bowSplash = Truthy(*v);
		if (auto* v = get("bShipRipples")) c.ripples = Truthy(*v);
		if (auto* v = get("bShipMesh")) c.mesh = Truthy(*v);
		if (auto* v = get("sShipMeshPath")) c.meshPath = *v;
		f("fShipMeshZ", c.meshZ);
		f("fShipMeshScale", c.meshScale);
		if (auto* v = get("iShipDebug")) c.debug = std::atoi(v->c_str());
		c.splashLifetime = std::clamp(c.splashLifetime, 0.2f, 10.0f);
		c.maxSpeed = std::max(c.maxSpeed, c.minSpeed + 1.0f);
		return c;
	}

	struct Ship
	{
		RE::ObjectRefHandle handle;
		RE::NiPoint3        last{};
		bool                haveLast{ false };
		float               speed{ 0.0f };      // smoothed units/s
		RE::NiPoint3        dir{ 0, 1, 0 };     // smoothed horizontal heading (unit)
		float               sinceRipple{ 10.0f };
		float               sinceSplash{ 10.0f };
		float               sinceLog{ 100.0f };
		std::string         model;
		bool                dumped{ false };
		bool                sized{ false };
		float               bowLen{ 600.0f };   // from the hull node to the bow, along its +Y
		float               sternLen{ 600.0f };
		RE::NiPointer<RE::NiNode> wake;        // the foam V, a child of the hull node
		bool                wakeFailed{ false };
	};

	inline std::vector<Ship> g_ships;
	inline float             g_sinceScan = 100.0f;

	inline std::string ModelOf(RE::TESObjectREFR* a_ref)
	{
		auto* base = a_ref ? a_ref->GetBaseObject() : nullptr;
		if (!base) return {};
		if (auto* model = base->As<RE::TESModel>()) {
			if (auto* p = model->GetModel()) return p;
		}
		return {};
	}

	inline void Scan(RE::PlayerCharacter* a_player, const Config& c)
	{
		auto* tes = RE::TES::GetSingleton();
		if (!tes) return;
		std::vector<Ship> found;
		tes->ForEachReferenceInRange(a_player, c.radius, [&](RE::TESObjectREFR* ref) {
			if (ref && !ref->IsDisabled() && ref->Is3DLoaded()) {
				const auto model = CswMenu::Lower(ModelOf(ref));
				if (!model.empty() && model.find(c.modelMatch) != std::string::npos) {
					Ship s;
					s.handle = ref->GetHandle();
					s.model = model;
					found.push_back(std::move(s));
				}
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});
		// keep the motion state of ships that were already tracked
		for (auto& n : found) {
			for (auto& o : g_ships) {
				if (o.handle == n.handle) {
					n = o;
					break;
				}
			}
		}
		if (c.debug > 0 && found.size() != g_ships.size()) {
			logger::info("ship wake: {} ship reference(s) within {:.0f} units", found.size(), c.radius);
		}
		g_ships = std::move(found);
	}

	inline void Dump(RE::NiAVObject* a_obj, int a_depth, int& a_budget)
	{
		if (!a_obj || a_budget <= 0 || a_depth > 5) return;
		--a_budget;
		const auto& w = a_obj->world.translate;
		logger::info("ship dump: {:{}}{} [{}] world ({:.0f},{:.0f},{:.0f}) local ({:.0f},{:.0f},{:.0f}) bound r={:.0f}", "", a_depth * 2, a_obj->name.c_str(), a_obj->GetRTTI() ? a_obj->GetRTTI()->GetName() : "?", w.x, w.y, w.z, a_obj->local.translate.x, a_obj->local.translate.y, a_obj->local.translate.z, a_obj->worldBound.radius);
		if (auto* node = a_obj->AsNode()) {
			for (auto& child : node->GetChildren()) {
				Dump(child.get(), a_depth + 1, a_budget);
			}
		}
	}

	inline void Measure(RE::NiAVObject* a_obj, float& a_bow, float& a_stern, bool& a_bowFound, bool& a_sternFound)
	{
		if (!a_obj) return;
		const std::string n = a_obj->name.c_str();
		if (n == "ADDonNODESSplashL" && (!a_bowFound || a_obj->local.translate.y > a_bow)) {
			a_bow = a_obj->local.translate.y;
			a_bowFound = true;
		} else if (n == "ADDonNODESBubble" && (!a_sternFound || a_obj->local.translate.y < a_stern)) {
			a_stern = a_obj->local.translate.y;
			a_sternFound = true;
		}
		if (auto* node = a_obj->AsNode()) {
			for (auto& child : node->GetChildren()) {
				Measure(child.get(), a_bow, a_stern, a_bowFound, a_sternFound);
			}
		}
	}

	constexpr int kSlots = 8;
	inline RE::ObjectRefHandle g_slotOwner[kSlots];

	inline void DetachWake(Ship& a_s)
	{
		if (a_s.wake && a_s.wake->parent) {
			a_s.wake->parent->DetachChild2(a_s.wake.get());
		}
	}

	// the foam V is a clone of our own mesh, attached under the ship's moving hull node: it moves, turns and bobs with the ship, and
	// goes when the ship's 3D unloads. Nothing in the ship's files changes.
	inline void UpdateMesh(Ship& a_s, RE::NiAVObject* a_hull, const Config& c, float a_t)
	{
		auto* hullNode = a_hull ? a_hull->AsNode() : nullptr;
		if (!hullNode || a_s.wakeFailed) return;
		if (!a_s.wake) {
			// NiObject::Clone hangs the game on 1.7.104, so every ship gets its own copy of the file (ShipWake0.nif ... ShipWake7.nif): the model
			// cache returns a separate node per file name.
			int slot = -1;
			for (int i = 0; i < kSlots; ++i) {
				auto owner = g_slotOwner[i].get();
				if (!owner || g_slotOwner[i] == a_s.handle) {
					slot = i;
					break;
				}
			}
			if (slot < 0) {
				return;  // more ships than copies; try again when one has gone
			}
			std::string path = c.meshPath;
			const auto  dot = path.rfind('.');
			path.insert(dot == std::string::npos ? path.size() : dot, std::to_string(slot));
			logger::info("ship wake: loading {} for {}", path, a_s.model);
			RE::NiPointer<RE::NiNode>                 model;
			const RE::BSModelDB::DBTraits::ArgsType args{};
			RE::BSModelDB::Demand(path.c_str(), model, args);
			if (!model) {
				a_s.wakeFailed = true;
				logger::info("ship wake: the mesh '{}' did not load (is it in WaterWade's Meshes folder?)", path);
				return;
			}
			g_slotOwner[slot] = a_s.handle;
			a_s.wake = model;
		}
		const bool moving = a_s.speed >= c.minSpeed;
		a_s.wake->SetAppCulled(!moving);
		const float shipLen = a_s.bowLen + a_s.sternLen;
		a_s.wake->local.translate = RE::NiPoint3{ 0.0f, a_s.bowLen, c.meshZ };
		a_s.wake->local.scale = std::max(0.05f, shipLen * c.vLength * c.meshScale / 1000.0f * (0.7f + 0.3f * a_t));
		if (a_s.wake->parent != hullNode) {
			DetachWake(a_s);
			logger::info("ship wake: step 4 attaching");
			hullNode->AttachChild(a_s.wake.get(), true);
			logger::info("ship wake: step 5 attached, updating");
			RE::NiUpdateData ctx{};
			a_s.wake->Update(ctx);
			if (c.debug > 0) logger::info("ship wake: foam V attached to {} (length {:.0f})", a_s.model, shipLen);
		}
	}

	// a_placeSplash(formId, at, scale, life) places the same kind of splash object WaterWade uses for the player
	template <class PlaceSplash>
	inline void Update(float a_dt, RE::PlayerCharacter* a_player, RE::TESWaterSystem* a_ws, const Config& c, PlaceSplash&& a_placeSplash)
	{
		if (!c.enabled || !a_player || !a_ws || a_dt <= 0.001f) {
			for (auto& s : g_ships) DetachWake(s);
			g_ships.clear();
			return;
		}
		auto* tes = RE::TES::GetSingleton();
		g_sinceScan += a_dt;
		if (g_sinceScan >= 1.5f) {
			g_sinceScan = 0.0f;
			Scan(a_player, c);
		}
		const RE::BSFixedString hullName{ "PositionNode" };
		for (auto& s : g_ships) {
			auto ref = s.handle.get();
			auto* root = ref && ref->Is3DLoaded() ? ref->Get3D() : nullptr;
			// the ship's moving hull node (the reference itself and the root bound stay at the marker)
			auto* hull = root ? root->GetObjectByName(hullName) : nullptr;
			if (!hull) {
				s.haveLast = false;
				continue;
			}
			if (!s.sized) {
				s.sized = true;
				float bow = 0, stern = 0;
				bool  bf = false, sf = false;
				Measure(root, bow, stern, bf, sf);
				if (bf) s.bowLen = std::max(bow, 100.0f);
				if (sf) s.sternLen = std::max(-stern, 100.0f);
				if (c.debug > 0) logger::info("ship wake: {} bow {:.0f} stern {:.0f}{}", s.model, s.bowLen, s.sternLen, bf && sf ? "" : " (default; no effect nodes found)");
			}
			const RE::NiPoint3 centre = hull->world.translate;
			// heading: the hull's own +Y (the bow splash nodes sit at +Y)
			const RE::NiPoint3 fwd3 = hull->world.rotate * RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
			const float        fl = std::hypot(fwd3.x, fwd3.y);
			const RE::NiPoint3 fwd = fl > 0.001f ? RE::NiPoint3{ fwd3.x / fl, fwd3.y / fl, 0.0f } : RE::NiPoint3{ 0, 1, 0 };
			if (s.haveLast) {
				const float d = std::hypot(centre.x - s.last.x, centre.y - s.last.y);
				float       v = d / a_dt;
				if (v > 3000.0f) {  // moved to its marker / cell change, not sailing
					v = 0.0f;
				}
				s.speed += (v - s.speed) * std::min(1.0f, a_dt * 2.0f);
				s.dir = fwd;
			}
			s.last = centre;
			s.haveLast = true;
			s.sinceRipple += a_dt;
			s.sinceSplash += a_dt;
			s.sinceLog += a_dt;

			// the ships float at the water plane: use the water lookup when it answers, the hull height otherwise
			float waterZ = tes ? tes->GetWaterHeight(centre, ref->GetParentCell()) : centre.z;
			if (!std::isfinite(waterZ) || waterZ < -100000.0f || std::fabs(waterZ - centre.z) > 600.0f) {
				waterZ = centre.z;
			}

			if (c.debug > 0 && s.sinceLog >= 6.0f) {
				s.sinceLog = 0.0f;
				logger::info("ship wake: {} hull ({:.0f},{:.0f},{:.0f}) heading ({:.2f},{:.2f}) speed={:.0f}", s.model, centre.x, centre.y, centre.z, fwd.x, fwd.y, s.speed);
			}
			if (c.mesh) {
				UpdateMesh(s, hull, c, std::clamp((s.speed - c.minSpeed) / (c.maxSpeed - c.minSpeed), 0.0f, 1.0f));
			} else {
				DetachWake(s);
			}
			if (s.speed < c.minSpeed || !c.ripples) {
				continue;
			}
			const float t = std::clamp((s.speed - c.minSpeed) / (c.maxSpeed - c.minSpeed), 0.0f, 1.0f);
			const float shipLen = s.bowLen + s.sternLen;
			RE::NiPoint3 bow{ centre.x + fwd.x * s.bowLen, centre.y + fwd.y * s.bowLen, waterZ };
			if (s.sinceRipple >= c.rippleInterval * (1.0f - 0.6f * t)) {
				s.sinceRipple = 0.0f;
				const float bowScale = c.rippleScaleMin + (c.rippleScaleMax - c.rippleScaleMin) * t;
				a_ws->AddRipple(bow, bowScale);
				// the V: two arms from the bow, opening backwards
				const float tanA = std::tan(c.vAngle * 0.0174533f);
				const float armLen = shipLen * c.vLength;
				const RE::NiPoint3 left{ -fwd.y, fwd.x, 0.0f };
				for (int i = 1; i <= c.vPoints; ++i) {
					const float back = armLen * static_cast<float>(i) / static_cast<float>(c.vPoints);
					const float side = back * tanA;
					const float fade = 1.0f - 0.55f * static_cast<float>(i) / static_cast<float>(c.vPoints);
					for (int sgn = -1; sgn <= 1; sgn += 2) {
						RE::NiPoint3 p{ bow.x - fwd.x * back + left.x * side * static_cast<float>(sgn), bow.y - fwd.y * back + left.y * side * static_cast<float>(sgn), waterZ };
						a_ws->AddRipple(p, bowScale * c.vScale * fade);
					}
				}
			}
			if (s.sinceSplash >= c.splashInterval * (1.0f - 0.5f * t)) {
				s.sinceSplash = 0.0f;
				const float sc = c.splashScaleMin + (c.splashScaleMax - c.splashScaleMin) * t;
				if (c.bowSplash) {
					a_placeSplash(c.splashForm, bow, sc, c.splashLifetime);
				}
				if (c.splash) {
					const float tanA = std::tan(c.vAngle * 0.0174533f);
					const RE::NiPoint3 left{ -fwd.y, fwd.x, 0.0f };
					const float back = shipLen * 0.5f, side = back * tanA;
					for (int sgn = -1; sgn <= 1; sgn += 2) {
						a_placeSplash(c.splashForm, RE::NiPoint3{ bow.x - fwd.x * back + left.x * side * static_cast<float>(sgn), bow.y - fwd.y * back + left.y * side * static_cast<float>(sgn), waterZ }, sc * 0.8f, c.splashLifetime);
					}
				}
			}
		}
	}
}
