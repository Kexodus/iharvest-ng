# Changelog

## 1.0.8 (2026-10-11)

- **Fix: disabled itself on Skyrim AE 1.7.99 / 1.7.104** ("Main::Update call site ... is not a CALL").
  The main-loop call it hooked moved in 1.7. On 1.7.99+ the per-frame update now hooks
  PlayerCharacter::Update (a vtable slot, no code offset). SE, AE 1.6.x and VR keep the Main::Update
  hook; on SE/AE an unexpected call site now falls back to the player hook instead of disabling the mod.

## 1.0.7 (2026-10-11)

- **Fix: wisps sent to targets they could never reach.** The search radius (up to ~32,000 units with
  a high Search radius multiplier) is now capped at 10,000, inside the 12,000-unit range at which a wisp
  vanishes. Long or slow trips (low Flight speed multiplier) get a longer lifetime instead of timing
  out at 25 s. Before, both cases cost magicka and XP for every wisp without harvesting anything.
- **Fix:** running out of magicka (or staff charge) no longer restarts the cast every frame while the
  effect is still winding down.
- **Fix:** Spell Tomes / Staves are no longer placed under the loading screen right after a save loads;
  the once-a-second check places them as soon as you're back in control.
- **Fix:** a Spell Tome you drop yourself no longer has an 80% chance to vanish; only the placed tomes
  (recognised by their exact spot) roll, as in the original.
- Harvestable types are only learned from loaded, enabled references, so an unloaded one can't mark a
  whole plant type "not harvestable" until Reset.
- The save now records placed tomes/staves by a stable id instead of a table position (old saves are
  read as before), and ignores a corrupt co-save's impossible item counts. Quitting with wisps in flight
  no longer touches the game during shutdown.

## 1.0.6 (2026-10-10)

- The log now lists every plugin that edits the Tamriel worldspace record, which holds the world map's
  camera and bounds (`Forms: Tamriel worldspace record ... edited by: ...`). The last file listed
  decides how the map behaves; send this line when reporting world map problems.
- Spell Tomes / Staves are never placed while the game is paused (map, menus); they are placed within a
  second of returning to the game. Placement log lines now include the cell and worldspace IDs.

## 1.0.5 (2026-10-10)

- **New: warns when an old IHarvest plugin breaks the world map.** The 1.0.0 `IHarvestNG.esp` (and the
  original IHarvest.esl and its forks) carry a copy of the Tamriel worldspace record that reverts world
  map mods. If one of them is still installed (for example the old `IHarvestNG-1.0.0` mod left in MO2
  next to the new `IHarvest NG` mod, and winning the file conflict), IHarvest NG now names it in a
  message box on load, in the log and on its Menu Framework page. Remove the old mod to fix the map.

## 1.0.4 (2026-10-10)

- **Fix: Spell Tomes and Staves of Harvest never appeared (1.0.1 to 1.0.3).** The placement table
  looked its forms up with a typed lookup that only matches an exact record type, so all 22 placements
  were rejected at startup (the log said `Placements: 0/22 ... resolved`). They now resolve, and
  existing saves get every tome and staff the next time you enter its cell.
- The release archive is now always named `IHarvest NG.zip`, so a manual install in MO2 offers to
  replace the existing mod.

## 1.0.3 (2026-10-10)

- **Fix: Respect ownership skipped things you're allowed to take.** Items the game lets you *take*
  rather than *steal* (for example in the Blue Palace once you're its steward) were still left alone,
  because an extra owner/faction test copied from the original disagreed with the game. For the
  player, the game's own stealing check is now the only test, so wisps take exactly what you could
  take yourself without stealing. NPC casters are unchanged.

## 1.0.2 (2026-10-10)

- **Fix: Spell Tomes and Staves of Harvest missing** in interiors you enter through a load door, such as
  Blackreach and Anise's Cabin. 1.0.1 placed them only when the game reported a cell as fully loaded,
  and that report can arrive before the cell is actually in the world. Loaded cells are now also
  re-checked once a second, so every placement lands. Existing saves pick up any that were missed.
- The log now records how many placements a save already has, and when a tome vanishes because you
  already know the spell. That is intended: as in the original, 80% of the remaining tomes disappear
  once you've learned Harvest. Staves never disappear.

## 1.0.1 (2026-10-10)

- **Fix: conflicts with world map, lighting and worldspace mods.** IHarvestNG.esp no longer overrides
  any vanilla cell or worldspace record. 1.0.0 inherited full copies of the Tamriel, Solstheim, Soul
  Cairn and Deepwood Redoubt worldspace records and 19 vanilla cells from the original IHarvest, only
  to hold the placed tomes and staves. Loaded late, those copies could revert other mods' changes,
  for example breaking the world map in large load orders.
- The 18 Spell Tomes and 4 Staves of Harvest are now placed by the DLL the first time their cell
  loads. They use the same positions and the same owner, and stay where they're put.
- **Upgrading from 1.0.0 mid-playthrough is safe.** Any tome or staff you already picked up stays in
  your inventory. Ones you never found reappear at their original spots.
- The plugin is now 4.6 KB and contains only new records.

## 1.0.0 (2026-10-10)

First release.

- **Native rewrite:** a complete native SKSE reimplementation of IHarvest 1.2.6 with no Papyrus. It
  keeps the same spell, staff, tome placements, skill-scaled timing and radius, and magicka/charge
  per wisp.
- **One DLL** for Skyrim SE 1.5.97, AE 1.6.x (including 1.6.1130+) and VR.
- **Data-only plugin:** `IHarvestNG.esp` is ESL-flagged, contains no scripts and no quests, and is
  built from the original's records.
- **Harvests as the caster:** Green Thumb, Hearthfire planters, critters, nirnroot and modded
  harvestables work without the original's Vanilla Fixes patch.
- **Modded ingredients:** works with flora and ingredients from any mod.
- **Save cleanup:** wisps saved mid-flight are cleaned up on load.
- **NG extras:**
  - The Alteration Dual Casting perk doubles wisps per refire when dual casting.
  - Wisp flight speed can go up to 20×.
  - Search radius multiplier.
  - Optional arrow/bolt collection.
  - The wisp hum can be turned off.
- **Settings:** a SKSE Menu Framework settings and status page, with an INI fallback.
