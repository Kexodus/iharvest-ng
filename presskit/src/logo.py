#!/usr/bin/env python3
"""logo.py - render the IHarvest NG icon (no lettering) into presskit/logo/.

A bone-white hand, seen from the side with its palm up, holds a gold spell (orb, two swirls,
sparkles); a leaf, a mushroom and a flower spiral into it, each with a short motion trail. Built like
the CMC logo: one light silhouette, dark cut lines between the fingers, gold for the magic only. Every
part is a mask, so the cut lines are real holes in the transparent file.

Needs Pillow (dev-only; not part of the mod):  python -m pip install pillow
    python presskit/src/logo.py
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
W = 4096                     # working size; coordinates below are 0..1024 design units
K = W / 1024
BG = (13, 13, 12)            # shared with build.py
INK = (232, 227, 216)        # bone white
GOLD = (196, 178, 136)       # the one accent
TRAIL = (150, 145, 136)      # dim ink
CUT = 13                     # dark cut lines between parts

ORB = (520, 470, 62)         # the spell: centre x, y, radius


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


# ---- hand: seen from the side, palm up and cupped, fingers curling up ---------------------------

DX, DY = -90, -40  # shifts the whole hand so the spell sits in the middle of the icon


def shift(pts):
	return [(x + DX, y + DY) for x, y in pts]


FOREARM = ((60 - DX, 1100), (450, 800), 76)

PALM = path(
	((388, 756), (452, 712), (560, 698), (618, 682)),   # inner forearm into the cupped palm
	((618, 682), (690, 690), (700, 760), (650, 796)),   # knuckles, curling under
	((650, 796), (566, 836), (452, 856), (378, 836)),   # back of the hand
	((378, 836), (356, 830), (360, 800), (388, 756)),   # heel, inside the forearm
)

FINGERS = [  # cubic centre lines (base -> tip, curling up) and half-widths; nearest finger first
	(((440, 748), (660, 712), (800, 664), (824, 584)), 27),
	(((628, 726), (768, 730), (842, 690), (860, 624)), 26),
	(((630, 762), (756, 774), (822, 744), (846, 690)), 24),
	(((622, 792), (722, 806), (780, 786), (802, 746)), 21),
]
THUMB = (((452, 722), (434, 656), (448, 610), (490, 588)), 26)


def stroke(m: "Mask", seg, r, v=255, t0=0.0):
	"""A round-ended thick curve, swept as dense discs (thick polylines leave seams at their joints)."""
	pts = cubic(*seg, n=160)
	for x, y in shift(pts[int(t0 * (len(pts) - 1)):]):
		m.circle(x, y, r, v)


def hand() -> "Mask":
	"""One solid silhouette; fingers are separated by cut lines only outside the palm."""
	m = Mask()
	(fa, fb, fr) = FOREARM
	m.capsule(*shift([fa, fb]), fr)
	m.poly(shift(PALM))
	for seg, r in FINGERS:
		stroke(m, seg, r)
	stroke(m, *THUMB)

	# A finger is outlined only where it lies over the fingers behind it (never against the palm).
	for i, (seg, r) in enumerate(FINGERS[:-1]):
		ring = Mask()
		stroke(ring, seg, r + CUT / 2)
		stroke(ring, seg, r, 0)
		behind = Mask()
		for seg2, r2 in FINGERS[i + 1:]:
			stroke(behind, seg2, r2)
		ring.im.paste(ImageChops.multiply(ring.im, behind.im))
		ring.poly(shift(PALM), 0)
		m.im.paste(0, (0, 0), ring.im)

	cx, cy, r = ORB
	m.circle(cx, cy, r + 36, 0)
	return m


# ---- magic --------------------------------------------------------------------------------------

def sparkle(m: "Mask", cx, cy, size):
	"""A four-point star."""
	pts = []
	for k in range(8):
		a = math.radians(-90 + k * 45)
		r = size if k % 2 == 0 else size * 0.3
		pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
	m.poly(pts)


def arc(m: "Mask", cx, cy, r, a0, a1, width):
	pts = [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in range(int(a0), int(a1) + 1, 2)]
	m.line(pts, width)


def swirl(m: "Mask", cx, cy, r, a0, a1, width):
	"""A comet-like arc that tapers from full width at a1 to a point at a0."""
	n = 48
	outer, inner = [], []
	for i in range(n + 1):
		t = i / n
		a = math.radians(a0 + (a1 - a0) * t)
		w = width * t ** 0.8 / 2
		outer.append((cx + (r + w) * math.cos(a), cy + (r + w) * math.sin(a)))
		inner.append((cx + (r - w) * math.cos(a), cy + (r - w) * math.sin(a)))
	m.poly(outer + inner[::-1])
	ex, ey = (outer[-1][0] + inner[-1][0]) / 2, (outer[-1][1] + inner[-1][1]) / 2
	m.circle(ex, ey, width / 2)


def magic(gold: "Mask"):
	cx, cy, r = ORB
	gold.circle(cx, cy, r)
	swirl(gold, cx, cy, r + 26, 150, 300, 14)   # two swirls chasing each other round the orb
	swirl(gold, cx, cy, r + 26, 330, 480, 14)
	sparkle(gold, cx - 150, cy - 96, 28)
	sparkle(gold, cx + 140, cy - 128, 20)
	sparkle(gold, cx + 22, cy - 186, 13)


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


ITEMS = [  # (angle around the spell, distance) - all spiralling clockwise into it
	(-170, 270),   # leaf, left
	(-64, 300),    # mushroom, above
	(-10, 300),    # flower, right
]


def render(transparent: bool) -> Image.Image:
	ink = hand()
	gold = Mask()
	trails = Mask()
	magic(gold)

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
