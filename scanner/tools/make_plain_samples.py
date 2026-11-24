#!/usr/bin/env python3
"""Render the plain-text samples used by the pipeline tests.

Only Pillow and an installed STIX font are needed, so this runs anywhere the
test suite runs. For samples with real fraction/root/integral layout use
make_samples.py, which needs matplotlib.

Each expression is written twice:

  <name>.png         clean render
  <name>_photo.png   rotated, blurred, shaded and noisy

Usage: make_plain_samples.py [--outdir DIR] [--size N]
"""

from __future__ import annotations

import argparse
import pathlib

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

FONT = "/usr/share/fonts/stix-fonts/STIXTwoText-Regular.otf"

EXPRESSIONS = {
    "plain_linear": "3x + 5 = 2x - 7",
    "plain_trig": "sin(2x) = cos(x)",
    "plain_frac_like": "x + 1 = 2 (x - 3)",
}


def render(text: str, size: int) -> Image.Image:
    font = ImageFont.truetype(FONT, size)
    canvas = Image.new("L", (size * len(text), size * 3), 255)
    draw = ImageDraw.Draw(canvas)
    draw.text((size // 2, size // 2), text, font=font, fill=20)
    box = ImageChops.invert(canvas).getbbox()
    if box is None:
        raise SystemExit(f"no ink for {text!r}")
    ink = canvas.crop(box)
    padded = Image.new("L", (ink.width + size, ink.height + size), 255)
    padded.paste(ink, (size // 2, size // 2))
    return padded


def degrade(image: Image.Image) -> Image.Image:
    gradient = Image.linear_gradient("L").resize(image.size).point(
        lambda v: 200 + v * 55 // 255
    )
    shaded = ImageChops.multiply(image, gradient)
    noisy = Image.blend(shaded, Image.effect_noise(shaded.size, 18), 0.10)
    rotated = noisy.rotate(2.2, resample=Image.BICUBIC, fillcolor=235, expand=True)
    return rotated.filter(ImageFilter.GaussianBlur(0.9))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--outdir", default="samples")
    parser.add_argument("--size", type=int, default=56)
    args = parser.parse_args()

    outdir = pathlib.Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    for name, text in EXPRESSIONS.items():
        clean = render(text, args.size)
        clean.save(outdir / f"{name}.png")
        degrade(clean).save(outdir / f"{name}_photo.png")
    print(f"wrote {2 * len(EXPRESSIONS)} images to {outdir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
