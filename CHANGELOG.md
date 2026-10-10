# Changelog

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
