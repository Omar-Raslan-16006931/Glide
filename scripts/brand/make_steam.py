#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
"""Steam store and library art in the Glide brand (graphite, logo green, the classic logo),
at the sizes Steamworks asks for. The Sunset City look is only for the app and website around
GTA VI's release; store art, promos and the repository use the brand.

Steam's rules for capsules: the product name or logo only, no review scores, awards, prices
or other text. Writes into packaging/steam/art/. Run: python3 scripts/brand/make_steam.py
"""
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "packaging/steam/art"
LOGO = Image.open(ROOT / "assets/pushfps-logo.png").convert("RGBA")
ICON = Image.open(ROOT / "assets/pushfps-icon.png").convert("RGBA")
GREEN = (137, 245, 42)

# name: (width, height, with logo)
SIZES = {
    "header_capsule": (920, 430, True),
    "small_capsule": (462, 174, True),
    "main_capsule": (1232, 706, True),
    "vertical_capsule": (748, 896, True),
    "page_background": (1438, 810, False),
    "library_capsule": (600, 900, True),
    "library_header": (920, 430, True),
    "library_hero": (3840, 1240, False),
}


def scene(w, h, seed=5):
    """Graphite with a logo-green glow and motion streaks, like the site's classic look."""
    img = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line((0, y, w, y), fill=(int(13 + 5 * t), int(16 + 5 * t), int(12 + 4 * t), 255))
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy, r0 = w * 0.62, h * 0.45, max(w, h) * 0.55
    for i in range(60, 0, -1):
        r = r0 * i / 60
        gd.ellipse((cx - r, cy - r * 0.8, cx + r, cy + r * 0.8), fill=GREEN + (int(40 * (1 - i / 60) ** 2),))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(max(w, h) * 0.02)))
    rnd = random.Random(seed)
    streaks = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    sd = ImageDraw.Draw(streaks)
    scale = max(w, h) / 1600
    for _ in range(int(110 * w * h / (1600 * 900)) + 20):
        x, y = rnd.uniform(-0.1 * w, w), rnd.uniform(0, h)
        ln = rnd.uniform(40, 170) * scale
        green = rnd.random() < 0.35
        col = GREEN if green else (220, 228, 236)
        for k in range(12):
            a = int((26 if green else 14) * (k + 1) / 12)
            sd.line((x + ln * k / 12, y, x + ln * (k + 1) / 12, y - ln * 0.03), fill=col + (a,), width=max(1, int(2 * scale)))
    img.alpha_composite(streaks)
    return img


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (w, h, with_logo) in SIZES.items():
        img = scene(w, h)
        if with_logo:
            tall = h > w
            lw = int(w * (0.88 if tall else (0.94 if w < 500 else 0.72)))
            logo = LOGO.resize((lw, int(LOGO.height * lw / LOGO.width)), Image.LANCZOS)
            shadow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            cy = int(h * (0.42 if tall else 0.5))
            shadow.alpha_composite(logo, ((w - lw) // 2, cy - logo.height // 2))
            black = Image.new("RGBA", (w, h), (0, 0, 0, 0))
            black.putalpha(shadow.split()[3].point(lambda v: int(v * 0.6)))
            img.alpha_composite(black.filter(ImageFilter.GaussianBlur(max(4, w // 90))))
            img.alpha_composite(logo, ((w - lw) // 2, cy - logo.height // 2))
        img.convert("RGB").save(OUT / f"{name}.png", optimize=True)
    canvas = Image.new("RGBA", (1280, 720), (0, 0, 0, 0))
    logo = LOGO.resize((1240, int(LOGO.height * 1240 / LOGO.width)), Image.LANCZOS)
    canvas.alpha_composite(logo, ((1280 - logo.width) // 2, (720 - logo.height) // 2))
    canvas.save(OUT / "library_logo.png", optimize=True)
    bg = Image.new("RGBA", (184, 184), (14, 17, 13, 255))
    bg.alpha_composite(ICON.resize((164, 164), Image.LANCZOS), (10, 10))
    bg.convert("RGB").save(OUT / "community_icon.png", optimize=True)
    print("steam art written to", OUT)


if __name__ == "__main__":
    main()
