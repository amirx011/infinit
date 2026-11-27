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

try:  # matplotlib ships the Computer Modern faces the dataset renders with.
    import matplotlib

    CM_FONTS = pathlib.Path(matplotlib.get_data_path()) / "fonts" / "ttf"
except Exception:  # pragma: no cover - the tool still runs without them
    CM_FONTS = pathlib.Path("/nonexistent")

# A third tuple element, when present, limits the font to those characters. TeX
# fonts map ASCII slots to *their* glyphs, not Unicode, so an unrestricted
# cmr10/cmsy10 pass would label a symbol with whatever happens to sit in that
# slot.
FONTS = [
    ("stix", "/usr/share/fonts/stix-fonts/STIXTwoMath-Regular.otf"),
    ("liberation-serif", "/usr/share/fonts/liberation-serif-fonts/LiberationSerif-Regular.ttf"),
    ("liberation-sans", "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf"),
    ("free-serif", "/usr/share/fonts/gnu-free/FreeSerif.ttf"),
    ("free-sans", "/usr/share/fonts/gnu-free/FreeSans.ttf"),
    (
        "cm-roman",
        str(CM_FONTS / "cmr10.ttf"),
        # TeX's ASCII slots are not Unicode: in cmr10 "|" is an en dash and
        # "<"/">" are Spanish punctuation, so only the slots whose glyph names
        # actually match the class are used. The hyphen is also left out; a
        # math minus comes from cmsy and the short hyphen template used to win
        # over it.
        set("0123456789+=()[]/.,%!*"),
    ),
]

# Printed math sets variables in *italic* and everything else upright, so a bank
# built only from upright fonts answers "x" with "?", "'" or "m" on real pages.
# These faces are used for the letter classes only; digits and operators stay
# upright, which is how textbooks set them.
# Computer Modern is the default mathtext face. Leaving it out made every cm
# page fail (x -> ?, 2 -> o, 4 -> ]), so the math-italic and roman cut are
# included with narrow, explicit character sets rather than dropped.
ITALIC_FONTS = [
    ("stix-italic", "/usr/share/fonts/stix-fonts/STIXTwoText-Italic.otf"),
    ("dejavu-italic", "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Oblique.ttf"),
    ("liberation-serif-italic", "/usr/share/fonts/liberation-serif-fonts/LiberationSerif-Italic.ttf"),
    ("free-serif-italic", "/usr/share/fonts/gnu-free/FreeSerifItalic.ttf"),
    ("cm-math-italic", str(CM_FONTS / "cmmi10.ttf")),
]

# Classes that a math renderer italicises. Same labels, extra faces: the score
# takes the best entry per label, so this only adds evidence.
ITALIC_CLASSES = set("xyzabcnmtesuvwdfghjklrpqio")

# label -> characters that all mean the same class
CLASSES = {
    "0": "0", "1": "1", "2": "2", "3": "3", "4": "4",
    "5": "5", "6": "6", "7": "7", "8": "8", "9": "9",
    "x": "x", "y": "y", "z": "z", "a": "a", "b": "b", "c": "c",
    "n": "n", "m": "m", "t": "t", "e": "e", "s": "s", "i": "i",
    "l": "l", "o": "o", "p": "p", "q": "q", "u": "u", "v": "v",
    "w": "w", "d": "d", "f": "f", "g": "g", "h": "h", "j": "j",
    "k": "k", "r": "r",
    "+": "+", "-": "-\u2212", "=": "=", "*": "*\u2217",
    "(": "(", ")": ")", "[": "[", "]": "]",
    "/": "/", ".": ".", ",": ",", "<": "<", ">": ">",
    "|": "|", "^": "^", "!": "!", "'": "'\u2032",
    # Comparison and arithmetic signs the plan's §5.4 lists. The bank had only
    # "<" and ">", so a printed "≤" had no class to land on.
    "×": "×", "÷": "÷", "≤": "≤", "≥": "≥",
    "≠": "≠", "±": "±", "%": "%",
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


_COVERAGE: dict[str, set[int] | None] = {}


def font_covers(font_path: str, char: str) -> bool:
    """True when the font's cmap really maps `char`.

    Pillow happily draws .notdef (a hollow box) for a missing character, which
    would become a bogus template under the real label. TeX fonts are checked
    too, but their ASCII slots do not mean Unicode, so those faces also carry an
    explicit allowed-character set.
    """
    if font_path not in _COVERAGE:
        try:
            from fontTools.ttLib import TTFont

            _COVERAGE[font_path] = set(TTFont(font_path).getBestCmap() or {})
        except Exception:
            _COVERAGE[font_path] = None
    cmap = _COVERAGE[font_path]
    return cmap is None or ord(char) in cmap


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="data/glyph_templates.bin")
    parser.add_argument("--report", default="data/glyph_templates.txt")
    parser.add_argument("--preview", default=None)
    args = parser.parse_args()

    entries = []
    report = []
    for font_index, entry in enumerate(FONTS):
        font_name, font_path = entry[0], entry[1]
        allowed = entry[2] if len(entry) > 2 else None
        if not pathlib.Path(font_path).exists():
            print(f"skip missing font: {font_path}")
            continue
        for label, chars in CLASSES.items():
            for char in chars:
                if allowed is not None and char not in allowed:
                    continue
                if not font_covers(font_path, char):
                    continue
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

    # Italic faces, letters only: this is what printed math actually looks like.
    for offset, entry in enumerate(ITALIC_FONTS):
        font_name, font_path = entry[0], entry[1]
        allowed = entry[2] if len(entry) > 2 else None
        if not pathlib.Path(font_path).exists():
            print(f"skip missing font: {font_path}")
            continue
        font_index = len(FONTS) + offset
        for label, chars in CLASSES.items():
            if label not in ITALIC_CLASSES:
                continue
            for char in chars:
                if allowed is not None and char not in allowed:
                    continue
                if not font_covers(font_path, char):
                    continue
                crop = render(char, font_path)
                if crop is None:
                    continue
                grid = normalise(crop)
                entries.append((label, font_index, grid))
                report.append(f"{label}\t{font_name}\t{crop.size[0]}x{crop.size[1]}")

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
