#!/usr/bin/env python3
"""Render synthetic math samples for the scanner.

matplotlib's mathtext gives real fraction/power/root layout without a browser,
so every sample is a realistic printed page fragment rather than a plain text
string. Two variants are written per expression:

  <name>.png         clean render, white page
  <name>_photo.png   same render with blur, noise, rotation and a shadow

Usage: make_samples.py [--outdir DIR] [--dpi N] [--only NAME]
"""

from __future__ import annotations

import argparse
import io
import pathlib

import numpy as np
from matplotlib import mathtext
from matplotlib.font_manager import FontProperties
from PIL import Image, ImageFilter

EXPRESSIONS = {
    "linear": r"$3x + 5 = 2x - 7$",
    "quadratic": r"$x^{2} - 5x + 6 = 0$",
    "fraction": r"$\frac{x+1}{x-3} = 2$",
    "nested_fraction": r"$\frac{\frac{1}{x}+1}{x^{2}-4}$",
    "power": r"$2^{x+1} = 8$",
    "root": r"$\sqrt{x^{2}+9} = x - 1$",
    "trig": r"$\sin(2x) = \cos(x)$",
    "log": r"$\ln(x+1) + \ln(x-1) = 0$",
    "derivative": r"$\frac{dy}{dx} = 3x^{2} - 2x$",
    "integral": r"$\int_{0}^{1} x^{2}\,dx$",
    "limit": r"$\lim_{x \to 0} \frac{\sin x}{x}$",
    "sum": r"$\sum_{n=1}^{\infty} \frac{1}{n^{2}}$",
    "system": r"$2x + y = 5,\; x - y = 1$",
    "mixed": r"$f(x) = \frac{x^{2}-1}{x+2}$",
}


def render(tex: str, dpi: int) -> Image.Image:
    buffer = io.BytesIO()
    mathtext.math_to_image(
        tex, buffer, dpi=dpi, prop=FontProperties(size=48), format="png"
    )
    buffer.seek(0)
    glyphs = Image.open(buffer).convert("RGBA")
    pad = 24
    canvas = Image.new(
        "RGBA", (glyphs.width + 2 * pad, glyphs.height + 2 * pad), (255, 255, 255, 255)
    )
    canvas.alpha_composite(glyphs, (pad, pad))
    return canvas.convert("L")


def degrade(image: Image.Image, seed: int) -> Image.Image:
    rng = np.random.default_rng(seed)
    out = image.rotate(2.5, resample=Image.BICUBIC, fillcolor=255, expand=True)
    out = out.filter(ImageFilter.GaussianBlur(1.1))
    pixels = np.asarray(out, dtype=np.float32)
    shadow = np.full_like(pixels, 22.0)
    shadow[8:, 10:] = 0.0
    pixels = np.clip(pixels - shadow, 0, 255)
    noise = rng.normal(0.0, 9.0, pixels.shape)
    pixels = np.clip(pixels + noise, 0, 255)
    return Image.fromarray(pixels.astype(np.uint8), mode="L")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--outdir", default="samples")
    parser.add_argument("--dpi", type=int, default=200)
    parser.add_argument("--only", default=None)
    args = parser.parse_args()

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    written = 0
    for name, tex in EXPRESSIONS.items():
        if args.only and args.only != name:
            continue
        clean = render(tex, args.dpi)
        clean.save(outdir / f"{name}.png")
        degrade(clean, seed=len(name)).save(outdir / f"{name}_photo.png")
        written += 2
    print(f"wrote {written} images to {outdir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
