# CS Water Mod

Community Shaders optimized water overhaul for Skyrim SE/AE (Nexus: https://www.nexusmods.com/skyrimspecialedition/mods/172959).
Inspired by TB's Improved Water (TheBlenderAnimator).

This repository is the working backup of the mod: the plugins, textures, meshes, sounds and shader overrides, plus the SKSE plugins and helper scripts used while tuning it.

## State (2026-10-19, end of the night)

This is the test-list version, ahead of the Nexus release. Everything below was checked in game except where noted.

### Plugins
- `CS Water Mod.esp` (ESL) and `CS water Clean Needs Patch.esp`.
- Cells that were hand-set to custom water types were compared against the game's own water (Update.esm winning); most were put back on vanilla types, then the water types were recoloured with the CS colours. Where cells dropped their water type (which made them fall back to DefaultWater), it was restored, and neighbouring cells were then made consistent.
- Seams fixed so far: Morthal marsh, Riverwood lake, the (31,3)-(31,7) waterfall column, (25,-12)/(25,-11), (-21,20), (-9,26), (-27..-25,18), the NE sea block (cells x39-43, y12-17, now DefaultWater, 11 placed ice floes removed), and the river mouth at (38,14)/(39,14) (new type `CSWRiverMouthBlend`, halfway between the river and the sea).
- Flow tiles `textures/water/skyrim.esm/flow.0.-15.dds` and `flow.0.-14.dds` have a blended left edge (river speed ramps up from the lake's still water). They are uncompressed DDS, the originals were BC7.
- `DefaultWater` (ocean): textures set to oceanwater x2 + CSWCalmWater, WaterFresnel 0.4, SpecularSunSparkleMagnitude 0.25. These are test values.
- Not yet fixed: other placed cover-up objects remain at (18,-17), (18,-18), (0,-14), (42,-24), (42,-25), (7,-8), (8,-6), (2,-13). LOD water direction (see below).

### SKSE plugins (`skse-plugins/`)
Built with alandtse's CommonLibSSE-NG for Skyrim 1.6.1170 and 1.7.x (SE/AE). Each folder holds `src/`, the `ng.cmake` that builds it (included from a parent CMake project that defines `ng_plugin()`), and the built DLL and ini as installed.
- **WaterWind**: scales water scroll speed and wave height with the weather's wind, repairs per-cell water materials that come up with no normal textures, drives scroll on materials the engine never sets up, and does the same for the wading displacement patch around the player (which stopped the tile at the player's feet freezing). **The shipped `WaterWind.ini` is in test state** (forced storm wind: fCalmWind/fReferenceWind/fStormWind 0.01/0.02/0.03). The defaults in the source are 0.05/0.12/0.40.
- **WaterWade**: wading slowdown by depth with armor swim penalties, the engine's wake patch length scaling with speed, and a speed-scaled splash (the game's own splash objects placed at the water surface). The wake-height setting (`bWakeHeight`) has no visible effect and is off.
- **WaterProbe**: read-only diagnostic logger for water objects, materials, scroll and cell data.

### Helper scripts (`dev-tools/`)
Python scripts used to compare cell water against the official masters, apply cell edits to Spriggit YAML, and blend flow tiles. They contain local paths and are kept for reference.

## Next
- LOD water: the LOD water streaks run perpendicular to the real water; rotate it 90 degrees.
- A few toggleable options for the SKSE plugins.
- Support for Open Shaders, PXLR and Bottle Shader (renderer notes for Open Shaders / ENB / vanilla are still to be written).
- Remaining seams in the cells listed above; regenerate AnvilLOD afterwards.

## Credits
TheBlenderAnimator (TB's Improved Water), the Community Shaders team, alandtse (CommonLibSSE-NG).
