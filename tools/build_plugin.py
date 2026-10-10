#!/usr/bin/env python3
"""build_plugin.py - derive iHarvestNG.esp (data only, zero scripts) from the original IHarvest.esl.

iHarvest NG moves every bit of logic into the SKSE plugin, so its plugin only has to carry the
*things players touch*: the Harvest spell + spell tome, the Staff of Harvest (+ enchantment, recipe,
cast visuals), and the art/sound records the native wisps use. Everything Papyrus-related from the
original (8 quests, 65 AI packages, story-manager nodes, the critter actors/race, 66 pathing markers,
form lists, globals, messages) is dropped, and every VMAD (script attachment) is stripped.

The original is WTFPL (see README.md). Usage:

    python -I tools/build_plugin.py <path/to/original/IHarvest.esl> [-o dist/Data/iHarvestNG.esp]

Pure stdlib. The output is deterministic (same input -> byte-identical output).
"""

from __future__ import annotations

import argparse
import struct
import sys
import zlib
from dataclasses import dataclass, field
from pathlib import Path

# Local form IDs (low 24 bits) of the original records we keep. Everything else is dropped.
KEEP = {
	0x8CC,  # KYWD IH_IsHarvestStaff
	0x818,  # SOUN IH_SFXGetterCritterDespawn
	0x804,  # MGEF IH_HarvestEffect            (spell effect, script stripped)
	0x8C6,  # MGEF IH_HarvestStaffEffect       (staff ability effect, script stripped)
	0x8CA,  # MGEF IH_HarvestStaffDummyEffect  (what the staff enchantment "fires")
	0x8C7,  # ENCH IH_HarvestStaffEnch
	0x800,  # SPEL IH_HarvestSpell
	0x8CB,  # SPEL IH_HarvestStaffAbility      (toggled on/off natively when the staff is cast)
	0x819,  # BOOK IH_SpellTomeHarvest         (script stripped; despawn logic is native)
	0x8D5,  # LIGH IH_StaffSpellLight
	0x8C5,  # WEAP IH_HarvestStaff             (script stripped; equip logic is native)
	0x8CF,  # COBJ IH_DLC2RecipeStaffHarvest
	0x8D6,  # PROJ IH_StaffDummySpellProj
	0x80E,  # ARTO IH_FXGetterCritterSpawnPoof
	0x810,  # ARTO IH_FXGetterCritterMeshObject
	0x8D4,  # ARTO IH_StaffCastEffect
	0x816,  # SNDR IH_GetterCritterLPM         (wisp loop)
	0x817,  # SNDR IH_SFXGetterCritterDespawnSD
}
# Base objects whose placed references (spell tomes, staves) are re-created at runtime by the DLL.
KEEP_PLACED_BASES = {0x819, 0x8C5}

# New records (local IDs above the original's highest, 0x8EC).
NEW_SOUN_WISP_LOOP = 0x8F0
NEW_ACTI_WISP = 0x8F1
NEW_ACTI_WISP_SILENT = 0x8F2

SELF_INDEX = 4  # Skyrim, Update, Dawnguard, Dragonborn -> this file is index 04 in its own form IDs
MASTERS = ["Skyrim.esm", "Update.esm", "Dawnguard.esm", "Dragonborn.esm"]

TOP_ORDER = ["KYWD", "SOUN", "MGEF", "ENCH", "SPEL", "ACTI", "BOOK", "LIGH", "WEAP", "COBJ", "PROJ",
             "ARTO", "SNDR"]

REC_FLAG_COMPRESSED = 0x40000
TES4_FLAG_LIGHT = 0x200


@dataclass
class Record:
	sig: str
	flags: int
	formid: int
	vc: int
	ver: int
	unk: int
	subs: list[tuple[str, bytes]]

	@property
	def local(self) -> int:
		return self.formid & 0xFFFFFF if (self.formid >> 24) == SELF_INDEX else -1

	def edid(self) -> str:
		return next((d.rstrip(b"\0").decode("latin1") for s, d in self.subs if s == "EDID"), "")


@dataclass
class Group:
	label: bytes
	gtype: int
	stamp: int
	unk: int
	children: list = field(default_factory=list)


def parse_subs(data: bytes) -> list[tuple[str, bytes]]:
	subs, i, big = [], 0, None
	while i < len(data):
		sig = data[i:i + 4].decode("latin1")
		size = struct.unpack_from("<H", data, i + 4)[0]
		i += 6
		if sig == "XXXX":
			big = struct.unpack_from("<I", data, i)[0]
			i += size
			continue
		if big is not None:
			size, big = big, None
		subs.append((sig, data[i:i + size]))
		i += size
	return subs


def parse(buf: bytes, off: int, end: int) -> list:
	out = []
	while off < end:
		sig = buf[off:off + 4].decode("latin1")
		if sig == "GRUP":
			size, label, gtype, stamp, unk = struct.unpack_from("<I4siII", buf, off + 4)
			g = Group(label, gtype, stamp, unk)
			g.children = parse(buf, off + 24, off + size)
			out.append(g)
			off += size
		else:
			size, flags, formid, vc, ver, unk = struct.unpack_from("<IIIIHH", buf, off + 4)
			data = buf[off + 24:off + 24 + size]
			if flags & REC_FLAG_COMPRESSED:
				data = zlib.decompress(data[4:])
				flags &= ~REC_FLAG_COMPRESSED
			out.append(Record(sig, flags, formid, vc, ver, unk, parse_subs(data)))
			off += 24 + size
	return out


def ser_subs(subs: list[tuple[str, bytes]]) -> bytes:
	out = bytearray()
	for sig, data in subs:
		if len(data) > 0xFFFF:
			out += b"XXXX" + struct.pack("<HI", 4, len(data))
			out += sig.encode("latin1") + struct.pack("<H", 0) + data
		else:
			out += sig.encode("latin1") + struct.pack("<H", len(data)) + data
	return bytes(out)


def ser(node) -> bytes:
	if isinstance(node, Group):
		body = b"".join(ser(c) for c in node.children)
		return b"GRUP" + struct.pack("<I4siII", 24 + len(body), node.label, node.gtype, node.stamp, node.unk) + body
	data = ser_subs(node.subs)
	return node.sig.encode("latin1") + struct.pack("<IIIIHH", len(data), node.flags, node.formid, node.vc, node.ver, node.unk) + data


def count(nodes) -> int:
	n = 0
	for c in nodes:
		n += 1
		if isinstance(c, Group):
			n += count(c.children)
	return n


def zstr(s: str) -> bytes:
	return s.encode("latin1") + b"\0"


def rename_edid(rec: Record) -> None:
	rec.subs = [(s, zstr("IHNG_" + d.rstrip(b"\0").decode("latin1")[3:]) if s == "EDID" and d.startswith(b"IH_") else d)
	            for s, d in rec.subs]


def strip_scripts(rec: Record) -> None:
	rec.subs = [(s, d) for s, d in rec.subs if s != "VMAD"]


def resolve(formid: int) -> tuple[str, int]:
	idx = formid >> 24
	return (MASTERS[idx] if idx < len(MASTERS) else "iHarvestNG.esp"), formid & 0xFFFFFF


def collect_placements(nodes: list, out: list[dict], cell: Record | None = None, world: int = 0) -> None:
	for node in nodes:
		if isinstance(node, Group):
			collect_placements(node.children, out, cell, world)
			continue
		if node.sig == "WRLD":
			world = node.formid
		elif node.sig == "CELL":
			cell = node
		elif node.sig == "REFR" and cell is not None:
			d = dict(node.subs)
			base = struct.unpack("<I", d["NAME"])[0]
			if (base >> 24) != SELF_INDEX or (base & 0xFFFFFF) not in KEEP_PLACED_BASES:
				continue
			cell_flags = struct.unpack("<H", dict(cell.subs).get("DATA", b"\0\0")[:2])[0]
			interior = bool(cell_flags & 1)
			owner = struct.unpack("<I", d["XOWN"][:4])[0] if "XOWN" in d else 0
			out.append({
				"base": base & 0xFFFFFF,
				"cell": resolve(cell.formid),
				"world": resolve(world) if not interior else ("", 0),
				"data": struct.unpack("<6f", d["DATA"]),
				"owner": resolve(owner) if owner else ("", 0),
				"note": cell.edid() or f"{cell.formid:08X}",
			})


def write_placements_inl(placements: list[dict], path: Path) -> None:
	lines = [
		"// Generated by tools/build_plugin.py from the original IHarvest.esl - do not edit.",
		"// The original hand-placed Spell Tomes / Staves of Harvest, re-created at runtime when their cell",
		"// loads, so iHarvestNG.esp never has to override a vanilla cell or worldspace record.",
		"// { base local id, cell plugin, cell local id, world plugin, world local id, pos xyz, rot xyz, owner plugin, owner local id }",
	]
	for p in placements:
		x, y, z, rx, ry, rz = p["data"]
		lines.append(
			f'{{ 0x{p["base"]:03X}, "{p["cell"][0]}"sv, 0x{p["cell"][1]:06X}, "{p["world"][0]}"sv, 0x{p["world"][1]:06X}, '
			f'{{ {x:.4f}f, {y:.4f}f, {z:.4f}f }}, {{ {rx:.6f}f, {ry:.6f}f, {rz:.6f}f }}, '
			f'"{p["owner"][0]}"sv, 0x{p["owner"][1]:06X} }},  // {p["note"]}')
	path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def find_dangling(rec: Record, kept: set[int]) -> list[str]:
	issues = []
	for sig, data in rec.subs:
		if sig in ("EDID", "FULL", "DESC", "DNAM", "MODL", "ANAM", "MODT", "OBND"):
			continue
		for i in range(0, len(data) - 3, 4):  # form IDs sit on 4-byte boundaries in every kept subrecord
			v = struct.unpack_from("<I", data, i)[0]
			if (v >> 24) == SELF_INDEX and 0x800 <= (v & 0xFFFFFF) <= 0xFFF and (v & 0xFFFFFF) not in kept:
				issues.append(f"{rec.sig} {rec.edid()} {sig}+{i}: {v:08X}")
	return issues


def new_record(sig: str, local: int, subs: list[tuple[str, bytes]]) -> Record:
	return Record(sig, 0, (SELF_INDEX << 24) | local, 0, 44, 0, subs)


def build(src: Path) -> tuple[bytes, list[str]]:
	buf = src.read_bytes()
	nodes = parse(buf, 0, len(buf))
	log: list[str] = []

	tops: dict[str, list] = {}
	placements: list[dict] = []
	for node in nodes[1:]:
		if not isinstance(node, Group) or node.gtype != 0:
			raise SystemExit("unexpected top-level layout")
		label = node.label.decode("latin1")
		if label in ("CELL", "WRLD"):
			# NG never overrides vanilla cells/worldspaces (they carry world-map, lighting and water data
			# other mods edit). The placed tomes/staves are re-created at runtime by the DLL instead.
			collect_placements(node.children, placements)
			continue
		kept = [r for r in node.children if isinstance(r, Record) and r.local in KEEP]
		if kept:
			tops[label] = kept

	all_records = [r for children in tops.values() for r in _iter_records(children)]
	for r in all_records:
		strip_scripts(r)
		rename_edid(r)
		if r.local == 0x804:  # was "IH Harvest Effect" - shown in the Active Effects list while casting
			r.subs = [(s, zstr("Harvest") if s == "FULL" else d) for s, d in r.subs]

	# --- New records --------------------------------------------------------------------------------
	soun = new_record("SOUN", NEW_SOUN_WISP_LOOP, [
		("EDID", zstr("IHNG_SFXWispLoop")),
		("OBND", bytes(12)),
		("SDSC", struct.pack("<I", (SELF_INDEX << 24) | 0x816)),
	])
	# The wisp: a collision-less, unnamed activator carrying the original's animated witchlight mesh and
	# its looping hum. Moved every frame by the plugin; never selectable (no collision, no name).
	acti = new_record("ACTI", NEW_ACTI_WISP, [
		("EDID", zstr("IHNG_Wisp")),
		("OBND", struct.pack("<6h", -16, -16, -16, 16, 16, 16)),
		("MODL", zstr("IDEK\\GetterCritter\\witchlightmesh.nif")),
		("SNAM", struct.pack("<I", (SELF_INDEX << 24) | NEW_SOUN_WISP_LOOP)),
		("FNAM", struct.pack("<H", 0x0003)),  # No Displacement | Ignored by Sandbox
	])
	acti_silent = new_record("ACTI", NEW_ACTI_WISP_SILENT, [(s, d) for s, d in acti.subs if s != "SNAM"])
	acti_silent.subs[0] = ("EDID", zstr("IHNG_WispSilent"))
	tops.setdefault("SOUN", []).append(soun)
	tops.setdefault("ACTI", []).extend([acti, acti_silent])

	kept_ids = KEEP | {NEW_SOUN_WISP_LOOP, NEW_ACTI_WISP, NEW_ACTI_WISP_SILENT}
	for r in _iter_records([x for v in tops.values() for x in v]):
		for issue in find_dangling(r, kept_ids):
			log.append("WARNING dangling: " + issue)

	# --- Header ---------------------------------------------------------------------------------------
	groups = []
	for label in TOP_ORDER:
		if label in tops:
			groups.append(Group(label.encode("latin1"), 0, 0, 0, tops[label]))
	n = count(groups)
	header_subs = [
		("HEDR", struct.pack("<fII", 1.71, n, 0xFFF)),
		("CNAM", zstr("iHarvest NG contributors (original by IDontEvenKnow)")),
		("SNAM", zstr("iHarvest NG - native SKSE reimplementation of IHarvest. Data only: no scripts, no quests. "
		              "Requires iHarvestNG.dll (SKSE).")),
	]
	for m in MASTERS:
		header_subs += [("MAST", zstr(m)), ("DATA", bytes(8))]
	header = Record("TES4", TES4_FLAG_LIGHT, 0, 0, 44, 0, header_subs)

	for r in _iter_records([x for v in tops.values() for x in v]):
		if (r.formid >> 24) != SELF_INDEX:
			raise SystemExit(f"refusing to write an override of a master record: {r.sig} {r.formid:08X}")

	out = ser(header) + b"".join(ser(g) for g in groups)
	log.append(f"wrote {n} records+groups; kept {len(all_records)} original records (+3 new), 0 master overrides")
	log.append(f"collected {len(placements)} tome/staff placements for runtime placement")
	return out, log, placements


def _iter_records(nodes):
	for n in nodes:
		if isinstance(n, Group):
			yield from _iter_records(n.children)
		else:
			yield n


def main() -> int:
	ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	ap.add_argument("source", type=Path, help="original IHarvest.esl (or .esp)")
	ap.add_argument("-o", "--output", type=Path, default=Path(__file__).resolve().parents[1] / "dist/Data/iHarvestNG.esp")
	args = ap.parse_args()
	data, log, placements = build(args.source)
	args.output.parent.mkdir(parents=True, exist_ok=True)
	args.output.write_bytes(data)
	inl = Path(__file__).resolve().parents[1] / "src/plugin/Placements.inl"
	write_placements_inl(placements, inl)
	log.append(f"-> {inl}")
	for line in log:
		print(line)
	print(f"-> {args.output} ({len(data)} bytes)")
	return 1 if any(l.startswith("WARNING") for l in log) else 0


if __name__ == "__main__":
	sys.exit(main())
