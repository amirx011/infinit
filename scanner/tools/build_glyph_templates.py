#!/usr/bin/env python3
"""Build the template bank for the classical glyph classifier.

Every class is rendered with several installed fonts, cropped to its ink box and
normalised to a 16x16 grid exactly the way the C++ side normalises an input
symbol. The result is a small binary blob that the scanner loads at startup, so
the app ships without any font or image dependency.

Format (little endian):
  magic   char[4]  "IGT1"
  count   uint32
  entries: uint8 labelLength, char label[], uint8 fontIndex,
           uint8 width, uint8 height, uint8 pixels[width*height]

Usage: build_glyph_templates.py [--out PATH] [--report PATH] [--preview DIR]
"""

from __future__ import annotations

import argparse
import pathlib
import struct

from PIL import Image, ImageDraw, ImageFont, ImageOps

GRID = 16
INNER = 14

FONTS = [
    ("stix", "/usr/share/fonts/stix-fonts/STIXTwoMath-Regular.otf"),
    ("liberation-serif", "/usr/share/fonts/liberation-serif-fonts/LiberationSerif-Regular.ttf"),
    ("liberation-sans", "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf"),
    ("free-serif", "/usr/share/fonts/gnu-free/FreeSerif.ttf"),
    ("free-sans", "/usr/share/fonts/gnu-free/FreeSans.ttf"),
]

# label -> characters that all mean the same class
CLASSES = {
    "0": "0", "1": "1", "2": "2", "3": "3", "4": "4",
    "5": "5", "6": "6", "7": "7", "8": "8", "9": "9",
    "x": "x", "y": "y", "z": "z", "a": "a", "b": "b", "c": "c",
    "n": "n", "m": "m", "t": "t", "e": "e", "s": "s", "i": "i",
    "l": "l", "o": "o", "p": "p", "q": "q", "u": "u", "v": "v",
    "w": "w", "d": "d", "f": "f", "g": "g", "h": "h", "j": "j",
    "k": "k", "r": "r",
    "+": "+", "-": "-\u2212", "=": "=", "*": "*",
    "(": "(", ")": ")", "[": "[", "]": "]",
    "/": "/", ".": ".", ",": ",", "<": "<", ">": ">",
    "|": "|", "^": "^", "!": "!", "'": "'",
    # Big operators and the symbols that appear in their bounds.  These labels
    # are structural: the reader turns them into sum/integral/limit nodes
    # instead of passing the raw character to the expression.
    "sum": "\u2211", "integral": "\u222b",
    "inf": "\u221e", "pi": "\u03c0", "arrow": "\u2192",
}


def render(char: str, font_path: str, size: int = 120) -> Image.Image | None:
    try:
        font = ImageFont.truetype(font_path, size)
    except OSError:
        return None
    canvas = Image.new("L", (size * 3, size * 3), 0)
    draw = ImageDraw.Draw(canvas)
    draw.text((size, size), char, font=font, fill=255)
    box = canvas.getbbox()
    if box is None:
        return None
    return canvas.crop(box)


def normalise(crop: Image.Image) -> Image.Image:
    width, height = crop.size
    scale = INNER / max(width, height)
    target = (max(1, round(width * scale)), max(1, round(height * scale)))
    small = crop.resize(target, Image.BOX)
    canvas = Image.new("L", (GRID, GRID), 0)
    canvas.paste(small, ((GRID - target[0]) // 2, (GRID - target[1]) // 2))
    return canvas


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="data/glyph_templates.bin")
    parser.add_argument("--report", default="data/glyph_templates.txt")
    parser.add_argument("--preview", default=None)
    args = parser.parse_args()

    entries = []
    report = []
    for font_index, (font_name, font_path) in enumerate(FONTS):
        if not pathlib.Path(font_path).exists():
            print(f"skip missing font: {font_path}")
            continue
        for label, chars in CLASSES.items():
            for char in chars:
                crop = render(char, font_path)
                if crop is None:
                    print(f"skip {char!r} in {font_name}: no ink")
                    continue
                grid = normalise(crop)
                entries.append((label, font_index, grid))
                report.append(f"{label}\t{font_name}\t{crop.size[0]}x{crop.size[1]}")
                if args.preview:
                    out = pathlib.Path(args.preview) / f"{font_index}_{ord(char):04x}_{label}.png"
                    out.parent.mkdir(parents=True, exist_ok=True)
                    grid.resize((GRID * 8, GRID * 8), Image.NEAREST).save(out)

    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("wb") as handle:
        handle.write(b"IGT1")
        handle.write(struct.pack("<I", len(entries)))
        for label, font_index, grid in entries:
            raw = label.encode("utf-8")
            handle.write(struct.pack("<B", len(raw)))
            handle.write(raw)
            handle.write(struct.pack("<B", font_index))
            handle.write(struct.pack("<B", GRID))
            handle.write(struct.pack("<B", GRID))
            handle.write(grid.tobytes())

    report_path = pathlib.Path(args.report)
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(report) + "\n")

    print(f"wrote {len(entries)} templates ({out_path.stat().st_size} bytes) to {out_path}")
    print(f"classes: {len(CLASSES)}, fonts: {len(FONTS)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
