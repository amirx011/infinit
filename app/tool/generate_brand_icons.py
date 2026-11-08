#!/usr/bin/env python3
"""Regenerate the app icons from the source mark.

The launcher icon is a static Android resource, so a palette change at runtime
cannot recolor it in place.  This script pre-renders one adaptive/legacy icon
per accent palette; the Android side switches between them with activity
aliases (see MainActivity.set_launcher_icon).

Run from the app directory:

    python3 tool/generate_brand_icons.py
"""

from __future__ import annotations

import pathlib

from PIL import Image, ImageDraw

APP = pathlib.Path(__file__).resolve().parents[1]
RES = APP / "android" / "app" / "src" / "main" / "res"
BRANDING = APP / "assets" / "branding"

# Foreground mark colours taken from OdePalette in lib/src/theme.dart.
PALETTES = {
    "ocean": ("#5CC8FF", "#5EEAD4"),
    "indigo": ("#818CF8", "#C084FC"),
    "emerald": ("#34D399", "#2DD4BF"),
    "amber": ("#FBBF24", "#FB923C"),
    "rose": ("#FB7185", "#F0ABFC"),
    "graphite": ("#A8B6CC", "#7DD3FC"),
}
DEFAULT = "rose"

DENSITIES = {
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}


def hex_rgb(value: str) -> tuple[int, int, int]:
    value = value.lstrip("#")
    return tuple(int(value[i : i + 2], 16) for i in (0, 2, 4))


def lerp(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def mark_bbox(image: Image.Image) -> tuple[int, int, int, int]:
    alpha = image.getchannel("A")
    return alpha.getbbox() or (0, 0, image.width, image.height)


def recolor(source: Image.Image, accent: str, alt: str) -> Image.Image:
    """Replace the mark's blue gradient with a palette gradient, keeping alpha."""
    out = Image.new("RGBA", source.size)
    a = hex_rgb(accent)
    b = hex_rgb(alt)
    left, _top, right, _bottom = mark_bbox(source)
    span = max(1, right - left)
    src = source.load()
    dst = out.load()
    for y in range(source.height):
        for x in range(source.width):
            r, g, bl, alpha = src[x, y]
            if alpha == 0:
                continue
            t = (x - left) / span
            cr, cg, cb = lerp(a, b, max(0.0, min(1.0, t)))
            dst[x, y] = (cr, cg, cb, alpha)
    return out


def background(size: int, round_icon: bool) -> Image.Image:
    """Dark radial backdrop, clipped to the legacy launcher shape."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    gradient = Image.new("RGBA", (size, size))
    px = gradient.load()
    corner = hex_rgb("#050810")
    centre = hex_rgb("#0E1B3A")
    cx, cy = size * 0.5, size * 0.45
    radius = size * 0.75
    for y in range(size):
        for x in range(size):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 / radius
            t = min(1.0, d)
            px[x, y] = (*lerp(centre, corner, t), 255)

    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    if round_icon:
        draw.ellipse((0, 0, size - 1, size - 1), fill=255)
    else:
        radius = round(size * 0.22)
        draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=255)
    img.paste(gradient, (0, 0), mask)
    return img


def legacy_icon(foreground: Image.Image, size: int, round_icon: bool) -> Image.Image:
    base = background(size, round_icon)
    mark = foreground.resize((size, size), Image.LANCZOS)
    base.alpha_composite(mark)
    return base


def main() -> None:
    source_foreground = Image.open(RES / "drawable-xxxhdpi" / "ic_launcher_foreground.png").convert("RGBA")
    monochrome = Image.open(RES / "drawable-xxxhdpi" / "ic_launcher_monochrome.png").convert("RGBA")

    # Per-density foregrounds for every palette.
    for density, size in DENSITIES.items():
        src_path = RES / f"drawable-{density}" / "ic_launcher_foreground.png"
        src = Image.open(src_path).convert("RGBA")
        for palette, (accent, alt) in PALETTES.items():
            recolor(src, accent, alt).save(
                RES / f"drawable-{density}" / f"ic_launcher_foreground_{palette}.png"
            )

    # Adaptive icon XML per palette.
    for palette in PALETTES:
        for suffix in ("", "_round"):
            name = f"ic_launcher_{palette}{suffix}"
            xml = f"""<?xml version="1.0" encoding="utf-8"?>
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@drawable/ic_launcher_background" />
    <foreground android:drawable="@drawable/ic_launcher_foreground_{palette}" />
    <monochrome android:drawable="@drawable/ic_launcher_monochrome" />
</adaptive-icon>
"""
            (RES / "mipmap-anydpi-v26" / f"{name}.xml").write_text(xml)

    # Legacy mipmaps per palette.
    for density, size in DENSITIES.items():
        for palette, (accent, alt) in PALETTES.items():
            foreground = recolor(source_foreground, accent, alt)
            legacy_icon(foreground, size, False).save(
                RES / f"mipmap-{density}" / f"ic_launcher_{palette}.png"
            )
            legacy_icon(foreground, size, True).save(
                RES / f"mipmap-{density}" / f"ic_launcher_{palette}_round.png"
            )

    # Make rose the packaged default everywhere.
    rose_fg = recolor(source_foreground, *PALETTES[DEFAULT])
    for density, size in DENSITIES.items():
        rose_fg.save(RES / f"drawable-{density}" / "ic_launcher_foreground.png")
        legacy_icon(rose_fg, size, False).save(RES / f"mipmap-{density}" / "ic_launcher.png")
        legacy_icon(rose_fg, size, True).save(RES / f"mipmap-{density}" / "ic_launcher_round.png")
    monochrome.save(RES / "drawable-xxxhdpi" / "ic_launcher_monochrome.png")

    # The splash logo and the Linux/window icon follow the default palette.
    legacy_icon(rose_fg, 512, False).save(RES / "drawable-nodpi" / "launch_logo.png")
    for size in (16, 24, 32, 48, 64, 96, 128, 192, 256, 512, 1024):
        legacy_icon(rose_fg, size, False).save(BRANDING / f"infinit-icon-{size}.png")

    source_logo = Image.open(BRANDING / "infinit-logo-1024.png").convert("RGBA")
    rose_logo = recolor(source_logo, *PALETTES[DEFAULT])
    for size in (16, 24, 32, 48, 64, 96, 128, 192, 256, 512, 1024):
        rose_logo.resize((size, size), Image.LANCZOS).save(
            BRANDING / f"infinit-logo-{size}.png"
        )

    # The in-app mark is used as an alpha mask (ShaderMask srcIn), so keep its
    # shape and only refresh the colour for non-masked uses.
    source_mark = Image.open(BRANDING / "infinit-mark.png").convert("RGBA")
    recolor(source_mark, *PALETTES[DEFAULT]).save(BRANDING / "infinit-mark.png")


if __name__ == "__main__":
    main()
