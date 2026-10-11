#!/usr/bin/env python3
"""nexus_art.py - render the Nexus page art for IHarvest NG in a minimal, Nordic-UI style.

Flat near-black panels, 1 px bone-white rules and widely tracked uppercase type - the name, a rule and
nothing else (no feature badges or version lists). Drawn at 2x and downsampled for clean thin lines.

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
TAG = "min"

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

	def rule(self, cx, cy, half, gap=12, color=INK_FAINT):
		"""Divider: a thin line broken by one small gold diamond."""
		self.line(cx - half, cy, cx - gap, cy, color)
		self.line(cx + gap, cy, cx + half, cy, color)
		self.diamond(cx, cy, 4, GOLD)

	def frame(self, x0, y0, x1, y1, notch=12):
		"""Single thin frame with chamfered corners."""
		n = notch
		pts = [(x0 + n, y0), (x1 - n, y0), (x1, y0 + n), (x1, y1 - n), (x1 - n, y1), (x0 + n, y1), (x0, y1 - n), (x0, y0 + n), (x0 + n, y0)]
		self.d.line([(self.s(x), self.s(y)) for x, y in pts], fill=(52, 50, 48), width=SS)

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


def title_block(c: Canvas, cy: float, title_size: int, rule_half: float):
	cx = c.w / 2
	c.text(cx, cy, "IHARVEST NG", font("Cinzel", title_size, "Regular"), INK, tracking=0.16)
	c.rule(cx, cy + title_size * 0.85, rule_half)


def banner(out: Path):
	c = Canvas(1920, 480, seed=3)
	c.frame(40, 40, 1880, 440)
	title_block(c, 222, 104, 240)
	c.save(out / f"banner-{TAG}.png")


def thumbnail(out: Path):
	c = Canvas(1280, 720, seed=5)
	c.frame(36, 36, 1244, 684)
	title_block(c, 340, 96, 220)
	c.save(out / f"thumbnail-{TAG}.png")


ROWS = [
	("Papyrus scripts and quests", "No scripts"),
	("Needs patches", "No patches"),
	("Edits vanilla cells", "Edits nothing"),
	("MCM", "SKSE Menu Framework"),
]


def compare(out: Path):
	w, top, row_h = 1600, 230, 84
	h = top + len(ROWS) * row_h + 90
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
	c.rule(w / 2, 152, 300)

	# Two columns, centred on the title's halves; thin separators only.
	xl, xr, x0, x3 = start + lw / 2, start + lw + gap + rw / 2, 260, w - 260
	body = font("Jost", 26, "Light")
	for i, (a, b) in enumerate(ROWS):
		y = top + row_h / 2 + i * row_h
		c.text(xl, y, a, body, INK_DIM)
		c.text(xr, y, b, body, INK)
		if i < len(ROWS) - 1:
			c.line(x0, y + row_h / 2, x3, y + row_h / 2, (40, 39, 37))
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
		gap = tw / 2 + 32
		for sgn in (-1, 1):
			c.line(w / 2 + sgn * gap, h / 2, w / 2 + sgn * 420, h / 2, INK_FAINT)
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
