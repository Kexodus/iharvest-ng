#!/usr/bin/env python3
"""build.py - render the IHarvest NG Nexus art into presskit/nexus/.

Run from the repo root (the header and primary image draw the icon from presskit/logo/ihng-icon-minimal.svg):

    python presskit/src/cutout.py   # logo/ transparent cut-out
    python presskit/src/build.py    # nexus/ images

Flat near-black, bone-white type, one gold accent. No frames, gradients, taglines or spec badges.
Drawn at 2x and downsampled for clean thin lines. Needs Pillow and Microsoft Edge (dev-only): python -m pip install pillow

Fonts (SIL OFL, see presskit/src/fonts/*-OFL.txt): Cinzel (titles), Jost (everything else).
"""

from __future__ import annotations

import argparse
import sys
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONTS = Path(__file__).resolve().parent / "fonts"
SS = 2  # supersampling factor

BG = (13, 13, 12)            # the icon's background (#0d0d0c), shared with cutout.py
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

	def icon(self, x, y, height) -> float:
		"""The logo icon with its top-left at (x, y), scaled to the given height. Returns its width.
		Pasted opaque from logo.render() on the same background, so the soft glow keeps every shade."""
		src = icon_art()
		w = src.width * height / src.height
		self.img.paste(src.resize((int(w * SS), int(height * SS)), Image.LANCZOS, reducing_gap=3.0), (int(self.s(x)), int(self.s(y))))
		return w

	@staticmethod
	def icon_width(height) -> float:
		src = icon_art()
		return src.width * height / src.height

	def orb(self, cx, cy, r):
		"""The icon's sparkle (its SVG path "M 0 -34 C 4 -8 8 -4 34 0 ..." mirrored into four arms), r = arm length."""
		def cubic(p0, p1, p2, p3, n=16):
			return [tuple((1 - t) ** 3 * a + 3 * (1 - t) ** 2 * t * b + 3 * (1 - t) * t * t * c + t ** 3 * d
			              for a, b, c, d in zip(p0, p1, p2, p3)) for t in (i / n for i in range(n))]
		k = r / 34
		quarter = cubic((0, -34), (4, -8), (8, -4), (34, 0))
		pts = []
		for rot in range(4):  # rotate the quarter by 0/90/180/270 degrees
			for x, y in quarter:
				for _ in range(rot):
					x, y = -y, x
				pts.append((self.s(cx + x * k), self.s(cy + y * k)))
		self.d.polygon(pts, fill=GOLD)

	def save(self, path: Path, palette: bool = True, card: int = 0):
		"""card > 0: save as a description card - rounded corners of that radius, a hairline border and
		a soft shadow on transparent margins, so the dark panel sits on the Nexus page like its own UI."""
		out = self.img.resize((self.w, self.h), Image.BOX).convert("RGB")  # exact 2x: no ringing
		if card:
			as_card(out, card).save(path, optimize=True)
			return
		if palette:
			# Flat art: a 256-colour palette is visually lossless here and much smaller.
			out = out.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
		out.save(path, optimize=True)  # palette=False for images with the spell's glow (a palette bands it)


NAME = "IHARVEST NG"

CARD_BORDER = (52, 51, 49)   # hairline round each card: just lighter than the panel, darker than the page
SHADOW = 0.5                 # card shadow opacity on the page


def as_card(img: Image.Image, radius: int) -> Image.Image:
	w, h = img.size
	k = 4  # supersampled corner mask
	mask = Image.new("L", (w * k, h * k), 0)
	ImageDraw.Draw(mask).rounded_rectangle([0, 0, w * k - 1, h * k - 1], radius * k, fill=255)
	mask = mask.resize((w, h), Image.BOX)
	border = Image.new("L", (w * k, h * k), 0)
	ImageDraw.Draw(border).rounded_rectangle([0, 0, w * k - 1, h * k - 1], radius * k, outline=255, width=int(1.5 * k))
	border = border.resize((w, h), Image.BOX)
	panel = img.copy()
	panel.paste(Image.new("RGB", (w, h), CARD_BORDER), (0, 0), border)
	panel = panel.convert("RGBA")
	panel.putalpha(mask)

	margin = max(12, radius)
	sheet = Image.new("RGBA", (w + 2 * margin, h + 2 * margin), (0, 0, 0, 0))
	shadow = Image.new("L", sheet.size, 0)
	shadow.paste(mask, (margin, margin + margin // 4))
	shadow = shadow.filter(ImageFilter.GaussianBlur(margin / 2.2)).point(lambda v: int(v * SHADOW))
	sheet.paste(Image.new("RGBA", sheet.size, (0, 0, 0, 255)), (0, 0), shadow)
	sheet.alpha_composite(panel, (margin, margin))
	return sheet

_ICON = None


def cutout():
	sys.path.insert(0, str(Path(__file__).resolve().parent))
	import cutout as module
	return module


def icon_art() -> Image.Image:
	"""The icon (presskit/logo/ihng-icon-minimal.svg), rendered large and opaque on BG, cropped to its
	artwork (rendered once)."""
	global _ICON
	if _ICON is None:
		art = cutout().render_svg(cutout().SOURCE, 4096)
		box = ImageChops.difference(art, Image.new("RGB", art.size, BG)).point(lambda v: 255 if v > 2 else 0).getbbox()
		_ICON = art.crop(box)
	return _ICON



def lockup(c: Canvas, icon_h: float, gap: float, size: int, tracking: float):
	"""Icon on the left, name on the right, centred as a pair."""
	f = font("Cinzel", size)
	tw = c.text_width(NAME, f, tracking)
	iw = c.icon_width(icon_h)
	x = (c.w - (iw + gap + tw)) / 2
	c.icon(x, (c.h - icon_h) / 2, icon_h)
	c.text(x + iw + gap, c.h / 2 + size * 0.05, NAME, f, INK, tracking=tracking, anchor="lm")


def header(out: Path):
	c = Canvas(1920, 480)
	lockup(c, 420, 90, 92, 0.14)
	c.save(out / "ihng-header-1920x480.png", palette=False)                 # Nexus "Header image" slot
	c.save(out / "ihng-banner-card.png", card=28)                            # top of the description


def primary(out: Path):
	c = Canvas(1920, 1080)
	lockup(c, 860, 120, 104, 0.12)
	c.save(out / "ihng-primary-1920x1080.png", palette=False)


ROWS = [
	("Papyrus scripts and quests", "No scripts"),
	("Needs patches", "No patches"),
	("Edits vanilla cells", "Edits nothing"),
	("MCM", "SKSE Menu Framework"),
]


def comparison(c: Canvas, ty: float, top: float, rule_inset: float):
	tf = font("Cinzel", 64)
	left, right, gap = "IHARVEST", NAME, 150
	lw, rw = c.text_width(left, tf, 0.12), c.text_width(right, tf, 0.12)
	start = c.w / 2 - (lw + gap + rw) / 2
	c.text(start + lw / 2, ty, left, tf, INK_DIM, tracking=0.12)
	c.text(start + lw + gap + rw / 2, ty, right, tf, INK, tracking=0.12)
	ax0, ax1 = start + lw + 40, start + lw + gap - 40
	c.line(ax0, ty, ax1, ty, GOLD)
	c.d.polygon([(c.s(ax1 + 2), c.s(ty)), (c.s(ax1 - 10), c.s(ty - 6)), (c.s(ax1 - 10), c.s(ty + 6))], fill=GOLD)

	xl, xr = start + lw / 2, start + lw + gap + rw / 2
	body = font("Jost", 36, "Light")
	row_h = 120
	for i, (a, b) in enumerate(ROWS):
		y = top + i * row_h
		c.text(xl, y, a, body, INK_DIM)
		c.text(xr, y, b, body, INK)
		if i < len(ROWS) - 1:
			c.line(rule_inset, y + row_h / 2, c.w - rule_inset, y + row_h / 2)


def gallery_comparison(out: Path):
	c = Canvas(1920, 1080)
	comparison(c, 250, 420, 330)
	c.save(out / "ihng-gallery-comparison-1920x1080.png")             # Images tab


def description_comparison(out: Path):
	c = Canvas(1600, 720)
	comparison(c, 112, 278, 170)
	c.save(out / "ihng-comparison-card.png", card=28)                       # in the description


SECTIONS = ["Features", "How It Works", "Requirements", "Installation", "Settings", "Compatibility", "FAQ", "Credits"]


def section_headers(out: Path):
	f = font("Cinzel", 34)
	for i, name in enumerate(SECTIONS, 1):
		c = Canvas(1200, 112)
		c.orb(54, c.h / 2, 20)
		c.text(98, c.h / 2 + 1, name.upper(), f, INK, tracking=0.24, anchor="lm")
		c.save(out / f"section-{i:02d}-{name.lower().replace(' ', '-')}.png", card=22)


def main():
	ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	ap.add_argument("-o", "--out", type=Path, default=ROOT / "presskit/nexus")
	args = ap.parse_args()
	args.out.mkdir(parents=True, exist_ok=True)
	for old in args.out.glob("*.png"):
		old.unlink()
	header(args.out)
	primary(args.out)
	gallery_comparison(args.out)
	description_comparison(args.out)
	section_headers(args.out)
	for p in sorted(args.out.glob("*.png")):
		with Image.open(p) as im:
			print(f"{p.name:40s} {im.size[0]}x{im.size[1]}  {p.stat().st_size // 1024} KB")


if __name__ == "__main__":
	main()
