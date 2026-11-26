#!/usr/bin/env python3
"""Generate a synthetic math-scanner dataset with pixel-accurate ground truth.

Every image is a matplotlib mathtext render (several math fontsets, sizes and
dpis) pushed through a seeded augmentation chain, so a dataset is reproducible
byte for byte from the same seed. The manifest records, per image, the source
expression, its Infinit syntax form, the augmentation chain with the resolved
parameters, and the per-symbol ground-truth boxes in the coordinates of the
*generated* image.

Ground truth is not estimated from the pixels: `MathTextParser('path').parse`
returns every glyph with its font, point size and pen position, so each box is
the real glyph outline box (outline bbox from FreeType, shifted by the ink
origin of the raster). Rules (fraction bars, radical overbars) come from the
same parse: a rule with glyphs above and below it is a fraction bar and stays a
symbol of its own, a rule without them is the overbar of a radical and is folded
into the glyph it belongs to. One box per typeset glyph, otherwise; glyphs that
merely touch stay separate. If the render's ink does not fall inside the boxes
(coverage check), that image is written with `"boxes": null` instead of
inventing geometry.

Usage:
  ~/.venvs/scanner/bin/python scanner/tools/make_dataset.py
  python3 scanner/tools/make_dataset.py --limit 5
  python3 scanner/tools/make_dataset.py --only blur       # chains using blur
  python3 scanner/tools/make_dataset.py --recipe clean     # one recipe only
  python3 scanner/tools/make_dataset.py --outdir /tmp/ds --seed 7 --variants 3

Output layout:
  <outdir>/manifest.json          every image with its ground truth
  <outdir>/images/####.png|.jpg   the generated image (JPEG when the chain says so)
  <outdir>/clean/####.png         the un-augmented render
  <outdir>/ref/####.png           the same render with only the geometric steps
                                  applied (rotation, perspective, resolution):
                                  the foreground reference for the preprocess
                                  metric, free of blur, noise, lighting, paper
                                  texture and ruled lines.
"""

from __future__ import annotations

import argparse
import io
import json
import math
import sys
import unicodedata
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402  (rc_context only)
from matplotlib import mathtext  # noqa: E402
from matplotlib.font_manager import FontProperties  # noqa: E402
from matplotlib.ft2font import LoadFlags  # noqa: E402

TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parent.parent
DEFAULT_BANK = TOOLS.parent / "data" / "glyph_templates.bin"

# The class table is the single source of truth for "which character is which
# classifier label"; the template builder owns it.
sys.path.insert(0, str(TOOLS))
try:  # pragma: no cover - only fails if the builder was moved
    from build_glyph_templates import CLASSES as _CLASSES  # noqa: E402
except ImportError:  # pragma: no cover
    _CLASSES = {}

# Extra spellings that parse to the same class but are not in the builder's
# table: unicode variants of the operators and the symbol-font spellings.
CHAR_LABEL = {}
for _label, _chars in _CLASSES.items():
    for _ch in _chars:
        CHAR_LABEL[_ch] = _label
CHAR_LABEL.update({"′": "'", "″": "'", "∗": "*", "⋅": "*", "«": "<", "»": ">"})

# FreeType glyph names are the font-independent way to identify a symbol: the
# Computer Modern symbol font stores the prime at character code 48 (the digit
# zero in ASCII) and the minus at 0xA1, so character codes alone lie.
GLYPH_NAME_LABEL = {
    "prime": "'",
    "uni2032": "'",
    "minus": "-",
    "infinity": "inf",
    "uni221e": "inf",
    "arrowright": "arrow",
    "uni2192": "arrow",
    "summation": "sum",
    "summationdisplay": "sum",
    "uni2211": "sum",
    "integral": "integral",
    "integraldisplay": "integral",
    "uni222b": "integral",
    "periodcentered": "*",
    "dotmath": "*",
    "uni22c5": "*",
}

MATRIX_BRACKET_FONT = "/usr/share/fonts/stix-fonts/STIXTwoMath-Regular.otf"


# ---------------------------------------------------------------------------
# Curated expressions
#
# `expect` is the Infinit text the scanner is *supposed* to produce for the
# clean render; it follows the emitter conventions of the committed samples
# (implicit multiplication, `^` for powers, `sqrt(...)`, `integral(f, x, a, b)`,
# `sum(term, n=a, b)`, `lim(f, x->a)`, `eq1,eq2` for systems, `[[..],[..]]` for
# matrices). Nothing here is derived from the scanner's own output.
# ---------------------------------------------------------------------------


def _expr(name, domain, latex, expect, kind="expression", layout=None, **extra):
    entry = {
        "name": name,
        "domain": domain,
        "latex": latex,
        "expect_text": expect,
        "expect_kind": kind,
    }
    if layout:
        entry["layout"] = layout
        entry.update(extra)
    return entry


EXPRESSIONS = [
    _expr("linear", "algebra", r"$3x + 5 = 2x - 7$", "3x+5=2x-7"),
    _expr("quadratic", "algebra", r"$x^{2} - 5x + 6 = 0$", "x^2-5x+6=0"),
    _expr("linear_system_row", "algebra", r"$4y - 9 = y + 3$", "4y-9=y+3"),
    _expr("nested_fraction", "fractions", r"$\frac{\frac{1}{x}+1}{x^{2}-4}$", "(1/x+1)/(x^2-4)"),
    _expr("fraction", "fractions", r"$\frac{x+1}{x-3} = 2$", "(x+1)/(x-3)=2"),
    _expr("power", "powers", r"$2^{x+1} = 8$", "2^(x+1)=8"),
    _expr("power_product", "powers", r"$x^{3} \cdot x^{2} = x^{5}$", "x^3*x^2=x^5"),
    _expr("root", "roots", r"$\sqrt{x^{2}+9} = x - 1$", "sqrt(x^2+9)=x-1"),
    _expr("trig", "trig_log", r"$\sin(2x) = \cos(x)$", "sin(2x)=cos(x)"),
    _expr("log", "trig_log", r"$\ln(x+1) + \ln(x-1) = 0$", "ln(x+1)+ln(x-1)=0"),
    _expr("derivative_frac", "derivative", r"$\frac{dy}{dx} = 3x^{2} - 2x$", "(dy)/(dx)=3x^2-2x"),
    _expr("derivative_prime", "derivative", r"$f^{\prime}(x) = 2x$", "f'(x)=2x"),
    _expr("integral", "integral", r"$\int_{0}^{1} x^{2}\,dx$", "integral(x^2, x, 0, 1)", "integral"),
    _expr("sum", "sum", r"$\sum_{n=1}^{\infty} \frac{1}{n^{2}}$", "sum(1/(n^2), n=1, inf)", "sum"),
    _expr("limit", "limit", r"$\lim_{x \to 0} \frac{\sin x}{x}$", "lim(sin(x)/x, x->0)", "limit"),
    _expr(
        "matrix2",
        "matrix",
        None,
        "[[1,2],[3,4]]",
        "matrix",
        layout="matrix",
        cells=[["1", "2"], ["3", "4"]],
    ),
    _expr(
        "matrix3",
        "matrix",
        None,
        "[[1,2,3],[4,5,6],[7,8,9]]",
        "matrix",
        layout="matrix",
        cells=[["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"]],
    ),
    _expr(
        "system2",
        "system",
        None,
        "2x+y=5,x-y=1",
        "system",
        layout="system",
        lines=[r"$2x + y = 5$", r"$x - y = 1$"],
    ),
]

FONTSETS = ["dejavusans", "dejavuserif", "cm", "stix", "stixsans"]
# (fontset, point size, dpi); spread over the expressions so the dataset does
# not look like one render repeated.
VARIANTS = [
    ("dejavusans", 40, 200),
    ("stix", 42, 190),
    ("dejavuserif", 36, 230),
    ("cm", 44, 180),
    ("stixsans", 38, 210),
]

# Augmentation recipes, applied in the listed order. Geometry first, then the
# paper and the camera, then compression.
RECIPES = [
    ("clean", []),
    ("blur-noise", ["blur", "noise"]),
    ("rotation", ["rotation"]),
    ("photo", ["rotation", "blur", "noise"]),
    ("paper", ["lighting", "texture"]),
    ("lowres", ["lowres"]),
    ("jpeg", ["jpeg", "noise"]),
    ("perspective", ["perspective"]),
    ("ruled", ["lines"]),
    ("scanned", ["rotation", "perspective", "lighting", "blur", "noise", "jpeg"]),
]

GEOMETRIC = {"rotation", "perspective", "lowres"}


# ---------------------------------------------------------------------------
# Rendering
# ---------------------------------------------------------------------------


def bank_labels(path: Path) -> set[str] | None:
    """Labels the shipped classifier can actually emit, read from the bank."""
    try:
        raw = path.read_bytes()
    except OSError:
        return None
    if raw[:4] != b"IGT1":
        return None
    count = int.from_bytes(raw[4:8], "little")
    labels: set[str] = set()
    offset = 8
    for _ in range(count):
        if offset >= len(raw):
            return None
        length = raw[offset]
        offset += 1
        labels.add(raw[offset : offset + length].decode("utf-8"))
        offset += length + 3  # fontIndex, width, height
        width = raw[offset - 2]
        height = raw[offset - 1]
        offset += width * height
    return labels


def _glyph_label(font, code: int, glyph_index: int) -> str | None:
    try:
        glyph_name = font.get_glyph_name(glyph_index).lower()
    except (ValueError, RuntimeError, AttributeError):
        glyph_name = ""
    if glyph_name in GLYPH_NAME_LABEL:
        return GLYPH_NAME_LABEL[glyph_name]
    char = chr(code)
    if 0x1D400 <= code <= 0x1D7FF:
        # Mathematical alphanumeric symbols: the stix/stixsans fontsets use
        # them for "italic x" and "bold 1"; NFKC maps them onto ASCII.
        char = unicodedata.normalize("NFKC", char)
        if len(char) != 1:
            return None
    return CHAR_LABEL.get(char)


def parse_layout(tex: str, fontset: str, size: float, dpi: int):
    """mathtext parse -> (width, height, glyphs, rules) in render pixels."""
    with matplotlib.rc_context({"mathtext.fontset": fontset}):
        parser = mathtext.MathTextParser("path")
        prop = FontProperties(size=size)
        parsed = parser.parse(tex, dpi=dpi, prop=prop)
        baseline = parsed.height - parsed.depth
        glyphs = []
        for font, font_size, code, glyph_index, ox, oy in parsed.glyphs:
            font.set_size(font_size, dpi)
            outline = font.load_char(code, flags=LoadFlags.NO_HINTING)
            # FreeType gives the outline box in 26.6 fixed point, in device
            # pixels for the size and dpi set above.
            x_min, y_min, x_max, y_max = (value / 64.0 for value in outline.bbox)
            x0 = ox + x_min
            x1 = ox + x_max
            top = baseline - (oy + y_max)
            bottom = baseline - (oy + y_min)
            glyphs.append(
                {
                    "char": chr(code),
                    "class": _glyph_label(font, code, glyph_index),
                    "box": [x0, top, x1, bottom],
                    "kind": "glyph",
                    "point_size": float(font_size),
                }
            )
        rules = []
        for x, y, width, height in parsed.rects:
            rules.append(
                {
                    "char": "",
                    "class": None,  # decided below from the ink around it
                    "box": [float(x), baseline - (y + height), float(x + width), baseline - y],
                    "kind": "rule",
                }
            )
    return float(parsed.width), float(parsed.height + parsed.depth), glyphs, rules


def _ink_bounds(image: Image.Image):
    """Top-left ink pixel of a render (the raster is cropped to its ink)."""
    ink = np.asarray(image) < 200
    if not ink.any():
        return None
    rows = np.flatnonzero(ink.any(axis=1))
    columns = np.flatnonzero(ink.any(axis=0))
    return int(columns[0]), int(rows[0])


def _wrap_space(body: str, left: bool) -> str:
    """Add a thin space inside the maths so a glyph is not cut by the crop."""
    if left:
        return body[:1] + r"\," + body[1:]
    return body[:-1] + r"\," + body[-1]


def render_tex(tex: str, fontset: str, size: float, dpi: int):
    """Render one mathtext line; returns (gray image, glyph list, rules).

    matplotlib crops the raster to its ink box, while the parse reports glyph
    positions in the layout box, so the boxes are shifted by the measured ink
    origin. A glyph with a negative side bearing would be cut by the crop;
    those lines are re-laid out with a thin space until nothing overhangs.
    """
    body = tex
    glyphs, rules = [], []
    for _attempt in range(3):
        width, _height, glyphs, rules = parse_layout(body, fontset, size, dpi)
        overhang_left = any(box["box"][0] < 0.5 for box in glyphs + rules)
        overhang_right = any(box["box"][2] > width - 0.5 for box in glyphs + rules)
        if not (overhang_left or overhang_right):
            break
        if overhang_left:
            body = _wrap_space(body, left=True)
        if overhang_right:
            body = _wrap_space(body, left=False)

    with matplotlib.rc_context({"mathtext.fontset": fontset}):
        buffer = io.BytesIO()
        mathtext.math_to_image(
            body, buffer, dpi=dpi, prop=FontProperties(size=size), format="png"
        )
    buffer.seek(0)
    image = Image.open(buffer).convert("L")

    origin = _ink_bounds(image)
    if origin is None:
        raise RuntimeError("render produced no ink")
    predicted = min(box["box"][0] for box in glyphs + rules)
    predicted_top = min(box["box"][1] for box in glyphs + rules)
    shift_x = origin[0] - predicted
    shift_y = origin[1] - predicted_top
    for box in glyphs + rules:
        x0, y0, x1, y1 = box["box"]
        box["box"] = [x0 + shift_x, y0 + shift_y, x1 + shift_x, y1 + shift_y]
    return image, glyphs, rules


def render_pil_glyph(char: str, font: ImageFont.FreeTypeFont):
    """Draw one character with PIL; returns (image, tight ink box) or None.

    Image.getbbox() reports the whole canvas for an "L" image (it treats white
    as data), so the ink extent is measured on the pixels themselves.
    """
    canvas = Image.new("L", (font.size * 3, font.size * 3), 255)
    draw = ImageDraw.Draw(canvas)
    draw.text((font.size, font.size), char, font=font, fill=0)
    ink = np.asarray(canvas) < 128
    if not ink.any():
        return None
    rows = np.flatnonzero(ink.any(axis=1))
    columns = np.flatnonzero(ink.any(axis=0))
    box = (int(columns[0]), int(rows[0]), int(columns[-1]) + 1, int(rows[-1]) + 1)
    return canvas.crop(box), box


def compose_page(parts, background: int = 255):
    """Paste (image, x, y, glyphs) parts onto one page; offset their boxes."""
    width = max(x + image.width for image, x, _, _ in parts)
    height = max(y + image.height for image, _, y, _ in parts)
    page = Image.new("L", (width, height), background)
    glyphs = []
    for image, x, y, part_glyphs in parts:
        page.paste(image, (x, y))
        for glyph in part_glyphs:
            x0, y0, x1, y1 = glyph["box"]
            glyph = dict(glyph)
            glyph["box"] = [x0 + x, y0 + y, x1 + x, y1 + y]
            glyphs.append(glyph)
    return page, glyphs


def _bracket(char: str, target_height: int):
    """Draw a bracket whose ink is about `target_height` tall."""
    path = MATRIX_BRACKET_FONT
    if not Path(path).exists():
        path = "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"
    probe = render_pil_glyph(char, ImageFont.truetype(path, 100))
    if probe is None:
        raise RuntimeError(f"no ink for bracket {char!r}")
    font_size = max(12, int(100 * target_height / probe[0].height))
    result = render_pil_glyph(char, ImageFont.truetype(path, font_size))
    if result is None:
        raise RuntimeError(f"no ink for bracket {char!r}")
    return result[0], font_size


def render_matrix(cells, fontset: str, size: float, dpi: int):
    """A bracketed grid; cells are mathtext, so their boxes stay exact."""
    grid = []
    for row in cells:
        rendered = []
        for cell in row:
            image, glyphs, _rules = render_tex(f"${cell}$", fontset, size * 0.72, dpi)
            rendered.append((image, glyphs))
        grid.append(rendered)

    gap_x = max(8, int(dpi * 0.16))
    gap_y = max(6, int(dpi * 0.12))
    columns = len(cells[0])
    column_width = [
        max(grid[r][c][0].width for r in range(len(cells))) for c in range(columns)
    ]
    row_height = [max(image.height for image, _ in row) for row in grid]
    grid_width = sum(column_width) + gap_x * (columns - 1)
    grid_height = sum(row_height) + gap_y * (len(cells) - 1)

    left, left_size = _bracket("[", int(grid_height * 1.08))
    right, _right_size = _bracket("]", int(grid_height * 1.08))
    margin = max(6, int(dpi * 0.10))

    page_width = margin + left.width + margin + grid_width + margin + right.width + margin
    page_height = max(left.height, grid_height, right.height) + 2 * margin
    parts = []
    # Brackets are real ink and stay in the ground truth, with their class.
    parts.append(
        (
            left,
            margin,
            (page_height - left.height) // 2,
            [
                {
                    "char": "[",
                    "class": CHAR_LABEL.get("["),
                    "box": [0.0, 0.0, float(left.width), float(left.height)],
                    "kind": "glyph",
                    "point_size": float(left_size),
                }
            ],
        )
    )
    parts.append(
        (
            right,
            page_width - margin - right.width,
            (page_height - right.height) // 2,
            [
                {
                    "char": "]",
                    "class": CHAR_LABEL.get("]"),
                    "box": [0.0, 0.0, float(right.width), float(right.height)],
                    "kind": "glyph",
                    "point_size": float(left_size),
                }
            ],
        )
    )
    x_grid = margin + left.width + margin
    y_grid = (page_height - grid_height) // 2
    for r, row in enumerate(grid):
        y = y_grid + sum(row_height[:r]) + gap_y * r
        x = x_grid
        for c, (image, glyphs) in enumerate(row):
            parts.append(
                (
                    image,
                    x + (column_width[c] - image.width) // 2,
                    y + (row_height[r] - image.height) // 2,
                    glyphs,
                )
            )
            x += column_width[c] + gap_x
    return compose_page(parts)


def render_system(lines, fontset: str, size: float, dpi: int):
    """Two independent equations stacked like a hand-written system."""
    parts = []
    y = 0
    gap = int(dpi * 0.28)
    for line in lines:
        image, glyphs, _rules = render_tex(line, fontset, size, dpi)
        parts.append((image, 0, y, glyphs))
        y += image.height + gap
    return compose_page(parts)


def render_expression(spec, fontset: str, size: float, dpi: int):
    layout = spec.get("layout")
    if layout == "matrix":
        return render_matrix(spec["cells"], fontset, size, dpi)
    if layout == "system":
        return render_system(spec["lines"], fontset, size, dpi)
    image, glyphs, rules = render_tex(spec["latex"], fontset, size, dpi)
    return image, glyphs + rules


# ---------------------------------------------------------------------------
# Ground-truth boxes
# ---------------------------------------------------------------------------


def _overlap(a0: float, a1: float, b0: float, b1: float) -> float:
    return min(a1, b1) - max(a0, b0)


def _rule_class(rule, glyphs) -> str | None:
    """A rule with glyphs on both sides is a fraction bar; otherwise structural.

    Decided from the glyph boxes, not from a pixel window: the overbar of a
    radical shares its left end with the radical hook, which spans both sides
    of the rule and so counts as neither above nor below it.
    """
    x0, y0, x1, y1 = rule["box"]
    above = below = False
    for glyph in glyphs:
        gx0, gy0, gx1, gy1 = glyph["box"]
        # The glyph has to sit mostly over the rule; scaling the test by the
        # rule's own width would exclude every glyph of a wide fraction bar.
        if _overlap(gx0, gx1, x0, x1) <= 0.5 * max(1.0, gx1 - gx0):
            continue
        if gy1 <= y0 + 2.0:
            above = True
        elif gy0 >= y1 - 2.0:
            below = True
    return "-" if (above and below) else None


def _merge_into_glyph(rule, glyphs):
    """Fold a structural rule (a radical overbar) into its glyph."""
    x0, y0, x1, y1 = rule["box"]
    candidates = [
        glyph
        for glyph in glyphs
        if _overlap(glyph["box"][1], glyph["box"][3], y0, y1) > 0.5 * max(1.0, y1 - y0)
    ]
    if not candidates:
        candidates = glyphs
    if not candidates:
        return None
    return min(candidates, key=lambda glyph: abs(glyph["box"][2] - x0))


def label_boxes(image: Image.Image, glyphs, rules):
    """Ground-truth symbols for one render, plus a check that they are honest.

    One box per typeset glyph. A fraction bar stays its own symbol because the
    scanner segments the bar as one too; a structural rule (the overbar of a
    radical) is folded into the glyph it belongs to, which is what the scanner
    sees as well. Glyphs that merely touch each other are *not* merged: an
    independent segmenter would split them, and pretending otherwise would make
    the segmentation score measure this script instead of the scanner.

    Returns (symbols, coverage, empty, folded) where `coverage` is the share of
    ink pixels inside some box; below ~0.99 the boxes do not describe the
    render and the caller must publish `"boxes": null`.
    """
    ink = np.asarray(image) < 128
    symbols = [
        {
            "class": glyph["class"],
            "box": [round(value, 1) for value in glyph["box"]],
            "kind": "glyph",
            "chars": glyph["char"],
        }
        for glyph in glyphs
    ]
    folded = 0
    for rule in rules:
        if _rule_class(rule, glyphs) == "-":
            symbols.append(
                {
                    "class": "-",
                    "box": [round(value, 1) for value in rule["box"]],
                    "kind": "fraction_bar",
                    "chars": "",
                }
            )
            continue
        glyph = _merge_into_glyph(rule, glyphs)
        if glyph is None:
            continue
        folded += 1
        for index, symbol in enumerate(symbols):
            if symbol["box"] == [round(value, 1) for value in glyph["box"]]:
                x0 = min(symbol["box"][0], rule["box"][0])
                y0 = min(symbol["box"][1], rule["box"][1])
                x1 = max(symbol["box"][2], rule["box"][2])
                y1 = max(symbol["box"][3], rule["box"][3])
                symbols[index] = {
                    "class": None if symbol["class"] is None else symbol["class"],
                    "box": [round(x0, 1), round(y0, 1), round(x1, 1), round(y1, 1)],
                    "kind": "radical",
                    "chars": symbol["chars"],
                }
                break

    symbols.sort(key=lambda symbol: (symbol["box"][1], symbol["box"][0]))
    covered = np.zeros_like(ink)
    for symbol in symbols:
        x0, y0, x1, y1 = [int(round(value)) for value in symbol["box"]]
        # One pixel of halo: an antialiased edge reaches just outside the
        # outline box, and a 5 px rule has two such edges.
        covered[max(0, y0 - 1) : y1 + 2, max(0, x0 - 1) : x1 + 2] = True
    total = int(ink.sum())
    coverage = float((ink & covered).sum()) / total if total else 1.0
    empty = [symbol for symbol in symbols if _ink_in(ink, symbol["box"]) == 0]
    return symbols, coverage, empty, folded


def _ink_in(ink: np.ndarray, box) -> int:
    x0, y0, x1, y1 = [int(round(v)) for v in box]
    return int(ink[max(0, y0) : y1, max(0, x0) : x1].sum())


# ---------------------------------------------------------------------------
# Augmentations
# ---------------------------------------------------------------------------


def _box_corners(box):
    x0, y0, x1, y1 = box["box"]
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def _map_boxes(boxes, forward, size):
    """Apply a point map to every box and clamp it to the new canvas."""
    width, height = size
    out = []
    for box in boxes:
        points = [forward(x, y) for x, y in _box_corners(box)]
        xs = [min(max(point[0], 0.0), width - 1.0) for point in points]
        ys = [min(max(point[1], 0.0), height - 1.0) for point in points]
        mapped = dict(box)
        mapped["box"] = [
            round(min(xs), 1),
            round(min(ys), 1),
            round(max(xs), 1),
            round(max(ys), 1),
        ]
        out.append(mapped)
    return out


def rotate_page(image, boxes, angle_deg, fill=255):
    """Rotate about the centre and grow the canvas, so nothing is clipped."""
    if abs(angle_deg) < 1e-6:
        return image, boxes, {"name": "rotation", "angle_deg": 0.0}
    radians = math.radians(angle_deg)
    ca, sa = math.cos(radians), math.sin(radians)
    width, height = image.size
    new_width = int(math.ceil(abs(width * ca) + abs(height * sa))) + 1
    new_height = int(math.ceil(abs(width * sa) + abs(height * ca))) + 1
    source = ((width - 1) / 2.0, (height - 1) / 2.0)
    target = ((new_width - 1) / 2.0, (new_height - 1) / 2.0)
    coefficients = (
        ca,
        sa,
        source[0] - ca * target[0] - sa * target[1],
        -sa,
        ca,
        source[1] + sa * target[0] - ca * target[1],
    )
    rotated = image.transform(
        (new_width, new_height),
        Image.AFFINE,
        coefficients,
        resample=Image.BICUBIC,
        fillcolor=fill,
    )

    def forward(x, y):
        dx, dy = x - source[0], y - source[1]
        return (ca * dx - sa * dy + target[0], sa * dx + ca * dy + target[1])

    return (
        rotated,
        _map_boxes(boxes, forward, (new_width, new_height)),
        {"name": "rotation", "angle_deg": round(angle_deg, 3)},
    )


def homography(source, target):
    """Plane projective map taking `source` corners onto `target` corners."""
    matrix = []
    for (x, y), (u, v) in zip(source, target):
        matrix.append([x, y, 1, 0, 0, 0, -u * x, -u * y, -u])
        matrix.append([0, 0, 0, x, y, 1, -v * x, -v * y, -v])
    a = np.array(matrix, dtype=float)
    _, _, vt = np.linalg.svd(a)
    h = vt[-1].reshape(3, 3)
    # The null vector is defined up to a sign and a scale; normalising the
    # bottom-right entry makes the map usable and unambiguous.
    if abs(h[2, 2]) < 1e-12:
        raise RuntimeError("degenerate perspective")
    return h / h[2, 2]


def perspective_page(image, boxes, strength, rng):
    """Keystone distortion: camera not square to the page."""
    width, height = image.size
    source = [(0, 0), (width - 1, 0), (width - 1, height - 1), (0, height - 1)]
    jitter = strength * min(width, height)
    target = [
        (x + rng.uniform(-jitter, jitter), y + rng.uniform(-jitter, jitter))
        for x, y in source
    ]
    forward = homography(source, target)
    inverse = np.linalg.inv(forward)
    inverse /= inverse[2, 2]
    warped = image.transform(
        (width, height),
        Image.PERSPECTIVE,
        tuple(inverse.flatten()[:8]),
        resample=Image.BICUBIC,
        fillcolor=255,
    )

    def map_point(x, y):
        vector = forward @ np.array([x, y, 1.0])
        return (vector[0] / vector[2], vector[1] / vector[2])

    return (
        warped,
        _map_boxes(boxes, map_point, (width, height)),
        {
            "name": "perspective",
            "strength": round(strength, 4),
            "corners": [[round(px, 1), round(py, 1)] for px, py in target],
        },
    )


def lowres_page(image, boxes, factor):
    """Downscale and scale back: the scanner sees less detail, same geometry."""
    width, height = image.size
    small = image.resize(
        (max(8, int(width * factor)), max(8, int(height * factor))), Image.BILINEAR
    )
    restored = small.resize((width, height), Image.BICUBIC)
    return (
        restored,
        boxes,
        {"name": "lowres", "factor": round(factor, 3)},
    )


def blur_page(image, boxes, radius):
    return image.filter(ImageFilter.GaussianBlur(radius)), boxes, {
        "name": "blur",
        "radius": round(radius, 3),
    }


def noise_page(image, boxes, sigma, rng):
    pixels = np.asarray(image, dtype=np.float32)
    pixels = pixels + rng.normal(0.0, sigma, pixels.shape)
    return (
        Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)),
        boxes,
        {"name": "noise", "sigma": round(sigma, 3)},
    )


def lighting_page(image, boxes, strength, rng):
    """Uneven lamp light plus a soft shadow edge across the page."""
    pixels = np.asarray(image, dtype=np.float32)
    height, width = pixels.shape
    ramp = np.linspace(-1.0, 1.0, width, dtype=np.float32)[None, :]
    ramp = ramp * rng.uniform(0.5, 1.0) + rng.uniform(-0.3, 0.3)
    vertical = np.linspace(-1.0, 1.0, height, dtype=np.float32)[:, None]
    field = (1.0 * ramp + 0.6 * vertical) * strength
    pixels = pixels * (1.0 - field)  # darker where the light falls off
    if rng.random() < 0.7:
        edge = rng.uniform(0.45, 0.8) * min(width, height)
        shadow = np.clip((np.arange(height, dtype=np.float32) - edge) / max(1.0, height * 0.25), 0, 1)
        pixels = pixels * (1.0 - 0.35 * strength * 4 * shadow[:, None] * 0.25)
    return (
        Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)),
        boxes,
        {"name": "lighting", "strength": round(strength, 3)},
    )


def texture_page(image, boxes, strength, rng):
    """Paper fibre: a smooth low-frequency field plus fine grain."""
    pixels = np.asarray(image, dtype=np.float32)
    height, width = pixels.shape
    small = rng.normal(0.0, 1.0, (max(2, height // 24), max(2, width // 24)))
    coarse = Image.fromarray(((small - small.min()) / (np.ptp(small) + 1e-6) * 255).astype(np.uint8))
    coarse = coarse.resize((width, height), Image.BICUBIC)
    field = np.asarray(coarse, dtype=np.float32) / 255.0 - 0.5
    grain = rng.normal(0.0, 1.0, pixels.shape)
    pixels = pixels + strength * (18.0 * field + 3.0 * grain)
    return (
        Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)),
        boxes,
        {"name": "texture", "strength": round(strength, 3)},
    )


def ruled_lines_page(image, boxes, spacing, shade, rng):
    """Notebook rules across the page; the preprocess stage should drop them."""
    draw = ImageDraw.Draw(image)
    height, width = image.size[1], image.size[0]
    spacing = max(10, int(spacing))
    start = int(rng.uniform(0, spacing))
    y = start
    while y < height:
        # Notebook rules are one or two pixels of light grey at this scale; a
        # heavy bar through every glyph would test nothing but the eraser.
        draw.line([(0, y), (width, y)], fill=shade, width=max(1, spacing // 48))
        y += spacing
    draw.rectangle([0, 0, width - 1, height - 1], outline=shade)
    return image, boxes, {"name": "lines", "spacing": spacing, "shade": shade}


def build_chain(index: int, only: str | None, recipe: str | None = None):
    """Pick an augmentation recipe; `--only`/`--recipe` narrow the choice."""
    if recipe is not None:
        for name, steps in RECIPES:
            if name == recipe:
                return name, list(steps)
        raise SystemExit(
            f"unknown recipe {recipe!r}; known: " + ", ".join(name for name, _ in RECIPES)
        )
    name, steps = RECIPES[index % len(RECIPES)]
    if only is None:
        return name, list(steps)
    if only == "clean":
        return "clean", []
    candidates = [recipe for recipe in RECIPES if only in recipe[1]]
    if not candidates:
        raise SystemExit(
            f"unknown augmentation {only!r}; known: "
            + ", ".join(sorted({step for _, steps in RECIPES for step in steps} | {"clean"}))
        )
    name, steps = candidates[index % len(candidates)]
    return name, list(steps)


def apply_chain(image, boxes, steps, rng, geometric_only=False):
    """Run the chain, returning the page, the moved boxes and the parameters."""
    applied = []
    for step in steps:
        if geometric_only and step not in GEOMETRIC:
            continue
        if step == "rotation":
            image, boxes, meta = rotate_page(image, boxes, rng.uniform(-8.0, 8.0))
        elif step == "perspective":
            image, boxes, meta = perspective_page(image, boxes, rng.uniform(0.01, 0.04), rng)
        elif step == "lowres":
            image, boxes, meta = lowres_page(image, boxes, rng.uniform(0.30, 0.55))
        elif step == "blur":
            image, boxes, meta = blur_page(image, boxes, rng.uniform(0.6, 1.8))
        elif step == "noise":
            image, boxes, meta = noise_page(image, boxes, rng.uniform(3.0, 12.0), rng)
        elif step == "lighting":
            image, boxes, meta = lighting_page(image, boxes, rng.uniform(0.15, 0.5), rng)
        elif step == "texture":
            image, boxes, meta = texture_page(image, boxes, rng.uniform(0.4, 1.0), rng)
        elif step == "lines":
            image, boxes, meta = ruled_lines_page(
                image, boxes, rng.uniform(46, 96), int(rng.uniform(185, 220)), rng
            )
        elif step == "jpeg":
            quality = int(rng.uniform(35, 85))
            image, boxes, meta = image, boxes, {"name": "jpeg", "quality": quality}
        else:  # pragma: no cover - guarded by build_chain
            raise SystemExit(f"unknown augmentation step {step!r}")
        applied.append(meta)
    return image, boxes, applied


# ---------------------------------------------------------------------------
# Dataset assembly
# ---------------------------------------------------------------------------


def pad_page(image, glyphs, pad):
    page = Image.new("L", (image.width + 2 * pad, image.height + 2 * pad), 255)
    page.paste(image, (pad, pad))
    padded = []
    for glyph in glyphs:
        glyph = dict(glyph)
        x0, y0, x1, y1 = glyph["box"]
        glyph["box"] = [round(x0 + pad, 1), round(y0 + pad, 1), round(x1 + pad, 1), round(y1 + pad, 1)]
        padded.append(glyph)
    return page, padded


def entries(args):
    """Deterministic list of (expression spec, fontset, size, dpi, recipe)."""
    plan = []
    index = 0
    for spec in EXPRESSIONS:
        for variant in range(args.variants):
            fontset, size, dpi = VARIANTS[(index + variant) % len(VARIANTS)]
            seed = args.seed + index * 7919 + variant * 104729
            recipe, steps = build_chain(index + variant, args.only, args.recipe)
            plan.append(
                {
                    "spec": spec,
                    "fontset": fontset,
                    "size": float(size),
                    "dpi": int(dpi),
                    "recipe": recipe,
                    "steps": steps,
                    "seed": seed,
                }
            )
            index += 1
    return plan


def generate(args) -> dict:
    outdir = Path(args.outdir)
    for sub in ("images", "clean", "ref"):
        (outdir / sub).mkdir(parents=True, exist_ok=True)

    known = bank_labels(Path(args.bank)) if args.bank else None
    plan = entries(args)
    if args.limit:
        plan = plan[: args.limit]

    images = []
    for position, item in enumerate(plan):
        spec = item["spec"]
        ident = f"{position:04d}"
        rng = np.random.default_rng(item["seed"])
        try:
            render, glyphs = render_expression(
                spec, item["fontset"], item["size"], item["dpi"]
            )
        except Exception as error:  # no rendering, no entry
            print(f"{ident} skip {spec['name']}: {error}", file=sys.stderr)
            continue
        pad = 24 + int(rng.integers(0, 48))
        page, glyphs = pad_page(render, glyphs, pad)
        page.save(outdir / "clean" / f"{ident}.png")

        symbols, coverage, empty, folded = label_boxes(
            page,
            [glyph for glyph in glyphs if glyph["kind"] == "glyph"],
            [glyph for glyph in glyphs if glyph["kind"] != "glyph"],
        )
        boxes_ok = (
            bool(symbols)
            and coverage >= 0.99
            and not empty
            and all(
                symbol["class"] is None or known is None or symbol["class"] in known
                for symbol in symbols
            )
        )

        # The reference has to go through exactly the same geometric steps as
        # the image, so run the chain on a fresh copy with the same seed.
        ref_page, _, _ = apply_chain(
            page.copy(), [], item["steps"], np.random.default_rng(item["seed"]),
            geometric_only=True,
        )
        ref_page.save(outdir / "ref" / f"{ident}.png")

        final, moved, applied = apply_chain(
            page.copy(), symbols, item["steps"], np.random.default_rng(item["seed"])
        )
        if _ink_in(np.asarray(final) < 128, [0, 0, final.width, final.height]) == 0:
            print(f"{ident} skip {spec['name']}: augmentation removed all ink", file=sys.stderr)
            continue
        jpeg = next((step for step in applied if step["name"] == "jpeg"), None)
        extension = "jpg" if jpeg else "png"
        path = outdir / "images" / f"{ident}.{extension}"
        if jpeg:
            final.save(path, quality=jpeg["quality"], subsampling=1)
        else:
            final.save(path)

        record = {
            "id": ident,
            "path": str(path.relative_to(outdir)),
            "clean": str((outdir / "clean" / f"{ident}.png").relative_to(outdir)),
            "ref": str((outdir / "ref" / f"{ident}.png").relative_to(outdir)),
            "expression": spec["name"],
            "domain": spec["domain"],
            "latex": spec["latex"],
            "expect_text": spec["expect_text"],
            "expect_kind": spec["expect_kind"],
            "fontset": item["fontset"],
            "point_size": item["size"],
            "dpi": item["dpi"],
            "seed": item["seed"],
            "size": list(final.size),
            "recipe": item["recipe"],
            "augmentations": applied,
            "rules_folded": folded,
            "boxes_coverage": round(coverage, 4),
            "boxes": moved if boxes_ok else None,
        }
        if not boxes_ok:
            print(
                f"{ident} boxes dropped for {spec['name']}: coverage={coverage:.3f} "
                f"empty={len(empty)} symbols={len(symbols)}",
                file=sys.stderr,
            )
        images.append(record)
        print(
            f"{ident} {spec['name']:<16} {item['fontset']:<12} {item['size']:>4.0f}pt "
            f"{item['dpi']}dpi {final.size[0]}x{final.size[1]} "
            f"[{'+'.join(step['name'] for step in applied) or 'clean'}] "
            f"symbols={len(symbols) if boxes_ok else 'n/a'}"
        )

    manifest = {
        "version": 1,
        "generator": "scanner/tools/make_dataset.py",
        "seed": args.seed,
        "limit": args.limit,
        "only": args.only,
        "recipe": args.recipe,
        "variants": args.variants,
        "fonts": FONTSETS,
        "augmentations": sorted({step for _, steps in RECIPES for step in steps}),
        "recipes": [{"name": name, "steps": steps} for name, steps in RECIPES],
        "expressions": EXPRESSIONS,
        "images": images,
    }
    manifest_path = outdir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=1, sort_keys=True) + "\n")
    boxed = sum(1 for image in images if image["boxes"])
    print(
        f"wrote {len(images)} images ({boxed} with boxes) and {manifest_path}"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--outdir",
        default=str(REPO / "scanner" / "dataset"),
        help="target directory (default: scanner/dataset, git-ignored)",
    )
    parser.add_argument("--limit", type=int, default=0, help="stop after N images")
    parser.add_argument("--only", default=None, help="only chains that use this augmentation")
    parser.add_argument(
        "--recipe", default=None, help="use one recipe for every image (see RECIPES)"
    )
    parser.add_argument("--seed", type=int, default=20260101, help="master seed")
    parser.add_argument("--variants", type=int, default=2, help="renders per expression")
    parser.add_argument(
        "--bank",
        default=str(DEFAULT_BANK),
        help="template bank, read for the label set (default: scanner/data)",
    )
    args = parser.parse_args()
    if args.variants < 1:
        raise SystemExit("--variants must be at least 1")
    generate(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
