#!/usr/bin/env python3
"""nexus_art.py - render the Nexus page art for iHarvest NG (banner, thumbnail, comparison chart, section headers).

Needs Pillow + NumPy (dev-only; not part of the mod):  python -m pip install pillow numpy
    python tools/nexus_art.py [-o docs/nexus/images]

Everything is procedural and seeded, so the output is reproducible. Title font: Cinzel (SIL OFL,
tools/fonts/Cinzel-OFL.txt). Body font: Segoe UI from Windows.
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CINZEL = ROOT / "tools/fonts/Cinzel.ttf"
SEGOE = Path("C:/Windows/Fonts/segoeui.ttf")
SEGOE_SB = Path("C:/Windows/Fonts/seguisb.ttf")
SEGOE_B = Path("C:/Windows/Fonts/segoeuib.ttf")
SEGOE_SYM = Path("C:/Windows/Fonts/seguisym.ttf")  # check / cross glyphs

GOLD_TOP = (246, 232, 196)
GOLD_BOTTOM = (196, 160, 98)
WISP = np.array([0.80, 1.00, 0.88])
WISP_CORE = np.array([1.00, 1.00, 0.97])


# ---------------------------------------------------------------------------------------------------
# Fonts
# ---------------------------------------------------------------------------------------------------
def cinzel(size: int, weight: str = "Bold") -> ImageFont.FreeTypeFont:
	f = ImageFont.truetype(str(CINZEL), size)
	try:
		f.set_variation_by_name(weight)
	except Exception:
		pass
	return f


def segoe(size: int, kind: str = "regular") -> ImageFont.FreeTypeFont:
	path = {"regular": SEGOE, "semibold": SEGOE_SB, "bold": SEGOE_B, "symbol": SEGOE_SYM}[kind]
	return ImageFont.truetype(str(path), size)


# ---------------------------------------------------------------------------------------------------
# Procedural scene (float RGB in 0..1, additive light)
# ---------------------------------------------------------------------------------------------------
def smooth_noise(rng: np.random.Generator, h: int, w: int, cell: int) -> np.ndarray:
	gh, gw = h // cell + 3, w // cell + 3
	grid = rng.random((gh, gw)).astype(np.float32)
	img = Image.fromarray((grid * 255).astype(np.uint8)).resize((gw * cell, gh * cell), Image.BICUBIC)
	arr = np.asarray(img, dtype=np.float32)[:h, :w] / 255.0
	return arr


def ridge(rng: np.random.Generator, w: int, base: float, amp: float, octaves: int = 6) -> np.ndarray:
	x = np.linspace(0, 1, w)
	y = np.zeros(w)
	a, f = amp, 2.0
	for _ in range(octaves):
		phase = rng.random() * math.tau
		y += a * np.abs(np.sin(x * f * math.pi + phase))  # sharp-ish peaks
		a *= 0.5
		f *= 2.1
	return base - y


def glow(img: np.ndarray, x: float, y: float, sigma: float, color: np.ndarray, strength: float) -> None:
	h, w, _ = img.shape
	r = int(sigma * 4) + 1
	x0, x1 = max(0, int(x) - r), min(w, int(x) + r + 1)
	y0, y1 = max(0, int(y) - r), min(h, int(y) + r + 1)
	if x0 >= x1 or y0 >= y1:
		return
	yy, xx = np.mgrid[y0:y1, x0:x1]
	d2 = (xx - x) ** 2 + (yy - y) ** 2
	k = np.exp(-d2 / (2 * sigma * sigma)) * strength
	img[y0:y1, x0:x1] += k[..., None] * color


def bezier(p0, c, p1, t):
	u = 1 - t
	return (u * u * p0[0] + 2 * u * t * c[0] + t * t * p1[0], u * u * p0[1] + 2 * u * t * c[1] + t * t * p1[1])


def wisp(img: np.ndarray, x: float, y: float, scale: float = 1.0) -> None:
	glow(img, x, y, 26 * scale, WISP * np.array([0.35, 0.75, 0.55]), 0.55)
	glow(img, x, y, 9 * scale, WISP, 0.9)
	glow(img, x, y, 3.2 * scale, WISP_CORE, 1.6)


def trail(img: np.ndarray, p0, c, p1, t_end: float, scale: float = 1.0) -> None:
	n = 70
	for i in range(n):
		t = t_end * i / n
		x, y = bezier(p0, c, p1, t)
		fade = (i / n) ** 1.6
		glow(img, x, y, 2.6 * scale, WISP * np.array([0.6, 1.0, 0.75]), 0.45 * fade)
	x, y = bezier(p0, c, p1, t_end)
	wisp(img, x, y, scale)


FLOWER_COLORS = [np.array(c) for c in ([0.35, 0.55, 1.0], [0.95, 0.30, 0.35], [0.70, 0.40, 1.0], [1.0, 0.85, 0.35], [0.45, 0.9, 1.0])]


def scene(w: int, h: int, seed: int, horizon: float = 0.62, title_clear: tuple[float, float] | None = None,
          wisp_paths: list | None = None) -> np.ndarray:
	"""Night tundra under an aurora; wisps flying from a caster point out to glowing flowers."""
	rng = np.random.default_rng(seed)
	yy = np.linspace(0, 1, h)[:, None]
	top = np.array([0.020, 0.040, 0.070])
	mid = np.array([0.060, 0.120, 0.150])
	img = (top * (1 - yy) + mid * yy)[:, None, :].repeat(w, axis=1).reshape(h, w, 3).astype(np.float32)

	# stars
	n_stars = int(w * h / 1800)
	sx, sy = rng.random(n_stars) * w, rng.random(n_stars) * h * horizon
	for x, y, b in zip(sx, sy, rng.random(n_stars)):
		glow(img, x, y, 0.8 + b * 0.6, np.array([0.85, 0.9, 1.0]), 0.25 + 0.6 * b ** 3)

	# aurora: soft curtains of green/teal
	xs = np.arange(w)[None, :]
	ys = np.arange(h)[:, None]
	for band in range(3):
		phase = rng.random() * math.tau
		freq = (1.4 + rng.random()) * math.tau / w
		yc = h * (0.18 + 0.12 * band) + h * 0.08 * np.sin(xs * freq + phase)
		width = h * (0.07 + 0.03 * band)
		shimmer = 0.55 + 0.45 * np.sin(xs * freq * 7.3 + phase * 3)
		curtain = np.exp(-(((ys - yc) / width) ** 2)) * shimmer
		# curtains hang down: extra soft tail below the band
		tail = np.clip((ys - yc) / (h * 0.35), 0, 1)
		curtain += np.exp(-(((ys - yc) / (width * 3.0)) ** 2)) * shimmer * 0.35 * (1 - tail)
		color = np.array([0.10, 0.55, 0.38]) if band != 1 else np.array([0.05, 0.40, 0.45])
		img += curtain[..., None] * color * (0.45 - band * 0.08)

	# mountains, far to near
	layers = [
		(horizon - 0.12, 0.16, np.array([0.050, 0.085, 0.110])),
		(horizon - 0.05, 0.12, np.array([0.035, 0.060, 0.075])),
		(horizon + 0.03, 0.07, np.array([0.022, 0.038, 0.045])),
	]
	for base, amp, col in layers:
		line = ridge(rng, w, base * h, amp * h)
		mask = (ys >= line[None, :])
		img[mask] = img[mask] * 0.15 + col
		# snow catch-light on the far ridges
		edge = np.exp(-((ys - line[None, :]) / 3.0) ** 2) * (ys >= line[None, :] - 2)
		img += edge[..., None] * col * 1.4

	# low mist
	fog = smooth_noise(rng, h, w, 90) * 0.6 + smooth_noise(rng, h, w, 40) * 0.4
	band = np.exp(-(((ys / h) - (horizon + 0.06)) / 0.08) ** 2)
	img += (fog * band)[..., None] * np.array([0.10, 0.16, 0.17]) * 0.9

	# foreground ground + grass + flowers
	ground_y = h * (horizon + 0.20)
	gline = ground_y + h * 0.025 * np.sin(np.arange(w) / w * math.tau * 1.3 + rng.random() * 6)
	gmask = ys >= gline[None, :]
	img[gmask] = img[gmask] * 0.1 + np.array([0.012, 0.022, 0.020])

	pil = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
	draw = ImageDraw.Draw(pil)
	for _ in range(int(w * 1.6)):
		x = rng.random() * w
		base_y = gline[int(min(w - 1, x))] + rng.random() * 6
		hgt = h * (0.02 + rng.random() * 0.06)
		lean = (rng.random() - 0.5) * hgt * 0.6
		shade = int(10 + rng.random() * 18)
		draw.line([(x, base_y), (x + lean, base_y - hgt)], fill=(shade // 2, shade, shade - 2), width=1)
	img = np.asarray(pil, dtype=np.float32) / 255.0
	img = img.copy()

	flowers = []
	for _ in range(int(w / 42)):
		x = rng.random() * w
		if title_clear and title_clear[0] * w < x < title_clear[1] * w and rng.random() < 0.7:
			continue
		y = gline[int(min(w - 1, x))] - h * (0.015 + rng.random() * 0.05)
		col = FLOWER_COLORS[rng.integers(len(FLOWER_COLORS))]
		glow(img, x, y, 5.5, col * 0.6, 0.55)
		glow(img, x, y, 1.8, col, 1.1)
		flowers.append((x, y))

	# wisps
	if wisp_paths is None:
		caster = (w * 0.5, h * (horizon + 0.17))
		flowers.sort(key=lambda p: abs(p[0] - caster[0]))
		picks = [flowers[i] for i in rng.choice(min(len(flowers), 22), size=min(7, len(flowers)), replace=False)]
		wisp_paths = []
		for fx, fy in picks:
			lift = h * (0.22 + rng.random() * 0.2)
			ctrl = ((caster[0] + fx) / 2 + (rng.random() - 0.5) * w * 0.08, min(caster[1], fy) - lift)
			wisp_paths.append((caster, ctrl, (fx, fy - 8), 0.55 + rng.random() * 0.43))
	scale = max(0.6, h / 600)
	for p0, c, p1, t in wisp_paths:
		trail(img, p0, c, p1, t, scale)

	# vignette
	xx = (np.arange(w)[None, :] / w - 0.5) * 2
	yv = (np.arange(h)[:, None] / h - 0.5) * 2
	vig = 1 - 0.35 * np.clip(xx ** 2 * 0.6 + yv ** 2 * 0.9, 0, 1)
	img *= vig[..., None]
	return img


def to_image(arr: np.ndarray) -> Image.Image:
	# soft filmic shoulder so additive glows roll off instead of clipping
	arr = 1 - np.exp(-arr * 1.15)
	return Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8)).convert("RGBA")


# ---------------------------------------------------------------------------------------------------
# Text helpers
# ---------------------------------------------------------------------------------------------------
def gradient_text(base: Image.Image, xy, text, font, top=GOLD_TOP, bottom=GOLD_BOTTOM, glow_radius=18,
                  glow_color=(120, 255, 190), glow_alpha=150, shadow=True, anchor="mm") -> None:
	w, h = base.size
	mask = Image.new("L", (w, h), 0)
	ImageDraw.Draw(mask).text(xy, text, font=font, fill=255, anchor=anchor)
	if shadow:
		sh = mask.filter(ImageFilter.GaussianBlur(6))
		dark = Image.new("RGBA", (w, h), (0, 0, 0, 0))
		dark.putalpha(sh.point(lambda v: int(v * 0.85)))
		base.alpha_composite(dark, (0, 4))
	if glow_radius:
		g = mask.filter(ImageFilter.GaussianBlur(glow_radius))
		layer = Image.new("RGBA", (w, h), glow_color + (0,))
		layer.putalpha(g.point(lambda v: int(v * glow_alpha / 255)))
		base.alpha_composite(layer)
	bbox = mask.getbbox()
	if not bbox:
		return
	grad = Image.new("RGBA", (w, h))
	gd = ImageDraw.Draw(grad)
	for y in range(bbox[1], bbox[3] + 1):
		t = (y - bbox[1]) / max(1, bbox[3] - bbox[1])
		col = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)) + (255,)
		gd.line([(bbox[0], y), (bbox[2], y)], fill=col)
	grad.putalpha(mask)
	base.alpha_composite(grad)


def plain_text(base: Image.Image, xy, text, font, fill=(225, 235, 230, 255), anchor="mm", shadow=True) -> None:
	if shadow:
		sh = Image.new("RGBA", base.size, (0, 0, 0, 0))
		ImageDraw.Draw(sh).text((xy[0], xy[1] + 2), text, font=font, fill=(0, 0, 0, 200), anchor=anchor)
		base.alpha_composite(sh.filter(ImageFilter.GaussianBlur(3)))
	ImageDraw.Draw(base).text(xy, text, font=font, fill=fill, anchor=anchor)


def ornament(base: Image.Image, cx: int, y: int, half: int, color=(196, 170, 120, 220)) -> None:
	d = ImageDraw.Draw(base)
	d.line([(cx - half, y), (cx - 18, y)], fill=color, width=2)
	d.line([(cx + 18, y), (cx + half, y)], fill=color, width=2)
	d.polygon([(cx, y - 7), (cx + 7, y), (cx, y + 7), (cx - 7, y)], outline=color, fill=(30, 50, 45, 230))
	for s in (-1, 1):
		d.ellipse([cx + s * half - 3, y - 3, cx + s * half + 3, y + 3], fill=color)


def pill(base: Image.Image, cx: int, cy: int, text: str, font, pad=(22, 10), fill=(18, 40, 36, 215),
         outline=(150, 230, 190, 255), color=(225, 250, 235, 255)) -> int:
	d = ImageDraw.Draw(base)
	tw = d.textlength(text, font=font)
	w = int(tw + pad[0] * 2)
	h = int(font.size + pad[1] * 2)
	box = [cx - w // 2, cy - h // 2, cx + w // 2, cy + h // 2]
	d.rounded_rectangle(box, radius=h // 2, fill=fill, outline=outline, width=2)
	d.text((cx, cy), text, font=font, fill=color, anchor="mm")
	return w


# ---------------------------------------------------------------------------------------------------
# Assets
# ---------------------------------------------------------------------------------------------------
def banner(out: Path) -> None:
	w, h = 1920, 480
	img = to_image(scene(w, h, seed=7, horizon=0.58, title_clear=(0.3, 0.7)))
	gradient_text(img, (w // 2, int(h * 0.36)), "iHarvest NG", cinzel(150, "Black"), glow_radius=26)
	ornament(img, w // 2, int(h * 0.56), 330)
	plain_text(img, (w // 2, int(h * 0.66)), "Wisps that harvest everything around you  —  now fully native", segoe(34, "semibold"))
	f = segoe(24, "bold")
	x = w // 2 - 210
	for label in ("SE", "AE", "VR", "NO PAPYRUS", "SKSE"):
		bw = pill(img, 0, -999, label, f)  # measure
		img2 = img
		pill(img2, x + bw // 2, int(h * 0.80), label, f)
		x += bw + 14
	img.convert("RGB").save(out / "banner.png", optimize=True)


def thumbnail(out: Path) -> None:
	w, h = 1280, 720
	img = to_image(scene(w, h, seed=11, horizon=0.55, title_clear=(0.2, 0.8)))
	gradient_text(img, (w // 2, int(h * 0.30)), "iHarvest", cinzel(150, "Black"), glow_radius=24)
	gradient_text(img, (w // 2, int(h * 0.47)), "N G", cinzel(96, "Bold"), top=(210, 255, 225), bottom=(120, 210, 165), glow_radius=20)
	ornament(img, w // 2, int(h * 0.585), 260)
	plain_text(img, (w // 2, int(h * 0.65)), "Native SKSE  ·  SE / AE / VR", segoe(36, "semibold"))
	img.convert("RGB").save(out / "thumbnail.png", optimize=True)


def compare(out: Path) -> None:
	w, h = 1600, 980
	bg = scene(w, h, seed=23, horizon=0.86, title_clear=(0.0, 1.0), wisp_paths=[])
	img = to_image(bg * 0.55)
	# Title with a drawn arrow (Cinzel has no arrow glyph)
	tf = cinzel(64, "Bold")
	left, right = "IHarvest", "iHarvest NG"
	measure = ImageDraw.Draw(Image.new("L", (1, 1)))
	lw, rw, gap = measure.textlength(left, font=tf), measure.textlength(right, font=tf), 120
	start = w / 2 - (lw + gap + rw) / 2
	gradient_text(img, (int(start + lw / 2), 82), left, tf, glow_radius=16, top=(225, 205, 180), bottom=(170, 140, 110))
	gradient_text(img, (int(start + lw + gap + rw / 2), 82), right, tf, glow_radius=16)
	ax0, ax1, ay = start + lw + 28, start + lw + gap - 28, 84
	ad = ImageDraw.Draw(img)
	ad.line([(ax0, ay), (ax1, ay)], fill=(170, 240, 200, 255), width=5)
	ad.polygon([(ax1 + 6, ay), (ax1 - 16, ay - 13), (ax1 - 16, ay + 13)], fill=(170, 240, 200, 255))
	ornament(img, w // 2, 138, 360)

	rows = [
		("Logic", "Papyrus: 8 quests, 8 finder threads", "Native C++ in the game loop"),
		("Wisps", "AI actors + 64 pathing markers", "Lightweight refs, curved native flight"),
		("Finding flora", "Story Manager / SkyPal, ~3 s timeouts", "Native scan of loaded cells"),
		("Harvesting", "Needs Vanilla Fixes / USSEP patches", "Harvests as you: everything just works"),
		("Save footprint", "Quests, actors, stuck-thread risk", "No quests, actors or scripts"),
		("Conflicts", "Overrides 4 worldspaces + 19 cells", "Zero vanilla records touched"),
		("VR", "Rename .esl → .esp by hand", "Same DLL, ESL-flagged .esp"),
		("Settings", "SkyUI MCM", "SKSE Menu Framework + INI"),
	]
	x0, x1, x2, x3 = 70, 390, 990, w - 70
	top, row_h = 190, 84
	d = ImageDraw.Draw(img)
	head = segoe(30, "bold")
	d.rounded_rectangle([x0, top, x3, top + 64], radius=14, fill=(16, 34, 32, 235), outline=(110, 170, 140, 255), width=2)
	for x, label, col in ((x0 + 24, "", None), (x1 + 24, "Original IHarvest", (230, 190, 170, 255)), (x2 + 24, "iHarvest NG", (170, 250, 200, 255))):
		if label:
			d.text((x, top + 32), label, font=head, fill=col, anchor="lm")
	body = segoe(27, "regular")
	key = segoe(28, "semibold")
	sym = segoe(26, "symbol")
	for i, (k, a, b) in enumerate(rows):
		y = top + 72 + i * row_h
		fill = (12, 26, 26, 215) if i % 2 == 0 else (18, 36, 34, 215)
		d.rounded_rectangle([x0, y, x3, y + row_h - 8], radius=12, fill=fill)
		cy = y + (row_h - 8) // 2
		d.text((x0 + 24, cy), k, font=key, fill=(235, 225, 200, 255), anchor="lm")
		d.text((x1 + 24, cy), "✕", font=sym, fill=(220, 120, 110, 255), anchor="lm")
		d.text((x1 + 60, cy), a, font=body, fill=(214, 180, 168, 255), anchor="lm")
		d.text((x2 + 24, cy), "✓", font=sym, fill=(120, 240, 170, 255), anchor="lm")
		d.text((x2 + 60, cy), b, font=body, fill=(176, 246, 204, 255), anchor="lm")
	img.convert("RGB").save(out / "comparison.png", optimize=True)


SECTIONS = ["Features", "How It Works", "Requirements", "Installation", "Settings", "Compatibility", "FAQ", "Credits"]


def section_headers(out: Path) -> None:
	w, h = 1200, 120
	for i, name in enumerate(SECTIONS):
		bg = scene(w, h * 4, seed=100 + i, horizon=0.45, title_clear=(0.0, 1.0), wisp_paths=[])[h * 1:h * 2] * 0.7
		img = to_image(bg)
		d = ImageDraw.Draw(img)
		d.line([(40, h - 18), (w - 40, h - 18)], fill=(150, 200, 170, 120), width=1)
		gradient_text(img, (w // 2, h // 2 - 4), name.upper(), cinzel(54, "Bold"), glow_radius=12, glow_alpha=120)
		# a little wisp on each side
		arr = np.asarray(img.convert("RGB"), dtype=np.float32) / 255.0
		arr = -np.log(np.clip(1 - arr, 1e-4, 1)) / 1.15  # undo filmic shoulder
		tw = ImageDraw.Draw(Image.new("L", (1, 1))).textlength(name.upper(), font=cinzel(54, "Bold"))
		for s in (-1, 1):
			wisp(arr, w / 2 + s * (tw / 2 + 46), h / 2 - 6, 0.75)
		img = to_image(arr)
		img.convert("RGB").save(out / f"section_{name.lower().replace(' ', '_')}.png", optimize=True)


def main() -> None:
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
