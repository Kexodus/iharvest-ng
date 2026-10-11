#!/usr/bin/env python3
"""cutout.py - cut the IHarvest NG icon into a transparent PNG.

Source of truth: presskit/logo/ihng-icon-minimal.svg (the icon on its #0d0d0c background); the 1024/512/256
PNGs next to it are its exports. This renders the SVG large with headless Microsoft Edge, takes the
background colour back out of every pixel (so cut-outs such as the mushroom's spots become real holes and
antialiased edges keep their colour), trims to the artwork and writes ihng-icon-minimal-transparent.png.

Needs Windows, Microsoft Edge and Pillow (dev-only):  python -m pip install pillow
    python presskit/src/cutout.py
"""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
LOGO = ROOT / "presskit/logo"
SOURCE = LOGO / "ihng-icon-minimal.svg"
EDGE = Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe")

BG = (13, 13, 12)
# The only colours the artwork uses (press kit style tokens): ink, dim, gold.
INKS = [(232, 227, 216), (150, 145, 136), (196, 178, 136)]


def render_svg(path: Path, px: int) -> Image.Image:
	"""Render an SVG with a 1024x1024 viewBox to a px x px RGB image with headless Edge."""
	svg = path.read_text(encoding="utf8")
	with tempfile.TemporaryDirectory() as tmp:
		html, png = Path(tmp) / "icon.html", Path(tmp) / "icon.png"
		body = f'<html><body style="margin:0;background:rgb{BG}">{svg}</body></html>'
		html.write_text(body.replace('width="1024" height="1024"', 'width="1024" height="1024" style="display:block"', 1), encoding="utf8")
		subprocess.run([str(EDGE), "--headless=new", "--disable-gpu", "--hide-scrollbars",
		                f"--force-device-scale-factor={px / 1024}", "--window-size=1024,1024",
		                f"--screenshot={png}", html.as_uri()], check=True, capture_output=True)
		with Image.open(png) as im:
			return im.convert("RGB").resize((px, px), Image.LANCZOS)


def unmix(im: Image.Image, inks=INKS) -> Image.Image:
	"""Every pixel is some ink blended over BG: recover that ink and how much of it there is."""
	INKS = inks  # noqa: N806 - the colours this image may contain
	src = im.load()
	out = Image.new("RGBA", im.size)
	dst = out.load()
	vecs = [tuple(c - b for c, b in zip(ink, BG)) for ink in INKS]
	norms = [sum(v * v for v in vec) for vec in vecs]
	cache: dict[tuple, tuple] = {}
	w, h = im.size
	for y in range(h):
		for x in range(w):
			c = src[x, y]
			hit = cache.get(c)
			if hit is None:
				d = tuple(a - b for a, b in zip(c, BG))
				best = None
				for ink, vec, n in zip(INKS, vecs, norms):
					a = max(0.0, min(1.0, sum(p * q for p, q in zip(d, vec)) / n))
					err = sum((di - a * vi) ** 2 for di, vi in zip(d, vec))
					if best is None or err < best[0]:
						best = (err, ink, a)
				_, ink, a = best
				hit = cache[c] = ink + (round(a * 255),) if a > 0.02 else (BG + (0,))
			dst[x, y] = hit
	return out


def main() -> None:
	art = unmix(render_svg(SOURCE, 2048))
	box = art.getbbox()
	pad = 24
	box = (max(0, box[0] - pad), max(0, box[1] - pad), min(art.width, box[2] + pad), min(art.height, box[3] + pad))
	art = art.crop(box)
	art = art.resize((art.width // 2, art.height // 2), Image.BOX)  # back to 1024-scale
	dest = LOGO / "ihng-icon-minimal-transparent.png"
	art.save(dest, optimize=True)
	print(f"{dest.relative_to(ROOT)}  {art.width}x{art.height}")


if __name__ == "__main__":
	main()
