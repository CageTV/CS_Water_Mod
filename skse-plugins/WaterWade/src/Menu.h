#pragma once
// WaterWade's page in SKSE Menu Framework: "CS Water Mod / WaterWade". Every setting is a line of WaterWade.ini, written live.
// (The per-armor swim penalties stay in the ini only.)
#include "CswMenu.h"

namespace WaterWadeMenu
{
	using CswMenu::Entry;

	inline Entry B(const char* a_section, const char* a_key, const char* a_label, const char* a_tip, bool a_def)
	{
		return Entry{ Entry::Kind::Bool, a_section, a_key, a_label, a_tip, 0, 1, "", a_def ? 1.0f : 0.0f, 0 };
	}
	inline Entry F(const char* a_section, const char* a_key, const char* a_label, const char* a_tip, float a_min, float a_max, const char* a_fmt, float a_def)
	{
		return Entry{ Entry::Kind::Float, a_section, a_key, a_label, a_tip, a_min, a_max, a_fmt, a_def, 0 };
	}

	inline CswMenu::Page& Page()
	{
		static CswMenu::Page page(
			"Data/SKSE/Plugins/WaterWade.ini",
			{
				B("Wading", "bSlowdown", "Wading slows movement", "Off: no slowdown from water depth or from armor.", true),
				B("Wading", "bYieldToWadeInWater", "Let Wade In Water Redone do the slowdown", "On: if Wade In Water Redone (WadeInWater.dll) is installed, WaterWade does not slow actors, so the two never add up. Splashes and wake stay.", true),
				B("Wading", "bPlayerOnly", "Only the player is slowed", "Off: every actor (NPCs, followers, horses) is slowed too.", false),
				F("Wading", "fSmallSlowdownPercent", "Ankle deep slowdown (%)", "Slowdown at about ankle depth (submerged level 0.2).", 0.0f, 60.0f, "%.0f", 8.0f),
				F("Wading", "fLargeSlowdownPercent", "Waist deep slowdown (%)", "Slowdown at about waist depth (submerged level 0.55).", 0.0f, 80.0f, "%.0f", 25.0f),
				F("Wading", "fSwimSlowdownPercent", "Swimming slowdown (%)", "Slowdown while swimming, before armor.", 0.0f, 80.0f, "%.0f", 10.0f),
				F("Wading", "fMinSpeedMultiplier", "Slowest allowed speed", "The speed multiplier never drops below this (1 = no limit to slowing is applied).", 0.0f, 1.0f, "%.2f", 0.10f),
				F("Wading", "fArmorWadeFactor", "Armor penalty while wading", "Share of the swim armor penalty that also applies while wading (0 = swimming only).", 0.0f, 1.0f, "%.2f", 0.0f),

				B("Wake", "bWakePatch", "Wake grows with speed", "Grows the engine's own wading wake patch with how fast the player moves.", true),
				F("Wake", "fWakePatchMaxScale", "Wake size at top speed", "The wake patch is this many times its normal size at the top speed below.", 1.0f, 6.0f, "%.1f", 2.0f),
				F("Wake", "fWakeMinSpeed", "Wake starts at (units/s)", "Walking is about 130, running about 400.", 0.0f, 600.0f, "%.0f", 60.0f),
				F("Wake", "fWakeMaxSpeed", "Wake is biggest at (units/s)", "Speed at which the wake reaches its full size.", 100.0f, 900.0f, "%.0f", 450.0f),

				B("Splash", "bSplash", "Splashes", "Spawns a water splash at the surface under the player.", true),
				F("Splash", "fSplashEntrySpeed", "Entry splash speed", "Speed at which stepping or jumping into water makes a splash.", 0.0f, 600.0f, "%.0f", 180.0f),
				F("Splash", "fSplashRunSpeed", "Running splash speed", "Running faster than this in knee to waist deep water keeps splashing.", 0.0f, 600.0f, "%.0f", 280.0f),
				F("Splash", "fSplashInterval", "Seconds between splashes", "While running through water.", 0.1f, 3.0f, "%.2f", 0.45f),
				F("Splash", "fSplashScaleMin", "Splash size, slow", "Splash scale at the entry / running speed.", 0.05f, 3.0f, "%.2f", 0.45f),
				F("Splash", "fSplashScaleMax", "Splash size, fast", "Splash scale at 600 units per second and faster.", 0.05f, 3.0f, "%.2f", 1.0f),
				F("Splash", "fSplashLifetime", "Splash lifetime (s)", "How long a splash stays before it is removed.", 0.2f, 5.0f, "%.1f", 1.0f),
				F("Splash", "fSplashBigSpeed", "Heavy splash from speed", "At or above this speed the heavy splash replaces the small one.", 200.0f, 900.0f, "%.0f", 450.0f),

				B("ShipWake", "bShipWake", "Ship wake (experimental)", "A foam V behind the moving ships of Animated Ships (Vicn). Follows the ship; nothing in the ship mod is changed.", false),
				B("ShipWake", "bShipMesh", "Ship wake: foam V mesh", "The foam V attached to the ship hull.", true),
				F("ShipWake", "fShipMeshScale", "Ship wake size", "Size multiplier of the foam V (it is about 1.6 ship lengths long at 1.0).", 0.3f, 3.0f, "%.2f", 1.0f),
				F("ShipWake", "fShipMeshZ", "Ship wake height", "Height of the foam above the waterline.", -20.0f, 40.0f, "%.0f", 4.0f),
				F("ShipWake", "fShipMinSpeed", "Ship wake starts at (units/s)", "A ship slower than this has no wake.", 0.0f, 200.0f, "%.0f", 25.0f),
			},
			"WaterWade slows actors by how deep they wade, grows the wake with the player's speed and adds splashes. Armor swim penalties "
			"and the splash meshes are only in WaterWade.ini.");
		return page;
	}

	inline void __stdcall Render() { Page().Render(); }

	inline void Register()
	{
		if (!CswMenu::Smf::Module()) {
			logger::info("No SKSE Menu Framework found; the in-game page is off. Settings are in WaterWade.ini.");
			return;
		}
		const bool ok = CswMenu::Smf::AddSectionItem("CS Water Mod/WaterWade", Render);
		logger::info("SKSE Menu Framework {:.1f}: {}", CswMenu::Smf::Version(), ok ? "WaterWade page added" : "AddSectionItem not exported, page off");
	}
}
