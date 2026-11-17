#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
"""Glide icon: the two chevrons of the full logo (assets/pushfps-logo.png) on the dark tile.

The chevron outlines are traced from the logo artwork (logo pixel coordinates below): the first
chevron's lower arm sweeps out into the logo's swoosh point, the second is a bevelled chevron. Both
are filled with the logo's top-to-bottom green and drawn 4x supersampled.

Writes assets/pushfps-icon.png (1024), assets/veyra-app-icon.png (512), assets/veyra.ico (the
executable's icon, 16-256 px), qml/Veyra/logo.png (the chevrons alone, 360x240, used in the app) and
the website's favicons. Run: python3 scripts/brand/make_icon.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]

# Traced from assets/pushfps-logo.png (1200x410).
FIRST = [(1065, 208), (1099, 208), (1139, 256), (1097, 311), (1040, 338), (1102, 256)]
SECOND = [(1121, 207), (1148, 207), (1188, 255), (1145, 311), (1119, 310), (1160, 254)]
TOP_GREEN = (178, 247, 82)     # sampled near the top of the logo's chevrons
BOTTOM_GREEN = (108, 236, 6)   # and near the bottom
TILE = (12, 14, 18)            # the existing icon's tile colour
SS = 4                         # supersampling


def bounds():
    xs = [x for x, _ in FIRST + SECOND]
    ys = [y for _, y in FIRST + SECOND]
    return min(xs), min(ys), max(xs), max(ys)


def centroid():
    """Area centroid of both chevrons together (shoelace formula)."""
    A = Cx = Cy = 0.0
    for poly in (FIRST, SECOND):
        for (xa, ya), (xb, yb) in zip(poly, poly[1:] + poly[:1]):
            cross = xa * yb - xb * ya
            A += cross
            Cx += (xa + xb) * cross
            Cy += (ya + yb) * cross
    return Cx / (3 * A), Cy / (3 * A)


def chevrons(width, height, fill_ratio, center_y=0.5, optical=False):
    """RGBA image of the two chevrons, fitted into width x height."""
    x0, y0, x1, y1 = bounds()
    bw, bh = x1 - x0, y1 - y0
    W, H = width * SS, height * SS
    scale = min(W * fill_ratio / bw, H * fill_ratio / bh)
    # Centre halfway between the bounding box and the shape's area centroid: the swoosh tail
    # leaves the lower left light, so pure box centring makes the mark look high and right.
    cx, cy = centroid()
    mx, my = ((x0 + x1) / 2 + cx) / 2, ((y0 + y1) / 2 + cy) / 2
    if not optical:
        mx, my = (x0 + x1) / 2, (y0 + y1) / 2
    ox = W / 2 - mx * scale
    oy = H * center_y - my * scale
    mask = Image.new("L", (W, H), 0)
    draw = ImageDraw.Draw(mask)
    for poly in (FIRST, SECOND):
        draw.polygon([(x * scale + ox, y * scale + oy) for x, y in poly], fill=255)
    # Vertical gradient over the chevrons' own height.
    top, bottom = y0 * scale + oy, y1 * scale + oy
    grad = Image.new("RGB", (1, H))
    for y in range(H):
        t = min(1.0, max(0.0, (y - top) / max(1.0, bottom - top)))
        grad.putpixel((0, y), tuple(round(a + (b - a) * t) for a, b in zip(TOP_GREEN, BOTTOM_GREEN)))
    colour = grad.resize((W, H))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste(colour, (0, 0), mask)
    return out.resize((width, height), Image.LANCZOS)


def tile(size):
    W = size * SS
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle((0, 0, W - 1, W - 1), radius=int(W * 0.215), fill=TILE + (255,))
    img = img.resize((size, size), Image.LANCZOS)
    # Small sizes need the chevrons larger to stay readable.
    ratio = 0.60 if size >= 64 else 0.70
    img.alpha_composite(chevrons(size, size, ratio, optical=True))
    return img


def main():
    tile(1024).save(ROOT / "assets/pushfps-icon.png", optimize=True)
    tile(512).save(ROOT / "assets/veyra-app-icon.png", optimize=True)
    sizes = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
    big = tile(256)
    frames = [tile(s) for s in sizes]
    big.save(ROOT / "assets/veyra.ico", format="ICO", sizes=[(s, s) for s in sizes], append_images=frames)
    chevrons(360, 240, 0.92).save(ROOT / "qml/Veyra/logo.png", optimize=True)
    web = ROOT / "website/assets"
    if web.is_dir():
        tile(180).save(web / "apple-touch-icon.png", optimize=True)
        tile(64).save(web / "favicon-64.png", optimize=True)
        tile(32).save(web / "favicon-32.png", optimize=True)
        chevrons(96, 96, 0.92).save(web / "mark.png", optimize=True)
    print("icons written")


if __name__ == "__main__":
    main()
