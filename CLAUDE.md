# iHarvest NG - contributor notes

Native SKSE (CommonLibSSE-NG, one DLL for SE + AE + VR) reimplementation of IHarvest (WTFPL, by
IDontEvenKnow; source at github.com/EyeDeck/IHarvest). Goal: behave like the original, faster, no Papyrus.

## Layout

| Path | What |
|---|---|
| `src/core/` | Engine-independent: `CastTiming` (refire ramp, radius, cost - exact original script math), `Flight` (wisp flight state machine). No CommonLib. Namespace `ihng::core` |
| `tests/` | Catch2 tests for `src/core` (`core-tests` preset, no game needed) |
| `src/plugin/` | SKSE plugin. `Manager` = per-frame orchestrator (sessions, wisps, claims, staff, tomes); `Wisp` = one temp `IHNG_Wisp` ACTI ref; `Harvestable` = classification/availability/harvest (reads vanilla script props from the Papyrus VM); `Hooks` = Main::Update call hook; `Events` = sinks that only enqueue; `UI/Menu` = SKSE Menu Framework page |
| `tools/build_plugin.py` | Regenerates `dist/Data/iHarvestNG.esp` from the original `IHarvest.esl` (keeps spell/tome/staff/art/sound + placed tomes/staves, strips every VMAD, adds `IHNG_Wisp`/`IHNG_WispSilent` ACTI + loop SOUN) |
| `tools/package.py` | Release zip |
| `dist/Data/` | Shipped data (plugin, original meshes/textures/sounds, default INI) |

## Rules

- Game work only on the main thread (inside `Manager::Update`). Event sinks and the Menu Framework render
  thread only call `Manager::QueueTask` / `WatchActor` / `Settings::Set`.
- Harvest by activating the target **as the caster** (`ActivateRef`) - that is what makes vanilla scripts,
  Green Thumb and Hearthfire work without patches. Don't add custom loot logic.
- No extra VFX (absorb glow / spawn poof were removed: CommonLib ApplyArtObject returns a flag, not a pointer, and the poof mesh renders as a white card). Never leave temp refs behind: every wisp ref is tracked, saved in the co-save and deleted on load;
  untracked ones are deleted on cell attach.
- Form IDs come from `iHarvestNG.esp` local IDs (see `Forms.cpp`); keep them stable.
- Before calling a new relocated CommonLib function, check it has a VR address (scratch script approach:
  `skyrim_vr_address_library/database.csv`).

## Build

```
cmake --preset core-tests && cmake --build --preset core-tests && ctest --preset core-tests
cmake --preset plugin && cmake --build --preset plugin-release
```
Needs `VCPKG_ROOT` and VS 2022 Build Tools. Set `IHNG_DEPLOY_DIR` (env or `-D`) to a mod folder to have
every build deployed there. `python -I tools/package.py` makes the release zip.

License: GPL-3.0 (CommonLibSSE-NG is GPL-3.0). Reused IHarvest records/meshes/sounds are WTFPL.
