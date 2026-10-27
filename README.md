# CS Water Mod

Community Shaders optimized water overhaul for Skyrim SE/AE (Nexus: https://www.nexusmods.com/skyrimspecialedition/mods/172959).
Inspired by TB's Improved Water (TheBlenderAnimator).

This repository is the working backup of the mod: the plugins, textures, meshes, sounds and shader overrides, plus the SKSE plugins and helper scripts used while tuning it. The installer (FOMOD) is in the release zip.

## Version 1.3.0

- The mod no longer ships water flow maps (`textures/water/skyrim.esm`, 749 files). Rivers and streams use the game's own flow maps. The tiles it shipped made the flow along the river from Riverwood to the Guardian Stones slow and broken.
- `CS Water Mod.esp` (ESL), still one plugin for Special Edition and Anniversary Edition. Cells (-24,12), (11,-2), (24,-3), (20,-6), (7,-10), (2,-11), (1,-12), (3,-14), (2,-14), (-5,-12), (-6,-12) and (0,-16) are now covered by the plugin. The Solstheim worldspace record with its four cells and the cells (19,11), (-6,15) and (-7,15) were removed.
- The "water causes damage" flag is no longer set on 8 water types: dunSleepingTreeCampWater, MurkyWater, HelgenWater, ClearInteriorWaterAnkleHeightStill, DefaultIntWaterAnkleHeightStill, ShadowmereWater, HorseTroughWater01 and MarkarthWaterFlow (34 of the 42 keep it).
- Installer: the same options as 1.2.1 (renderer tuning for Community Shaders, PIXL Renderer, Open Shaders or Bottled Shaders, and the optional SKSE plugins); the flow maps are no longer part of the required files. The installer sources are in `fomod/` and the shader folders `Shaders*` of this repository.
- If you installed an older version in place, delete its `textures/water/skyrim.esm` folder, or install this one as a new mod.

## Version 1.2.1

- `CS Water Mod.esp` (ESL) now works on Special Edition and Anniversary Edition alike. The one cell edit that needed the CC Tower mod (Morthal cells 0093BA/0093BB, location only; the water fix there stays) was removed, so there is no separate SE plugin.
- The iNeed and Last Seed style "water causes damage" flag is now part of the main plugin (42 water records), so there is no separate patch plugin and no patch step in the installer.

## Version 1.2.0

### Water plugins
- `CS Water Mod.esp` (ESL) for Anniversary Edition and a Special Edition variant without the one cell edit that needs the CC Tower mod. `CS water Clean Needs Patch.esp` is the optional iNeed patch.
- Cells that were hand-set to custom water types were compared against the game's own water and most were put back on vanilla types, recoloured with the CS colours. Seams were fixed at Morthal marsh, Riverwood lake, the (31,3)-(31,7) waterfall column, (25,-12)/(25,-11), (-21,20), (-9,26), (-27..-25,18), the north-east sea block (cells x39-43, y12-17, 11 placed ice floes removed) and the river mouth at (38,14)/(39,14) (new type `CSWRiverMouthBlend`).
- Flow tiles: 8 tiles that were stored as sRGB were re-encoded as linear, and 11 tile borders got a speed ramp (over 20 pixels) where two flowmap river cells met with different speeds.
- Water sounds per water type: river, creek, lake and ocean each have their own sound, puddles are silent, and all of it is on one audio slider named "CSW-Water" in the game's Audio settings.
- Shader tuning for Community Shaders (default), PIXL Renderer, Open Shaders and Bottled Shaders. Only the Community Shaders option has been checked in game for this release; the other three are carried over from 1.1.6 and are being re-tested.

### SKSE plugins (`skse-plugins/`)
Built with alandtse's CommonLibSSE-NG. They use the Address Library, take no plugin slot, and get a settings page each in SKSE Menu Framework ("CS Water Mod / WaterWind", "CS Water Mod / WaterWade") when that mod is installed; otherwise every setting is a line in the `.ini`.
- **WaterWind**: scales water scroll speed and wave height with the weather's wind, repairs per-cell water materials that come up with no normal textures, drives scroll on water (including the distant LOD water) that the engine leaves frozen, and does the same for the wading displacement patch around the player.
- **WaterWade**: wading slowdown by depth with armor swim penalties, the engine's wake patch growing with speed, speed-scaled splashes, and an experimental foam wake for the ships of Animated Ships (Vicn). The ship wake is off by default; it attaches its own foam mesh to each ship's moving hull node and changes nothing in the ship mod. Wade In Water Redone is detected and WaterWade leaves the slowdown to it.
- `WaterProbe`: read-only diagnostic logger (not part of the release).

Tested on Skyrim 1.7.104. The plugins have not been run on Skyrim VR, SE 1.5.97 or AE versions before 1.6.1170; there is no build for VR or 1.5.97 because the older CommonLibSSE-NG does not have the water and shader structures these plugins read. On those versions install the mod without the SKSE plugins.

### Building the plugins
Each plugin folder has its `ng.cmake`, included from a parent CMake project that defines `ng_plugin()` and adds the CommonLibSSE-NG source of alandtse (`E:/WorkSpace/ng-build` here). `skse-plugins/shared/CswMenu.h` is the SKSE Menu Framework glue (resolved by name at run time, no SDK header). `skse-plugins/WaterWade/mesh/make_wake.py` writes the foam meshes with PyNifly.

### Helper scripts (`dev-tools/`)
Python scripts used to compare cell water against the official masters, apply cell edits to Spriggit YAML, blend flow tiles and set the water sounds. They contain local paths and are kept for reference.

## Known open points
- Cover-up objects still placed by the plugin at (18,-17), (18,-18), (0,-14), (42,-24), (42,-25), (7,-8), (8,-6), (2,-13); river-mouth blends where a flowmap river meets cells without flowmap water.
- The cells (-3,0) and (0,-2) lose their water type when WAVY Waterfalls Effect loads after this mod; load it before.

## License
MIT for the files written for this repository (see `LICENSE.txt`); the compiled plugins link alandtse's CommonLibSSE-NG and are distributed under the GPL, source in this repository. The water plugins, textures, meshes and sounds are not under that license.

## Credits
TheBlenderAnimator (TB's Improved Water), the Community Shaders team, alandtse (CommonLibSSE-NG), Vicn (Animated Ships, only as the target of the optional ship wake), the SKSE Menu Framework authors.
