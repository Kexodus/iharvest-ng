# IHarvest NG Press Kit

Art and copy for the **IHarvest NG** Nexus page. Everything here is loose files. Nothing is zipped.

![Header](nexus/ihng-header-1920x480.png)

## Copy-paste text

| File | Paste into |
|---|---|
| [`nexus-description.bbcode.txt`](nexus-description.bbcode.txt) | Nexus **Description** field (BBCode). Images load from this repo's `main` branch. |
| [`nexus-summary.txt`](nexus-summary.txt) | Nexus **Brief overview** (under 255 characters) |

Every other field is under [Nexus fields](#nexus-fields) below, one copy box each.

## Nexus page images

Upload these on the Nexus **Images** tab. Make the primary image the mod's **Primary image**.

| Image | Size | Use |
|---|---|---|
| [`ihng-primary-1920x1080.png`](nexus/ihng-primary-1920x1080.png) | 1920×1080 | Primary image / thumbnail |
| [`ihng-gallery-comparison-1920x1080.png`](nexus/ihng-gallery-comparison-1920x1080.png) | 1920×1080 | Gallery: original IHarvest vs NG |
| [`ihng-header-1920x480.png`](nexus/ihng-header-1920x480.png) | 1920×480 | Description banner / page header |

<p>
  <img src="nexus/ihng-primary-1920x1080.png" width="49%">
  <img src="nexus/ihng-gallery-comparison-1920x1080.png" width="49%">
</p>

## Description section headers

The BBCode file already uses these. They are 1200×112.

![Features](nexus/section-01-features.png)
![How it works](nexus/section-02-how-it-works.png)
![Requirements](nexus/section-03-requirements.png)
![Installation](nexus/section-04-installation.png)
![Settings](nexus/section-05-settings.png)
![Compatibility](nexus/section-06-compatibility.png)
![FAQ](nexus/section-07-faq.png)
![Credits](nexus/section-08-credits.png)

## Logo files

| File | Notes |
|---|---|
| [`logo/ihng-icon-1024.png`](logo/ihng-icon-1024.png) | Icon on the dark background |
| [`logo/ihng-icon-512.png`](logo/ihng-icon-512.png), [`logo/ihng-icon-256.png`](logo/ihng-icon-256.png) | Avatar / icon sizes |
| [`logo/ihng-icon-transparent.png`](logo/ihng-icon-transparent.png) | Icon only, transparent background |

<img src="logo/ihng-icon-512.png" width="256">

The icon: a hand seen from the side, palm up, holding a Harvest spell (a gold orb with swirls and sparkles),
with a leaf, a mushroom and a flower spiralling into it. The arm runs off the bottom edge, so on the header
and primary image the icon stands on the bottom of the frame. The transparent file is light artwork. Place
it only on dark backgrounds.

## Style

| Token | Value | Use |
|---|---|---|
| Background | `#0d0d0c` | Every canvas |
| Foreground | `#e8e3d8` | Icon, headings, body text |
| Dim | `#969188` | Secondary text, trails |
| Accent | `#c4b288` | Magic only: the spell, its swirls and sparkles. Nothing else is gold. |
| Type | Cinzel (titles), Jost (body) | Uppercase titles with wide letter spacing. Both SIL OFL, in `src/fonts/` |

Keep it flat: no gradients, glows, textures, frames or extra colors. One idea per image, no taglines or spec badges.
The orb mark in the section headers is redrawn from the icon's spell.

## Rebuild

Requires Python 3 with Pillow (`python -m pip install pillow`).

```powershell
python presskit/src/logo.py    # logo/ icon in every size
python presskit/src/build.py   # nexus/ images (uses the icon from logo/)
```

Image copy lives in `presskit/src/build.py`, the icon in `presskit/src/logo.py`. Change them there, rebuild, and
commit the PNGs. Nexus loads the description images from `main`.

## Nexus fields

### Mod name
```text
IHarvest NG
```

### Version
```text
1.0.7
```

### Category
```text
Gameplay
```

### Author
```text
Kexodus
```

### Files tab: main file

**Upload:** [IHarvest.NG.zip](https://github.com/Kexodus/iharvest-ng/releases/download/v1.0.7/IHarvest.NG.zip) (GitHub shows the space as a dot; rename it to `IHarvest NG.zip` if you like). Category: *Main Files*.

File name:
```text
IHarvest NG
```
File version:
```text
1.0.7
```
File description:
```text
Skyrim SE 1.5.97 / AE 1.6.x / VR. Install with a mod manager and enable IHarvestNG.esp. Requires SKSE + Address Library. Do not use together with the original IHarvest.
```

### Requirements tab
Add these as requirements (search by name or ID):

| Mod | Nexus ID | Note to paste |
|---|---|---|
| SKSE | 30379 | `Required (SKSEVR for VR)` |
| Address Library for SKSE Plugins | 32444 | `Required (VR Address Library for VR)` |
| SKSE Menu Framework | 120352 | `Optional - in-game settings page (INI otherwise)` |

### Changelog tab
Version `1.0.7`:
```text
Fixed wisps being sent to targets they could never reach (high search radius or low flight speed cost magicka for nothing). Fixed the cast restarting every frame when out of magicka. Tomes/staves are no longer placed under the loading screen. A Spell Tome you drop yourself no longer vanishes. Sturdier save data.
```
Version `1.0.6`:
```text
Diagnostics for world map reports: the log lists every plugin that edits the Tamriel worldspace (map camera/bounds). Tomes and staves are no longer placed while the game is paused.
```
Version `1.0.5`:
```text
World map broken? IHarvest NG now detects an old IHarvest plugin (the 1.0.0 IHarvestNG.esp, or the original IHarvest / a fork) that still overrides the Tamriel worldspace, and names it in a message box on load. Remove that old mod to fix the map.
```
Version `1.0.4`:
```text
Fixed Spell Tomes / Staves of Harvest never appearing: all 22 placements were rejected at startup. Existing saves get every tome and staff the next time you enter its cell.
```
Version `1.0.3`:
```text
Fixed Respect ownership skipping items the game lets you take without stealing (e.g. in the Blue Palace as its steward).
```
Version `1.0.2`:
```text
Fixed Spell Tomes / Staves of Harvest not appearing in interiors entered through a load door (e.g. Blackreach, Anise's Cabin). Existing saves pick up any missed ones.
```
Version `1.0.1`:
```text
Fixed conflicts with world-map, lighting and worldspace mods: the plugin no longer overrides any vanilla record (tomes and staves are now placed at runtime). Safe to update mid-playthrough.
```
Version `1.0.0`:
```text
First release.
```

### Permissions tab
Credits (the "Credits" text box):
```text
IDontEvenKnow - the original IHarvest (WTFPL): gameplay design, records, wisp meshes and sounds. Please endorse the original too.
vh724 - iHarvest - Redux, the SkyPal-free rework of IHarvest.
CommonLibSSE-NG contributors (alandtse, CharmedBaryon, powerofthree and others).
SKSE team, meh321 (Address Library), SKSE Menu Framework team.
```
Suggested toggles:
- Upload to other sites, modify, convert, use assets: **Yes, with credit** (source is GPL-3.0)
- Use in paid mods / Creation Club: **No**

### Tags
```text
Alchemy, Ingredients, Spells, Magic, SKSE, Gameplay, Immersion, VR, Quality of Life
```

## Still to do yourself
- **In-game screenshots.** Real ones sell the mod best, and I can't take them. Good ones: dual casting in a
  flower meadow with wisps mid-flight, a wisp diving into a flower, the Staff of Harvest, the settings page.
  Use `tm` in the console to hide the UI and `tfc` for a free camera.
- Message IDontEvenKnow with the link (a courtesy, not required).
