# Nexus release kit: iHarvest NG

Everything to fill in on the Nexus "Add mod" form. The images are in `images/` (regenerate with
`python tools/nexus_art.py`).

## Mod details

| Field | Value |
|---|---|
| Name | `iHarvest NG` |
| Version | `1.0.1` |
| Category | **Gameplay** (alternative: Magic - Spells & Enchantments) |
| Author | `Kexodus` |
| Created by | `Kexodus, original IHarvest by IDontEvenKnow` |
| Language | English |
| Primary image | `images/thumbnail.png` (1280×720) |
| Header image | `images/banner.png` (1920×480) |
| Brief overview | See below (≤ 255 characters) |
| Description | Paste `description.bbcode` |
| Contains adult content | No |

**Brief overview** (220 characters):

> Native SKSE remake of IHarvest: cast Harvest and wisps pick every ingredient around you. Same spell, tomes and staff, but no Papyrus, no quests, zero vanilla overrides. Works with modded flora. One DLL for SE, AE and VR.

## Requirements tab

| Mod | Type | Notes |
|---|---|---|
| SKSE (30379, link the off-site page) | Required | SKSEVR for VR |
| Address Library for SKSE Plugins (32444) | Required | VR Address Library for VR |
| SKSE Menu Framework (120352) | Optional | In-game settings page; INI otherwise |

Also tick **"Mods requiring this file: none"**, and list **IHarvest (27789)** under *Incompatible / alternatives*.

## Files tab

| Field | Value |
|---|---|
| File | `release/iHarvestNG-1.0.1.zip` |
| Category | Main Files |
| Name | `iHarvest NG` |
| Version | `1.0.1` |
| Description | `SE 1.5.97 / AE 1.6.x / VR. Install with a mod manager, enable iHarvestNG.esp.` |

## Permissions tab (suggested)

- **Credits:** IDontEvenKnow (original IHarvest, WTFPL), CommonLibSSE-NG contributors, SKSE team, meh321
  (Address Library), SKSE Menu Framework team.
- **Upload / modify / convert:** yes with credit. The source is GPL-3.0 on GitHub.
- **Assets from the original IHarvest:** WTFPL. The original author has given open permission.
- **Use in paid mods / Creation Club:** no.
- **Donation points:** your choice.

## Tags

`Alchemy`, `Ingredients`, `Spells`, `Magic`, `SKSE`, `Gameplay`, `Immersion`, `VR`, `Quality of Life`

## Changelog (paste into the changelog tab)

- **1.0.1**: Fixed conflicts with world-map, lighting and worldspace mods. The plugin no longer overrides
  any vanilla record, and tomes and staves are placed at runtime instead. Safe to update mid-playthrough.
- **1.0.0**: First release.

## Screenshots to take in-game

The procedural art covers the header, thumbnail and description. Real screenshots sell the mod,
though, so grab these (21:9 or 16:9, UI hidden with `tm` in the console):

1. **Hero shot:** third person, dual casting Harvest in a flower meadow at dusk, with 6+ wisps
   mid-flight. Use Flight speed 1.0 so they're spread out.
2. **The pick:** a close-up of a wisp diving into a mountain flower or a butterfly.
3. **The return:** a wisp heading back to you, with the item notification in the top-left.
4. **Staff of Harvest** in hand with harvesting toggled on.
5. **Spell Tome: Harvest** in one of its spots (Anise's Cabin works well).
6. **The settings page** open in SKSE Menu Framework (both tabs).
7. **Swarm shot:** dual cast plus the Dual Casting perk at max wisps, from a distance.
8. **VR,** if you or a tester can: a hand cast from the headset view.

Tip: `fov 90`, `tfc` (free camera) and `sucsm 5` (slow camera) help with framing.

## Posting checklist

- [ ] Push 1.0.1 to GitHub. The description's images load from the repo, so they must be on `main` first.
- [ ] Create the mod, upload the zip, and paste the description and changelog.
- [ ] Upload `banner.png` as the header and `thumbnail.png` as the primary image. Add the screenshots.
- [ ] Set the requirements and the IHarvest incompatibility.
- [ ] Message IDontEvenKnow with a link (courtesy, not required by WTFPL).
- [ ] Add the Nexus link to the GitHub README.
