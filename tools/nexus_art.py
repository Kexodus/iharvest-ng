#!/usr/bin/env python3
"""nexus_art.py - render the Nexus page art for iHarvest NG in a minimal, Nordic-UI style.

Flat near-black panels, 1 px bone-white rules, notched frames, diamond ornaments and widely tracked
uppercase type. Everything is drawn at 2x and downsampled for clean thin lines.

Needs Pillow + NumPy (dev-only; not part of the mod):  python -m pip install pillow numpy
    python tools/nexus_art.py [-o docs/nexus/images]

Fonts (SIL OFL, see tools/fonts/*-OFL.txt): Cinzel (titles), Jost (everything else).
"""

from __future__ import annotations

import argparse
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / "tools/fonts"
SS = 2  # supersampling factor
# Bumped whenever the art style changes: new file names defeat GitHub/browser image caches.
TAG = "nordic"

BG_CENTER = (24, 24, 23)
BG_EDGE = (10, 10, 10)
INK = (232, 227, 216)        # bone white
INK_DIM = (150, 145, 136)
INK_BODY = (170, 164, 154)
INK_FAINT = (92, 89, 84)
GOLD = (196, 178, 136)
PANEL_ALT = (27, 27, 26)


def font(name: str, size: int, weight: str) -> ImageFont.FreeTypeFont:
	f = ImageFont.truetype(str(FONTS / f"{name}.ttf"), size * SS)
	f.set_variation_by_name(weight)
	return f


class Canvas:
	"""Drawing surface; all coordinates are output pixels (scaled internally for supersampling)."""

	def __init__(self, w: int, h: int, seed: int = 1, vignette: float = 1.0):
		self.w, self.h = w, h
		W, H = w * SS, h * SS
		yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
		d = np.sqrt(((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) / 1.42
		t = np.clip(d * vignette, 0, 1)[..., None] ** 1.6
		img = np.array(BG_CENTER, np.float32) * (1 - t) + np.array(BG_EDGE, np.float32) * t
		img += np.random.default_rng(seed).normal(0, 0.7, (H, W, 1)).astype(np.float32)  # dither against gradient banding
		self.img = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8), "RGB").convert("RGBA")
		self.d = ImageDraw.Draw(self.img)

	@staticmethod
	def s(v: float) -> float:
		return v * SS

	def line(self, x0, y0, x1, y1, color=INK_DIM):
		self.d.line([(self.s(x0), self.s(y0)), (self.s(x1), self.s(y1))], fill=color, width=SS)

	def diamond(self, cx, cy, r, color=INK, fill=None):
		pts = [(self.s(cx), self.s(cy - r)), (self.s(cx + r), self.s(cy)), (self.s(cx), self.s(cy + r)), (self.s(cx - r), self.s(cy))]
		self.d.polygon(pts, outline=color, fill=fill, width=SS)

	def rule(self, cx, cy, half, gap=14, color=INK_DIM):
		"""Nordic UI divider: thin line, gold diamond in the middle, tiny diamonds at the ends."""
		self.line(cx - half, cy, cx - gap, cy, color)
		self.line(cx + gap, cy, cx + half, cy, color)
		self.diamond(cx, cy, 6, GOLD)
		self.diamond(cx, cy, 2.2, GOLD, fill=GOLD)
		for sgn in (-1, 1):
			self.diamond(cx + sgn * half, cy, 2.5, color, fill=color)

	def frame(self, x0, y0, x1, y1, notch=14, inset=7):
		"""Double thin frame with chamfered corners and gold corner studs."""
		for k, col in ((0, INK_FAINT), (inset, (62, 60, 57))):
			a, b, c, e = x0 + k, y0 + k, x1 - k, y1 - k
			n = notch - k * 0.6
			pts = [(a + n, b), (c - n, b), (c, b + n), (c, e - n), (c - n, e), (a + n, e), (a, e - n), (a, b + n), (a + n, b)]
			self.d.line([(self.s(x), self.s(y)) for x, y in pts], fill=col, width=SS)
		for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
			self.diamond(x + (notch * 0.45 if x == x0 else -notch * 0.45), y + (notch * 0.45 if y == y0 else -notch * 0.45), 2.2, GOLD, fill=GOLD)

	def text(self, x, y, s, f, color=INK, tracking=0.0, anchor="mm"):
		"""Text with letter spacing (tracking in em). anchor: 'mm' centre or 'lm' left-middle."""
		extra = tracking * f.size
		widths = [self.d.textlength(ch, font=f) for ch in s]
		total = sum(widths) + extra * (len(s) - 1)
		px = self.s(x) - (total / 2 if anchor[0] == "m" else 0)
		for ch, cw in zip(s, widths):
			self.d.text((px, self.s(y)), ch, font=f, fill=color, anchor="l" + anchor[1])
			px += cw + extra

	def text_width(self, s, f, tracking=0.0) -> float:
		widths = [self.d.textlength(ch, font=f) for ch in s]
		return (sum(widths) + tracking * f.size * (len(s) - 1)) / SS

	def save(self, path: Path):
		out = self.img.resize((self.w, self.h), Image.LANCZOS).convert("RGB")
		# Flat art: a 256-colour palette is visually lossless here and ~4x smaller.
		out.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).save(path, optimize=True)


def title_block(c: Canvas, cy: float, title_size: int, sub_size: int, rule_half: float):
	cx = c.w / 2
	c.text(cx, cy, "IHARVEST NG", font("Cinzel", title_size, "Regular"), INK, tracking=0.16)
	ry = cy + title_size * 0.80
	c.rule(cx, ry, rule_half)
	c.text(cx, ry + sub_size * 2.0, "NATIVE SKSE   ·   SE   ·   AE   ·   VR", font("Jost", sub_size, "Regular"), INK_DIM, tracking=0.32)


def banner(out: Path):
	c = Canvas(1920, 480, seed=3)
	c.frame(40, 40, 1880, 440)
	title_block(c, 200, 104, 22, 300)
	c.save(out / f"banner-{TAG}.png")


def thumbnail(out: Path):
	c = Canvas(1280, 720, seed=5)
	c.frame(36, 36, 1244, 684)
	title_block(c, 316, 96, 22, 270)
	c.text(c.w / 2, 560, "WISPS THAT HARVEST EVERY INGREDIENT AROUND YOU", font("Jost", 15, "Light"), INK_FAINT, tracking=0.30)
	c.save(out / f"thumbnail-{TAG}.png")


ROWS = [
	("LOGIC", "Papyrus: 8 quests, 8 finder threads", "Native C++ in the game loop"),
	("WISPS", "AI actors and 64 pathing markers", "Lightweight refs, curved native flight"),
	("FINDING FLORA", "Story Manager / SkyPal, ~3 s timeouts", "Native scan of loaded cells"),
	("HARVESTING", "Needs Vanilla Fixes / USSEP patches", "Harvests as you, no patches"),
	("SAVE FOOTPRINT", "Quests, actors, stuck-thread risk", "No quests, actors or scripts"),
	("CONFLICTS", "Overrides 4 worldspaces and 19 cells", "Zero vanilla records touched"),
	("VR", "Rename .esl to .esp by hand", "Same DLL, ESL-flagged .esp"),
	("SETTINGS", "SkyUI MCM", "SKSE Menu Framework + INI"),
]


def compare(out: Path):
	w, top, row_h = 1600, 236, 72
	h = top + 46 + len(ROWS) * row_h + 70
	c = Canvas(w, h, seed=9)
	c.frame(30, 30, w - 30, h - 30)
	tf = font("Cinzel", 44, "Regular")
	left, right = "IHARVEST", "IHARVEST NG"
	lw, rw, gap = c.text_width(left, tf, 0.12), c.text_width(right, tf, 0.12), 110
	start = w / 2 - (lw + gap + rw) / 2
	c.text(start + lw / 2, 98, left, tf, INK_DIM, tracking=0.12)
	c.text(start + lw + gap + rw / 2, 98, right, tf, INK, tracking=0.12)
	ax0, ax1 = start + lw + 30, start + lw + gap - 30
	c.line(ax0, 98, ax1, 98, GOLD)
	c.diamond(ax1, 98, 4, GOLD, fill=GOLD)
	c.rule(w / 2, 152, 330)

	x0, x1, x2, x3 = 90, 430, 1010, w - 90
	head = font("Jost", 17, "Medium")
	c.text(x1, top, "ORIGINAL IHARVEST", head, INK_DIM, tracking=0.25, anchor="lm")
	c.text(x2, top, "IHARVEST NG", head, GOLD, tracking=0.25, anchor="lm")
	c.line(x0, top + 26, x3, top + 26, INK_FAINT)

	key = font("Jost", 16, "Medium")
	body = font("Jost", 23, "Light")
	for i, (k, a, b) in enumerate(ROWS):
		y = top + 46 + row_h / 2 + i * row_h
		if i % 2 == 0:
			c.d.rectangle([c.s(x0), c.s(y - row_h / 2 + 4), c.s(x3), c.s(y + row_h / 2 - 4)], fill=PANEL_ALT + (255,))
		c.text(x0 + 22, y, k, key, INK_DIM, tracking=0.22, anchor="lm")
		c.text(x1, y, a, body, INK_BODY, anchor="lm")
		c.diamond(x2 - 18, y, 3.2, GOLD, fill=GOLD)
		c.text(x2, y, b, body, INK, anchor="lm")
	end = top + 46 + len(ROWS) * row_h + 4
	c.line(x0, end, x3, end, INK_FAINT)
	c.save(out / f"comparison-{TAG}.png")


SECTIONS = ["Features", "How It Works", "Requirements", "Installation", "Settings", "Compatibility", "FAQ", "Credits"]


def section_headers(out: Path):
	w, h = 1200, 90
	f = font("Cinzel", 30, "Regular")
	for name in SECTIONS:
		c = Canvas(w, h, seed=zlib.crc32(name.encode()), vignette=0.6)
		label = name.upper()
		tw = c.text_width(label, f, tracking=0.22)
		c.text(w / 2, h / 2 - 2, label, f, INK, tracking=0.22)
		gap = tw / 2 + 34
		for sgn in (-1, 1):
			c.line(w / 2 + sgn * gap, h / 2, w / 2 + sgn * 470, h / 2, INK_FAINT)
			c.diamond(w / 2 + sgn * (gap - 14), h / 2, 4.5, GOLD)
			c.diamond(w / 2 + sgn * 470, h / 2, 2.4, INK_FAINT, fill=INK_FAINT)
		c.save(out / f"section_{name.lower().replace(' ', '_')}-{TAG}.png")


def main():
	ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	ap.add_argument("-o", "--out", type=Path, default=ROOT / "docs/nexus/images")
	args = ap.parse_args()
	args.out.mkdir(parents=True, exist_ok=True)
	banner(args.out)
	thumbnail(args.out)
	compare(args.out)
	section_headers(args.out)
	for p in sorted(args.out.glob("*.png")):
		with Image.open(p) as im:
			print(f"{p.name:32s} {im.size[0]}x{im.size[1]}  {p.stat().st_size // 1024} KB")


if __name__ == "__main__":
	main()
