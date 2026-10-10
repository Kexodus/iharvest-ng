# iHarvest NG

A native SKSE reimplementation of **[IHarvest](https://www.nexusmods.com/skyrimspecialedition/mods/27789)**
by IDontEvenKnow. Cast *Harvest* (or wield the *Staff of Harvest*) and little wisps fly out, pick every
ingredient around you, and bring it back.

You get the same spell, the same tomes in the same places, the same staff, skill scaling and
magicka-per-wisp economy. The difference is that all ~3,800 lines of the original's Papyrus are now
C++ running inside the game loop.

**One DLL for Skyrim SE 1.5.97, AE 1.6.x (including 1.6.1130+), and Skyrim VR.**

## Why NG

| | IHarvest (original) | iHarvest NG |
|---|---|---|
| Logic | Papyrus: 8 quests, Story Manager / quest-start searches, 8 finder threads, latent waits | Native C++ in the main loop. No scripts at all |
| Wisps | Pooled **actors** with AI travel packages, plus 64 pathing markers and aliases | Lightweight collision-less refs, moved natively every frame along curved flight paths |
| Finding flora | Story Manager or SkyPal, ~3 s timeouts, "Story Manager under high load" warnings | Direct native scan of the loaded cells. No Papyrus, no quest restarts. The cost is shown live in the status page |
| Save footprint | Persistent critter actors, quests, form lists, and a risk of stuck threads (`PathToReference`) | No quests, actors or scripts. Wisps saved mid-flight are deleted again on load |
| Harvesting | The critter activates the target, so vanilla scripts needed patching ("Vanilla Fixes") and Hearthfire needed USSEP | Harvests *as the caster* through the game's own activation. Green Thumb, harvest sounds, critter / nirnroot / Hearthfire scripts and quest hooks just work, with no patches |
| Conflicts | Full copies of 4 worldspaces and 19 vanilla cells (world map / lighting conflicts in big load orders) | **Zero** vanilla records overridden. Tomes and staves are placed at runtime |
| VR | "Seems to work", after renaming `.esl` to `.esp` by hand | Same DLL. Every engine address it uses is verified against the VR Address Library. `iHarvestNG.esp` is ESL-flagged: light on SE/AE, a normal plugin on VR |
| Config | SkyUI MCM | SKSE Menu Framework page (applies live), plus a plain INI |

## Gameplay

The core gameplay is identical to the original:

* **Spell Tome: Harvest** is hand-placed in the original's 18 locations (Anise's Cabin, Blind Cliff Cave,
  Hob's Fall, Fellglow Keep, Blackreach, Tel Mithryn, Solstheim and more). Once you know the spell,
  80 % of the remaining tomes quietly vanish as you find them.
* **Harvest** is a free Alteration concentration spell. Each wisp it sends costs **8 Magicka**, reduced
  by Alteration cost reduction (never below 15 %).
* The first wisp leaves instantly and the second after 1 s. After that the cast speeds up towards a
  floor set by your **Alteration** and **Alchemy**: 500 ms with no skill, about 330 ms at 100/100.
  The search radius grows from 1500 to 4000 units with the same skills.
* **Dual casting** gives 1.5× radius and a faster ramp.
* If nothing is in range, or the wisp cap is reached, the cast backs off exactly like the original.
* **Staff of Harvest** is placed in 4 locations and can be crafted at a Solstheim staff enchanter.
  Cast it to toggle hands-free harvesting (3000+ unit radius). It drains 12 charge per wisp, and you
  can't dual-wield two of them.
* Each wisp grants 10 Alteration skill XP.
* It respects ownership by default, so it never steals.
* **What gets harvested:**
  * flora and trees that yield ingredients, including those from **any mod**
  * loose ingredients
  * butterflies, moths, dragonflies and fish
  * nirnroot, poison blooms, wisp cores, Creation Club harvestables and Beyond Skyrim wisp stalks
  * optionally, food, Hearthfire planters (on by default), and arrows/bolts
* NPCs who know the spell or wield the staff send wisps too.

**NG extras:**

* With the **Alteration Dual Casting** perk, dual casting sends **two wisps per refire**. Each one costs
  magicka, and you can turn this off.
* Wisp flight speed can be raised up to **20×**.

## Requirements

* [SKSE](https://www.nexusmods.com/skyrimspecialedition/mods/30379) (or SKSEVR)
* [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (SE/AE),
  or VR Address Library for SKSEVR
* Optional: [SKSE Menu Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120352) for the
  in-game settings page

## Install

1. Install the archive with your mod manager.
2. Enable `iHarvestNG.esp`.

Do **not** run it alongside the original IHarvest, or both will hand out a Harvest spell. To switch over:

1. Use the original's MCM **Reset Mod**.
2. Disable and remove the original.
3. Make a clean save.

## Settings

Open SKSE Menu Framework, then **iHarvest NG**:

* **Status & Maintenance** shows live wisp and harvest counts, with *Recall all wisps* and *Reset*.
  A testing section can add the spell or the staff. A *Diagnostics* section shows timings and the log level.
* **Settings** holds every option. Changes apply live and are saved when the menu closes.

Without Menu Framework, edit `Data/SKSE/Plugins/iHarvestNG.ini`, which is documented inline.

Log: `Documents/My Games/Skyrim Special Edition/SKSE/iHarvestNG.log`.

## Building

Requirements: Visual Studio 2022 (MSVC v143), CMake 3.25+, and vcpkg with `VCPKG_ROOT` set.

```
git clone --recursive https://github.com/Kexodus/iharvest-ng
cmake --preset core-tests && cmake --build --preset core-tests && ctest --preset core-tests
cmake --preset plugin && cmake --build --preset plugin-release
python -I tools/package.py   # release zip in release/
```

* `src/core` holds the engine-independent cast-timing and wisp-flight logic. It is unit-tested in
  `tests/` (Catch2).
* `src/plugin` is the SKSE plugin (CommonLibSSE-NG, multi-runtime).
* `tools/build_plugin.py` regenerates `dist/Data/iHarvestNG.esp` from the original `IHarvest.esl`.
  It keeps the spell, tome, staff and sound records, strips every script, and adds the wisp activators.
  Run it with `python -I tools/build_plugin.py path/to/IHarvest.esl`.
* Optional: set `IHNG_DEPLOY_DIR` (environment variable or `-DIHNG_DEPLOY_DIR=...`) to a mod folder,
  such as your MO2 `mods/iHarvest NG`, and every build is copied there.

## Credits & license

* **IHarvest** (the original mod: scripts, plugin, sounds) is © IDontEvenKnow, released under the
  [WTFPL](http://www.wtfpl.net/) ([`LICENSE-IHarvest-WTFPL.txt`](LICENSE-IHarvest-WTFPL.txt)). Its
  meshes and textures are rehashed Bethesda assets. iHarvest NG reuses its records, meshes, textures and
  sounds under that license, and is renamed as the license asks. All credit for the gameplay design
  goes to IDontEvenKnow.
* **iHarvest NG** code is licensed under the **GNU GPL v3** ([`LICENSE`](LICENSE)), because it links
  [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR) (GPL-3.0 with modding exceptions).
* The [SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API) API header is
  LGPL-2.1.
