#!/usr/bin/env python3
"""logo.py - render the IHarvest NG icon (no lettering) into presskit/logo/.

An open bone-white hand holds a gold spell orb; a leaf, a mushroom and a flower spiral into it, each
with a short motion trail. Built like the CMC logo: one light silhouette, dark cut lines between its
parts, one gold accent. Every part is a mask, so the cut lines are real holes in the transparent file.

Needs Pillow (dev-only; not part of the mod):  python -m pip install pillow
    python presskit/src/logo.py
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
W = 4096                     # working size; coordinates below are 0..1024 design units
K = W / 1024
BG = (13, 13, 12)            # shared with build.py
INK = (232, 227, 216)        # bone white
GOLD = (196, 178, 136)       # the one accent
TRAIL = (150, 145, 136)      # dim ink
CUT = 13                     # dark cut lines between parts

ORB = (512, 492, 60)         # centre x, y, radius


def P(x, y):
	return (x * K, y * K)


class Mask:
	def __init__(self):
		self.im = Image.new("L", (W, W), 0)
		self.d = ImageDraw.Draw(self.im)

	def poly(self, pts, v=255):
		self.d.polygon([P(x, y) for x, y in pts], fill=v)

	def circle(self, cx, cy, r, v=255):
		self.d.ellipse([P(cx - r, cy - r), P(cx + r, cy + r)], fill=v)

	def ring(self, cx, cy, r, width, v=255):
		self.d.ellipse([P(cx - r, cy - r), P(cx + r, cy + r)], outline=v, width=max(1, int(width * K)))

	def line(self, pts, width, v=255):
		self.d.line([P(x, y) for x, y in pts], fill=v, width=max(1, int(width * K)), joint="curve")
		for x, y in (pts[0], pts[-1]):
			self.circle(x, y, width / 2, v)

	def capsule(self, a, b, r, v=255):
		self.line([a, b], 2 * r, v)


def cubic(p0, p1, p2, p3, n=48):
	out = []
	for i in range(n + 1):
		t = i / n
		u = 1 - t
		out.append((u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
		            u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1]))
	return out


def path(*segments):
	pts = []
	for seg in segments:
		pts += cubic(*seg)[(1 if pts else 0):]
	return pts


# ---- hand ---------------------------------------------------------------------------------------

def finger(base, angle_deg, length, r):
	a = math.radians(angle_deg)
	return base, (base[0] + length * math.sin(a), base[1] - length * math.cos(a)), r


FINGERS = [
	finger((420, 590), -13, 262, 28),   # index
	finger((481, 574), -4, 300, 29),    # middle
	finger((546, 576), 5, 282, 28),     # ring
	finger((606, 596), 15, 214, 26),    # little
]
THUMB = ((410, 720), (300, 606), 32)

PALM = path(
	((410, 576), (600, 566), (612, 570), (626, 600)),
	((626, 600), (662, 664), (650, 742), (604, 796)),
	((604, 796), (586, 818), (580, 846), (584, 890)),
	((584, 890), (586, 940), (590, 990), (594, 1024)),
	((594, 1024), (540, 1024), (484, 1024), (430, 1024)),
	((430, 1024), (434, 990), (438, 940), (440, 890)),
	((440, 890), (444, 846), (436, 816), (414, 790)),
	((414, 790), (376, 742), (370, 640), (410, 576)),
)


def hand() -> Mask:
	"""One solid silhouette; fingers are separated by cut lines only where they rise above the palm."""
	m = Mask()
	m.poly(PALM)
	for b, t, r in FINGERS:
		m.capsule(b, t, r)
	b, t, r = THUMB
	m.capsule(b, t, r)

	cuts = Mask()
	for b, t, r in FINGERS:
		cuts.capsule(b, t, r + CUT / 2)
		cuts.capsule(b, t, r, 0)
	cuts.poly(PALM, 0)  # no cut lines inside the palm
	m.im.paste(0, (0, 0), cuts.im)

	# The orb floats in front of the fingers: clear a gap around it.
	cx, cy, r = ORB
	m.circle(cx, cy, r + 34, 0)
	return m


# ---- ingredients spiralling into the orb --------------------------------------------------------

def spiral(angle_deg, radius):
	cx, cy, _ = ORB
	a = math.radians(angle_deg)
	return cx + radius * math.cos(a), cy + radius * math.sin(a)


def trail(m: Mask, angle_deg, radius):
	"""Shrinking dots behind an ingredient, further out along its inward spiral."""
	for k, r in enumerate((10, 7.5, 5)):
		m.circle(*spiral(angle_deg - 16 - k * 10, radius + 22 + k * 18), r)


def rotate(pts, cx, cy, deg):
	a = math.radians(deg)
	c, s = math.cos(a), math.sin(a)
	return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


def leaf(ink: Mask, cx, cy, deg):
	L, H = 112, 46
	top = [(cx - L / 2 + L * t, cy - H / 2 * math.sin(math.pi * t) ** 0.8) for t in (i / 40 for i in range(41))]
	bot = [(cx + L / 2 - L * t, cy + H / 2 * math.sin(math.pi * t) ** 0.8) for t in (i / 40 for i in range(41))]
	ink.poly(rotate(top + bot, cx, cy, deg))
	rib = rotate([(cx - L / 2 + 10, cy), (cx + L / 2 - 18, cy)], cx, cy, deg)
	ink.line(rib, 6, 0)
	stem = rotate([(cx - L / 2 - 16, cy), (cx - L / 2 + 4, cy)], cx, cy, deg)
	ink.line(stem, 7)


def mushroom(ink: Mask, cx, cy, deg):
	cap = [(cx + 50 * math.cos(a), cy - 8 - 44 * math.sin(a)) for a in (math.pi * i / 40 for i in range(41))]
	ink.poly(rotate(cap, cx, cy, deg))
	ink.line(rotate([(cx - 50, cy - 8), (cx + 50, cy - 8)], cx, cy, deg), 6, 0)
	ink.line(rotate([(cx, cy + 6), (cx, cy + 40)], cx, cy, deg), 26)


def flower(ink: Mask, gold: Mask, cx, cy):
	for k in range(5):
		a = math.radians(-90 + k * 72)
		ink.circle(cx + 23 * math.cos(a), cy + 23 * math.sin(a), 19)
	ink.circle(cx, cy, 13, 0)
	gold.circle(cx, cy, 9)


ITEMS = [  # (angle around the orb, distance) - all travelling clockwise into it
	(-152, 248),   # leaf, upper left
	(-30, 262),    # mushroom, upper right
	(32, 258),     # flower, lower right
]


def render(transparent: bool) -> Image.Image:
	ink = hand()
	gold = Mask()
	trails = Mask()

	cx, cy, r = ORB
	gold.circle(cx, cy, r)
	gold.ring(cx, cy, r + 17, 3)

	(la, lr), (ma, mr), (fa, fr) = ITEMS
	for a, rad in ITEMS:
		trail(trails, a, rad)
	leaf(ink, *spiral(la, lr), la + 90 + 20)
	mushroom(ink, *spiral(ma, mr), 28)
	flower(ink, gold, *spiral(fa, fr))

	img = Image.new("RGBA", (W, W), (0, 0, 0, 0) if transparent else BG + (255,))
	for color, mask in ((TRAIL, trails), (INK, ink), (GOLD, gold)):
		img.paste(Image.new("RGBA", (W, W), color + (255,)), (0, 0), mask.im)
	return img


def main():
	ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	ap.add_argument("-o", "--out", type=Path, default=ROOT / "presskit/logo")
	args = ap.parse_args()
	args.out.mkdir(parents=True, exist_ok=True)
	for old in args.out.glob("ihng-icon-*.png"):
		old.unlink()
	dark = render(False)
	for size in (1024, 512, 256):
		dark.resize((size, size), Image.LANCZOS).convert("RGB").save(args.out / f"ihng-icon-{size}.png", optimize=True)
	clear = render(True)
	box = clear.getbbox()
	pad = 16 * K
	box = (max(0, box[0] - pad), max(0, box[1] - pad), min(W, box[2] + pad), min(W, box[3] + pad))
	crop = clear.crop(tuple(int(v) for v in box))
	crop.resize((crop.width // 4, crop.height // 4), Image.LANCZOS).save(args.out / "ihng-icon-transparent.png", optimize=True)
	for p in sorted(args.out.glob("*.png")):
		with Image.open(p) as im:
			print(f"{p.name:28s} {im.size[0]}x{im.size[1]}  {p.stat().st_size // 1024} KB")


if __name__ == "__main__":
	main()
