#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
"""The Glide "Sunset City" look: the logo filled with a neon sunset over a palm-lined city, and
the layers of the startup intro (qml/Veyra/PushIntro.qml) and the website.

Everything here is drawn from scratch (gradients, a striped sun, a procedural skyline, palm
silhouettes, a perspective road grid); no game artwork or trademarks are used.

Writes into qml/Veyra/ (app) and website/assets/ (site). Run: python3 scripts/brand/make_sunset.py
"""
import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[2]
QML = ROOT / "qml/Veyra"
WEB = ROOT / "website/assets"
LOGO = Image.open(ROOT / "assets/pushfps-logo.png").convert("RGBA")   # 1200x410
LW, LH = LOGO.size
S = 2                                                                  # logo art scale (2400x820)

# Palette: night violet -> magenta -> hot pink -> coral -> gold at the horizon, teal water.
SKY = [(0.00, (22, 8, 52)), (0.30, (78, 22, 130)), (0.52, (196, 38, 150)), (0.66, (255, 61, 139)),
       (0.80, (255, 128, 72)), (0.92, (255, 196, 92)), (1.00, (255, 226, 140))]
SUN_TOP, SUN_BOTTOM = (255, 225, 120), (255, 52, 140)
BUILDING_BACK, BUILDING_FRONT = (88, 26, 126), (28, 8, 56)
WINDOWS = [(255, 120, 200), (120, 230, 255), (255, 210, 120)]
STROKE = (255, 236, 248)
GLOW_IN, GLOW_OUT = (255, 70, 170), (150, 40, 190)
OUTLINE = (24, 8, 46)


def gradient(w, h, stops, y0=0, y1=None):
    y1 = h if y1 is None else y1
    col = np.zeros((h, 3), np.float32)
    for y in range(h):
        t = min(1.0, max(0.0, (y - y0) / max(1, y1 - y0)))
        for (ta, ca), (tb, cb) in zip(stops, stops[1:]):
            if ta <= t <= tb:
                k = (t - ta) / max(1e-6, tb - ta)
                col[y] = [ca[i] + (cb[i] - ca[i]) * k for i in range(3)]
                break
    return Image.fromarray(np.repeat(col[:, None, :], w, axis=1).astype(np.uint8), "RGB").convert("RGBA")


def sun(d, scale=1.0, glow=True):
    """A striped sun: gold on top, hot pink at the bottom, gaps widening towards the horizon."""
    D = int(d * scale)
    pad = int(D * 0.45) if glow else 0
    out = Image.new("RGBA", (D + 2 * pad, D + 2 * pad), (0, 0, 0, 0))
    disc = gradient(D, D, [(0, SUN_TOP), (0.55, (255, 150, 90)), (1, SUN_BOTTOM)])
    mask = Image.new("L", (D, D), 0)
    md = ImageDraw.Draw(mask)
    md.ellipse((0, 0, D - 1, D - 1), fill=255)
    for i in range(7):                                   # the gaps
        y = D * (0.52 + i * 0.07)
        hgt = D * (0.008 + i * 0.006)
        md.rectangle((0, y, D, y + hgt), fill=0)
    if glow:
        halo = Image.new("L", out.size, 0)
        ImageDraw.Draw(halo).ellipse((pad, pad, pad + D, pad + D), fill=200)
        halo = halo.filter(ImageFilter.GaussianBlur(D * 0.16))
        tint = Image.new("RGBA", out.size, (255, 110, 120, 0))
        tint.putalpha(halo.point(lambda v: int(v * 0.75)))
        out.alpha_composite(tint)
    disc.putalpha(mask)
    out.alpha_composite(disc, (pad, pad))
    return out


def skyline(w, h, seed, base_color, windows=True, max_frac=0.95, min_frac=0.25, density=1.0):
    """A procedural coastal skyline: slim towers, stepped art-deco tops, a few spires."""
    rnd = random.Random(seed)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x = -10
    while x < w:
        bw = int(rnd.uniform(0.018, 0.05) * w / density)
        bh = int(rnd.uniform(min_frac, max_frac) ** 1.4 * h)
        top = h - bh
        d.rectangle((x, top, x + bw, h), fill=base_color + (255,))
        style = rnd.random()
        if style < 0.28:                                  # stepped deco top
            s1 = int(bw * 0.18)
            d.rectangle((x + s1, top - bh * 0.06, x + bw - s1, top), fill=base_color + (255,))
            d.rectangle((x + 2 * s1, top - bh * 0.11, x + bw - 2 * s1, top), fill=base_color + (255,))
        elif style < 0.4:                                 # spire
            cx = x + bw / 2
            d.polygon([(cx - bw * 0.12, top), (cx, top - bh * 0.22), (cx + bw * 0.12, top)], fill=base_color + (255,))
        elif style < 0.5:                                 # rounded crown
            d.ellipse((x, top - bw * 0.5, x + bw, top + bw * 0.5), fill=base_color + (255,))
        if windows and bw > 8:
            for wy in range(int(top + 8), h - 6, 9):
                for wx in range(int(x + 4), int(x + bw - 4), 7):
                    if rnd.random() < 0.18:
                        c = rnd.choice(WINDOWS)
                        d.rectangle((wx, wy, wx + 2, wy + 3), fill=c + (rnd.randint(150, 255),))
        x += bw + rnd.randint(0, int(w * 0.006))
    return img


def palm(h, color=(20, 6, 40), lean=0.18, seed=3):
    """A palm silhouette: a curved trunk and drooping fronds."""
    rnd = random.Random(seed)
    w = int(h * 0.9)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    base = (w * 0.45, h)
    top = (w * (0.45 + lean), h * 0.18)
    pts = []
    for i in range(41):                                   # trunk as a quadratic curve
        t = i / 40
        cx = (1 - t) ** 2 * base[0] + 2 * (1 - t) * t * (w * 0.42) + t * t * top[0]
        cy = (1 - t) ** 2 * base[1] + 2 * (1 - t) * t * (h * 0.55) + t * t * top[1]
        pts.append((cx, cy, h * (0.022 - 0.012 * t)))
    for (x0, y0, r0), (x1, y1, r1) in zip(pts, pts[1:]):
        d.polygon([(x0 - r0, y0), (x0 + r0, y0), (x1 + r1, y1), (x1 - r1, y1)], fill=color + (255,))
    tx, ty = top
    for k in range(9):                                    # fronds
        a = math.radians(-170 + k * 40 + rnd.uniform(-8, 8))
        length = h * rnd.uniform(0.28, 0.36)
        droop = h * 0.12
        prev = (tx, ty)
        for i in range(1, 21):
            t = i / 20
            px = tx + math.cos(a) * length * t
            py = ty + math.sin(a) * length * t * 0.55 + droop * t * t
            width = h * 0.028 * (1 - t) + 1
            d.line((prev, (px, py)), fill=color + (255,), width=int(width * 2))
            for side in (-1, 1):                          # leaflets
                lx = px + math.cos(a + side * 1.2) * h * 0.04 * (1 - t * 0.5)
                ly = py + math.sin(a + side * 1.2) * h * 0.04 * (1 - t * 0.5) + h * 0.02
                d.line(((px, py), (lx, ly)), fill=color + (255,), width=max(3, int(h * 0.012)))
            prev = (px, py)
    d.ellipse((tx - h * 0.025, ty - h * 0.02, tx + h * 0.025, ty + h * 0.03), fill=color + (255,))
    return img


def masks():
    """Logo alpha split into parts: "Push" (white), "FPS" + swoosh (green, left of the
    chevrons or above them) and the two chevrons."""
    arr = np.array(LOGO).astype(np.int32)
    a = arr[:, :, 3]
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    yy, xx = np.mgrid[0:LH, 0:LW]
    white = (r > 185) & (g > 185) & (b > 185) & (abs(r - g) < 35)
    # Grow "Push" over its anti-aliased edge, inside the letters' own box only, and never onto green.
    white = np.array(Image.fromarray((white * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0
    white &= (xx < 600) & (yy > 130) & (yy < 345) & ~(g - r > 25)
    chev_zone = (xx >= 1036) & (yy >= 200) & ~((xx < 1062) & (yy < 300))
    chev2 = chev_zone & (xx >= 1112) & ~white
    chev1 = chev_zone & (xx < 1112) & ~white
    push = white & ~chev_zone
    fps = ~white & ~chev_zone
    out = {}
    for name, m in [("push", push), ("fps", fps), ("chev1", chev1), ("chev2", chev2)]:
        out[name] = Image.fromarray(np.where(m, a, 0).astype(np.uint8), "L")
    out["all"] = LOGO.split()[3]
    return out


def letter_fill(w, h):
    """The picture inside the letters, built to stay readable: a bright sunset gradient over the
    letters' height, a soft sun, and the city and palms only as low silhouettes near the bottom
    (the letters span about 39-80 % of the logo's height)."""
    top, horizon, bottom = int(h * 0.36), int(h * 0.755), int(h * 0.82)
    img = gradient(w, h, [(0.00, (200, 128, 255)), (0.32, (255, 96, 196)), (0.62, (255, 118, 96)),
                          (0.86, (255, 186, 98)), (1.00, (255, 226, 150))], top, horizon)
    sd = sun(int(h * 0.36), glow=False)
    sd.putalpha(sd.split()[3].point(lambda v: int(v * 0.55)))
    img.alpha_composite(sd, (int(w * 0.47 - sd.width / 2), int(horizon - sd.height * 0.62)))
    city = skyline(w, int(h * 0.1), 23, (142, 46, 146), windows=False, max_frac=0.9, density=1.1)
    img.alpha_composite(city, (0, horizon - city.height))
    water = gradient(w, h - horizon, [(0, (170, 54, 140)), (1, (214, 84, 168))], 0, h - horizon)
    img.alpha_composite(water, (0, horizon))
    d = ImageDraw.Draw(img)
    for i in range(5):                                    # the sun's reflection
        y = horizon + 3 + i * (bottom - horizon) / 5
        half = w * (0.07 - i * 0.008)
        d.rectangle((w * 0.47 - half, y, w * 0.47 + half, y + 2 + i), fill=(255, 196, 120, 200 - i * 25))
    for x0, hh, lean, seed in [(0.05, 0.30, 0.16, 3), (0.30, 0.24, -0.12, 5), (0.66, 0.27, 0.14, 7), (0.80, 0.22, -0.1, 9)]:
        pl = palm(int(h * hh), color=(58, 14, 82), lean=lean, seed=seed)
        img.alpha_composite(pl, (int(w * x0 - pl.width * 0.45), horizon + int(h * 0.03) - pl.height))
    return img


def filled(mask, scene, stroke=True):
    """Letters cut from the scene, with a light inner rim and a dark outline around them so the
    shapes read on any background."""
    m = mask.resize((LW * S, LH * S), Image.LANCZOS)
    art = scene.copy()
    art.putalpha(ImageChops.multiply(art.split()[3], m))
    if not stroke:
        return art
    out = Image.new("RGBA", art.size, (0, 0, 0, 0))
    dark = Image.new("RGBA", art.size, OUTLINE + (0,))
    dark.putalpha(m.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(1.2)))
    out.alpha_composite(dark)
    out.alpha_composite(art)
    rim = ImageChops.subtract(m, m.filter(ImageFilter.MinFilter(5)))
    line = Image.new("RGBA", art.size, STROKE + (0,))
    line.putalpha(rim.point(lambda v: int(v * 0.9)))
    out.alpha_composite(line)
    return out


def glow(mask, pad):
    """A soft neon haze around the logo, kept dim so it never competes with the letters."""
    m = mask.resize((LW * S, LH * S), Image.LANCZOS)
    big = Image.new("L", (m.width + 2 * pad, m.height + 2 * pad), 0)
    big.paste(m, (pad, pad))
    inner = big.filter(ImageFilter.GaussianBlur(pad * 0.16))
    outer = big.filter(ImageFilter.GaussianBlur(pad * 0.42))
    img = Image.new("RGBA", big.size, GLOW_OUT + (0,))
    img.putalpha(outer.point(lambda v: min(255, int(v * 0.9))))
    tin = Image.new("RGBA", big.size, GLOW_IN + (0,))
    tin.putalpha(inner.point(lambda v: min(255, int(v * 0.8))))
    img.alpha_composite(tin)
    return img


def road(w, h, lines=24):
    """Perspective road grid towards a vanishing point at the top centre (neon lines)."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    vx = w / 2
    for i in range(-lines, lines + 1):
        x_bottom = vx + i * w / lines * 1.6
        d.line(((vx + i * 6, 0), (x_bottom, h)), fill=(255, 80, 190, 170), width=3)
    fade = gradient(1, h, [(0, (0, 0, 0)), (1, (255, 255, 255))]).convert("L").resize((w, h))
    img.putalpha(ImageChops.multiply(img.split()[3], fade))
    return img.filter(ImageFilter.GaussianBlur(0.6))


def main():
    m = masks()
    scene = letter_fill(LW * S, LH * S)
    full = filled(m["all"], scene)
    pad = 140
    g = glow(m["all"], pad)

    # The home page logo: glow baked under the art (software scene graph: no shader effects).
    home = g.copy()
    home.alpha_composite(full, (pad, pad))
    home.resize((home.width // 2, home.height // 2), Image.LANCZOS).save(QML / "sunset-logo.png", optimize=True)
    # Intro layers: the parts on the same canvas, and the glow on its padded canvas.
    for name in ("push", "fps", "chev1", "chev2"):
        part = filled(m[name], scene)
        part.resize((LW, LH), Image.LANCZOS).save(QML / f"sunset-logo-{name}.png", optimize=True)
    g.resize((g.width // 2, g.height // 2), Image.LANCZOS).save(QML / "sunset-logo-glow.png", optimize=True)

    sun(900).save(QML / "sunset-sun.png", optimize=True)
    back = skyline(2400, 420, 31, BUILDING_BACK, windows=False, density=1.1)
    front = skyline(2400, 300, 47, BUILDING_FRONT, windows=True, max_frac=0.9)
    city = Image.new("RGBA", (2400, 420), (0, 0, 0, 0))
    city.alpha_composite(back)
    city.alpha_composite(front, (0, 420 - 300))
    city.save(QML / "sunset-skyline.png", optimize=True)
    palm(900, seed=13, lean=0.2).save(QML / "sunset-palm.png", optimize=True)
    road(1800, 520).save(QML / "sunset-road.png", optimize=True)
    # A dark haze the intro puts behind the logo over the bright scene (no shader effects in the
    # software scene graph, so the radial falloff is baked).
    yy, xx = np.mgrid[0:400, 0:800]
    r = np.sqrt(((xx - 400) / 400) ** 2 + ((yy - 200) / 200) ** 2)
    alpha = (np.clip(1 - r, 0, 1) ** 1.3 * 255).astype(np.uint8)
    shade = Image.new("RGBA", (800, 400), (16, 5, 34, 0))
    shade.putalpha(Image.fromarray(alpha, "L"))
    shade.save(QML / "sunset-shade.png", optimize=True)

    if WEB.is_dir():
        home.resize((1100, int(home.height * 1100 / home.width)), Image.LANCZOS).save(WEB / "sunset-logo.webp", quality=90, method=6)
        home.resize((560, int(home.height * 560 / home.width)), Image.LANCZOS).save(WEB / "sunset-logo-560.webp", quality=90, method=6)
        city.resize((1600, 280), Image.LANCZOS).save(WEB / "sunset-skyline.webp", quality=88, method=6)
        palm(600, seed=13, lean=0.2).save(WEB / "sunset-palm.webp", quality=90, method=6)
        sun(600).save(WEB / "sunset-sun.webp", quality=90, method=6)
        # The plain logo for the site's light theme: "Push" in graphite instead of white.
        light = LOGO.copy()
        ink = Image.new("RGBA", LOGO.size, (22, 25, 30, 255))
        light.paste(ink, (0, 0), m["push"].point(lambda v: 255 if v > 0 else 0))
        light.putalpha(LOGO.split()[3])
        light.resize((1200, int(LH * 1200 / LW)), Image.LANCZOS).save(WEB / "pushfps-logo-light.webp", quality=90, method=6)
        light.resize((600, int(LH * 600 / LW)), Image.LANCZOS).save(WEB / "pushfps-logo-light-600.webp", quality=90, method=6)
    print("sunset art written")


if __name__ == "__main__":
    main()
