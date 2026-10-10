# Changelog

## 1.0.0 (2026-10-10)

First release.

- **Native rewrite:** a complete native SKSE reimplementation of IHarvest 1.2.6 with no Papyrus. It
  keeps the same spell, staff, tome placements, skill-scaled timing and radius, and magicka/charge
  per wisp.
- **One DLL** for Skyrim SE 1.5.97, AE 1.6.x (including 1.6.1130+) and VR.
- **Data-only plugin:** `iHarvestNG.esp` is ESL-flagged, contains no scripts and no quests, and is
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
