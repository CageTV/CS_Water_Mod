#pragma once
// WaterWind's page in SKSE Menu Framework: "CS Water Mod / WaterWind". Every setting is a line of WaterWind.ini, written live.
#include "CswMenu.h"

namespace WaterWindMenu
{
	using CswMenu::Entry;

	inline Entry B(const char* a_key, const char* a_label, const char* a_tip, bool a_def)
	{
		return Entry{ Entry::Kind::Bool, "WaterWind", a_key, a_label, a_tip, 0, 1, "", a_def ? 1.0f : 0.0f, 0 };
	}
	inline Entry F(const char* a_key, const char* a_label, const char* a_tip, float a_min, float a_max, const char* a_fmt, float a_def)
	{
		return Entry{ Entry::Kind::Float, "WaterWind", a_key, a_label, a_tip, a_min, a_max, a_fmt, a_def, 0 };
	}
	inline Entry I(const char* a_key, const char* a_label, const char* a_tip, int a_min, int a_max, int a_def)
	{
		return Entry{ Entry::Kind::Int, "WaterWind", a_key, a_label, a_tip, static_cast<float>(a_min), static_cast<float>(a_max), "%d", static_cast<float>(a_def), 0 };
	}

	inline CswMenu::Page& Page()
	{
		static CswMenu::Page page(
			"Data/SKSE/Plugins/WaterWind.ini",
			{
				B("bEnabled", "WaterWind on", "Off: every water type keeps its own scroll speed and wave height.", true),
				I("iWindSource", "Wind source (0 weather, 1 live sky)", "0 follows the weather record's steady wind, 1 follows the sky's live wind including gusts.", 0, 1, 0),
				F("fCalmWind", "Calm wind", "At or below this wind the calm scales apply (0 to 1; Clear weather is about 0.10).", 0.0f, 0.9f, "%.2f", 0.05f),
				F("fReferenceWind", "Reference wind", "At this wind the water keeps its own values (scale 1).", 0.01f, 0.95f, "%.2f", 0.12f),
				F("fStormWind", "Storm wind", "At or above this wind the storm scales apply.", 0.02f, 1.0f, "%.2f", 0.40f),
				F("fPrecipitationWindBonus", "Rain and snow wind bonus", "Added to the wind of rainy and snowy weathers, whose records have low wind values.", 0.0f, 0.5f, "%.2f", 0.10f),
				F("fCalmSpeedScale", "Ripple speed when calm", "Scroll speed of the ripples in no wind, times the water's own (1 = unchanged).", 0.0f, 3.0f, "%.2f", 0.40f),
				F("fStormSpeedScale", "Ripple speed in a storm", "Scroll speed of the ripples in a gale.", 0.0f, 8.0f, "%.2f", 2.0f),
				F("fCalmAmplitudeScale", "Wave height when calm", "How strongly the three ripple layers blend in no wind (1 = unchanged).", 0.0f, 3.0f, "%.2f", 0.55f),
				F("fStormAmplitudeScale", "Wave height in a storm", "Wave height in a gale.", 0.0f, 8.0f, "%.2f", 3.0f),
				F("fSmoothingSeconds", "Smoothing (seconds)", "A weather change eases in over about this long instead of jumping.", 0.0f, 60.0f, "%.0f", 8.0f),
				B("bAffectFlowmapWater", "Rivers respond too", "Water types with Enable Flowmap (rivers) follow the wind as well. Off keeps rivers at their own values.", true),
				B("bTextureFix", "Repair textureless water", "Water materials that came up with no ripple textures borrow them from a loaded material of the same water type.", true),
				B("bDriveStalledScroll", "Move frozen water", "Water materials the engine never set scrolling are scrolled here.", true),
			},
			"WaterWind makes open water respond to the weather's wind: calmer in no wind, choppier and faster in a storm. Values apply to the water "
			"types and the live water materials. The ripple speed and wave height are scales: 1 is the water's own value.");
		return page;
	}

	inline void __stdcall Render() { Page().Render(); }

	inline void Register()
	{
		if (!CswMenu::Smf::Module()) {
			logger::info("No SKSE Menu Framework found; the in-game page is off. Settings are in WaterWind.ini.");
			return;
		}
		const bool ok = CswMenu::Smf::AddSectionItem("CS Water Mod/WaterWind", Render);
		logger::info("SKSE Menu Framework {:.1f}: {}", CswMenu::Smf::Version(), ok ? "WaterWind page added" : "AddSectionItem not exported, page off");
	}
}
