#!/usr/bin/env python3
"""build.py - render the IHarvest NG Nexus art into presskit/nexus/.

Run from the repo root after presskit/src/logo.py (the header and primary image use its icon):

    python presskit/src/logo.py
    python presskit/src/build.py

Flat near-black, bone-white type, one gold accent. No frames, gradients, taglines or spec badges.
Drawn at 2x and downsampled for clean thin lines. Needs Pillow (dev-only): python -m pip install pillow

Fonts (SIL OFL, see presskit/src/fonts/*-OFL.txt): Cinzel (titles), Jost (everything else).
"""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = Path(__file__).resolve().parent / "fonts"
LOGO = ROOT / "presskit/logo/ihng-icon-transparent.png"
SS = 2  # supersampling factor

BG = (13, 13, 12)            # shared with logo.py
INK = (232, 227, 216)        # bone white
INK_DIM = (150, 145, 136)
INK_FAINT = (60, 58, 55)
GOLD = (196, 178, 136)       # the one accent


def font(name: str, size: int, weight: str = "Regular") -> ImageFont.FreeTypeFont:
	f = ImageFont.truetype(str(FONTS / f"{name}.ttf"), size * SS)
	f.set_variation_by_name(weight)
	return f


class Canvas:
	"""Flat drawing surface; coordinates are output pixels (scaled internally for supersampling)."""

	def __init__(self, w: int, h: int):
		self.w, self.h = w, h
		self.img = Image.new("RGBA", (w * SS, h * SS), BG + (255,))
		self.d = ImageDraw.Draw(self.img)

	@staticmethod
	def s(v: float) -> float:
		return v * SS

	def line(self, x0, y0, x1, y1, color=INK_FAINT):
		self.d.line([(self.s(x0), self.s(y0)), (self.s(x1), self.s(y1))], fill=color, width=SS)

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

	def icon(self, x, height) -> float:
		"""The logo icon, left edge at x, standing on the bottom edge (the arm comes into frame from
		below, as in the square icon). Returns its width."""
		with Image.open(LOGO) as src:
			w = src.width * height / src.height
			icon = src.convert("RGBA").resize((int(w * SS), int(height * SS)), Image.LANCZOS)
		self.img.alpha_composite(icon, (int(self.s(x)), int(self.s(self.h - height))))
		return w

	@staticmethod
	def icon_width(height) -> float:
		with Image.open(LOGO) as src:
			return src.width * height / src.height

	def orb(self, cx, cy, r):
		"""The small gold orb mark from the logo: a disc and a thin halo."""
		s = self.s
		self.d.ellipse([s(cx - r), s(cy - r), s(cx + r), s(cy + r)], fill=GOLD)
		h = r * 1.55
		self.d.ellipse([s(cx - h), s(cy - h), s(cx + h), s(cy + h)], outline=GOLD, width=SS)

	def save(self, path: Path):
		out = self.img.resize((self.w, self.h), Image.LANCZOS).convert("RGB")
		# Flat art: a 256-colour palette is visually lossless here and much smaller.
		out.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).save(path, optimize=True)


NAME = "IHARVEST NG"


def lockup(c: Canvas, icon_h: float, gap: float, size: int, tracking: float):
	"""Icon on the left, name on the right, centred as a pair."""
	f = font("Cinzel", size)
	tw = c.text_width(NAME, f, tracking)
	iw = c.icon_width(icon_h)
	x = (c.w - (iw + gap + tw)) / 2
	c.icon(x, icon_h)
	c.text(x + iw + gap, c.h / 2 + size * 0.05, NAME, f, INK, tracking=tracking, anchor="lm")


def header(out: Path):
	c = Canvas(1920, 480)
	lockup(c, 440, 90, 92, 0.14)
	c.save(out / "ihng-header-1920x480.png")


def primary(out: Path):
	c = Canvas(1920, 1080)
	lockup(c, 840, 120, 100, 0.12)
	c.save(out / "ihng-primary-1920x1080.png")


ROWS = [
	("Papyrus scripts and quests", "No scripts"),
	("Needs patches", "No patches"),
	("Edits vanilla cells", "Edits nothing"),
	("MCM", "SKSE Menu Framework"),
]


def gallery_comparison(out: Path):
	c = Canvas(1920, 1080)
	tf = font("Cinzel", 64)
	left, right, gap = "IHARVEST", NAME, 150
	lw, rw = c.text_width(left, tf, 0.12), c.text_width(right, tf, 0.12)
	start = c.w / 2 - (lw + gap + rw) / 2
	ty = 250
	c.text(start + lw / 2, ty, left, tf, INK_DIM, tracking=0.12)
	c.text(start + lw + gap + rw / 2, ty, right, tf, INK, tracking=0.12)
	ax0, ax1 = start + lw + 40, start + lw + gap - 40
	c.line(ax0, ty, ax1, ty, GOLD)
	c.d.polygon([(c.s(ax1 + 2), c.s(ty)), (c.s(ax1 - 10), c.s(ty - 6)), (c.s(ax1 - 10), c.s(ty + 6))], fill=GOLD)

	xl, xr = start + lw / 2, start + lw + gap + rw / 2
	body = font("Jost", 36, "Light")
	row_h, top = 120, 420
	for i, (a, b) in enumerate(ROWS):
		y = top + i * row_h
		c.text(xl, y, a, body, INK_DIM)
		c.text(xr, y, b, body, INK)
		if i < len(ROWS) - 1:
			c.line(330, y + row_h / 2, c.w - 330, y + row_h / 2)
	c.save(out / "ihng-gallery-comparison-1920x1080.png")


SECTIONS = ["Features", "How It Works", "Requirements", "Installation", "Settings", "Compatibility", "FAQ", "Credits"]


def section_headers(out: Path):
	f = font("Cinzel", 34)
	for i, name in enumerate(SECTIONS, 1):
		c = Canvas(1200, 112)
		c.orb(40, c.h / 2, 11)
		c.text(84, c.h / 2 + 1, name.upper(), f, INK, tracking=0.24, anchor="lm")
		c.save(out / f"section-{i:02d}-{name.lower().replace(' ', '-')}.png")


def main():
	ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	ap.add_argument("-o", "--out", type=Path, default=ROOT / "presskit/nexus")
	args = ap.parse_args()
	if not LOGO.exists():
		raise SystemExit(f"{LOGO} missing: run presskit/src/logo.py first")
	args.out.mkdir(parents=True, exist_ok=True)
	for old in args.out.glob("*.png"):
		old.unlink()
	header(args.out)
	primary(args.out)
	gallery_comparison(args.out)
	section_headers(args.out)
	for p in sorted(args.out.glob("*.png")):
		with Image.open(p) as im:
			print(f"{p.name:40s} {im.size[0]}x{im.size[1]}  {p.stat().st_size // 1024} KB")


if __name__ == "__main__":
	main()
