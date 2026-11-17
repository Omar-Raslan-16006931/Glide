#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
"""The animated Glide logo at the top of the README (classic Glide brand: logo green, graphite).

Story, about five seconds, looping:
  1. The logo slides in, leaving a trail of logo-green copies: the generated frames between real ones.
  2. A counter under it climbs 30 -> 60 -> 90 -> 120 FPS while the logo settles.
  3. A light sweep crosses the logo, then the chevrons push forward twice.
  4. A short hold, a quieter chevron push, a hold, and the logo slides out with its trail.

Two transparent APNG files (soft edges on any background; GIF transparency is all-or-nothing):
  assets/pushfps-logo-animated-dark.png   (GitHub dark theme: white "Push", from pushfps-logo.png)
  assets/pushfps-logo-animated-light.png  (GitHub light theme: dark "Push", from pushfps-logo-light.png)
The README picks one with <picture> and prefers-color-scheme. Fonts are the website's own Barlow files.
Run: python3 -I scripts/brand/make_readme_animation.py [--preview DIR]
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage

ROOT = Path(__file__).resolve().parents[2]
W, H = 720, 320           # shown at width 480 in the README (1.5x for sharp high-DPI screens)
LOGO_W = 600
FPS = 30
GREEN = (141, 242, 55)    # the logo green (#8df237)

def ease_out(x): x = min(max(x, 0.0), 1.0); return 1 - (1 - x) ** 3
def ease_in(x): x = min(max(x, 0.0), 1.0); return x ** 3

def load_logo(name):
    im = Image.open(ROOT / "assets" / name).convert("RGBA")
    return im.resize((LOGO_W, round(im.height * LOGO_W / im.width)), Image.LANCZOS)

def chevron_mask(logo):
    """Soft mask of the two chevrons (the green shapes right of the S)."""
    a = np.asarray(logo).astype(int)
    green = (a[..., 3] > 40) & (a[..., 1] > a[..., 0] + 25) & (a[..., 1] > a[..., 2] + 25)
    labels, n = ndimage.label(green)
    h, w = green.shape
    keep = np.zeros_like(green)
    for i in range(1, n + 1):
        ys, xs = np.nonzero(labels == i)
        if xs.size > 30 and xs.min() > w * 0.86 and ys.min() > h * 0.45:
            keep |= labels == i
    keep = ndimage.binary_dilation(keep, iterations=3)
    return Image.fromarray((keep * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.8))

def with_alpha(im, factor):
    if factor >= 0.999: return im
    out = im.copy(); out.putalpha(im.getchannel("A").point(lambda v: int(v * factor)))
    return out

def silhouette(logo, colour, factor):
    s = Image.new("RGBA", logo.size, colour + (0,))
    s.putalpha(logo.getchannel("A").point(lambda v: int(v * factor)))
    return s

class Scene:
    def __init__(self, logo, number_colour, unit_colour):
        self.logo = logo
        self.cmask = chevron_mask(logo)
        inv = self.cmask.point(lambda v: 255 - v)
        self.body = logo.copy(); self.body.putalpha(Image.fromarray(
            (np.asarray(logo.getchannel("A")).astype(np.uint16) * np.asarray(inv) // 255).astype(np.uint8)))
        self.chev = logo.copy(); self.chev.putalpha(Image.fromarray(
            (np.asarray(logo.getchannel("A")).astype(np.uint16) * np.asarray(self.cmask) // 255).astype(np.uint8)))
        self.chev_glow = silhouette(self.chev, GREEN, 1.0).filter(ImageFilter.GaussianBlur(7))
        yy, xx = np.mgrid[0:logo.height, 0:logo.width]
        self.slant = xx + (logo.height - yy) * 0.35       # the light sweep leans forward
        self.alpha = np.asarray(logo.getchannel("A")).astype(np.float32) / 255
        self.number_font = ImageFont.truetype(str(ROOT / "website/fonts/BarlowSemiCondensed-ExtraBoldItalic.woff2"), 50)
        self.unit_font = ImageFont.truetype(str(ROOT / "website/fonts/BarlowSemiCondensed-SemiBold.woff2"), 28)
        self.number_colour, self.unit_colour = number_colour, unit_colour
        self.x0, self.y0 = (W - LOGO_W) // 2, 22

    @staticmethod
    def offset(t):
        if t < 0.85: return -300 * (1 - ease_out(t / 0.85))
        if t > 4.55: return 320 * ease_in((t - 4.55) / 0.45)
        return 0.0

    @staticmethod
    def visibility(t):
        if t < 0.22: return t / 0.22
        if t > 4.55: return max(0.0, 1 - (t - 4.55) / 0.45)
        return 1.0

    @staticmethod
    def chevron_push(t):
        """(shift in px, glow 0..1): two pushes after the sweep, a quieter one in the hold."""
        for start, size in ((1.45, 1.0), (1.85, 1.0), (3.2, 0.6)):
            u = (t - start) / 0.38
            if 0 <= u <= 1:
                k = math.sin(math.pi * u) * size
                return 14 * k, k
        return 0.0, 0.0

    def frame(self, t):
        c = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        vis = self.visibility(t)
        dx = self.offset(t)
        # The trail: logo-green copies where the logo just was (the generated frames).
        for k, strength in enumerate((0.34, 0.24, 0.16, 0.10, 0.05), start=1):
            gx = self.offset(max(0.0, t - k * 0.045))
            if abs(gx - dx) < 3 or (t > 4.55 and t - k * 0.045 < 4.55): continue
            c.alpha_composite(silhouette(self.logo, GREEN, strength * vis), (int(self.x0 + gx), self.y0))
        # The logo, with the light sweep and the chevron push.
        logo = self.body.copy()
        if 0.9 <= t <= 1.6:
            centre = -120 + (LOGO_W + 300) * ((t - 0.9) / 0.7)
            band = np.exp(-((self.slant - centre) ** 2) / (2 * 28.0 ** 2)) * 0.7 * self.alpha
            light = Image.new("RGBA", logo.size, (255, 255, 255, 0))
            light.putalpha(Image.fromarray((band * 255).astype(np.uint8)))
            logo.alpha_composite(light)
        shift, glow = self.chevron_push(t)
        if glow > 0: logo.alpha_composite(with_alpha(self.chev_glow, 0.55 * glow), (int(round(shift)), 0))
        logo.alpha_composite(self.chev, (int(round(shift)), 0))
        c.alpha_composite(with_alpha(logo, vis), (int(self.x0 + dx), self.y0))
        # The counter: 30 -> 60 -> 90 -> 120 FPS while the logo arrives.
        value = 30 if t < 0.3 else 60 if t < 0.5 else 90 if t < 0.7 else 120
        if vis > 0:
            layer = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
            number, unit = str(value), " FPS"
            nw = d.textlength(number, font=self.number_font); uw = d.textlength(unit, font=self.unit_font)
            x = (W - nw - uw) / 2; y = self.y0 + self.logo.height + 14
            d.text((x, y), number, font=self.number_font, fill=self.number_colour)
            d.text((x + nw, y + 17), unit, font=self.unit_font, fill=self.unit_colour)
            c.alpha_composite(with_alpha(layer, vis))
        return c

def timeline():
    """(time, duration in ms): 30 frames per second while something moves, single frames for holds."""
    out = []
    def run(a, b):
        n = round((b - a) * FPS)
        out.extend((a + i / FPS, 1000 / FPS) for i in range(n))
    run(0.0, 2.25); out.append((2.25, 950))   # arrive, count, sweep, push; hold
    run(3.2, 3.6); out.append((3.6, 950))     # a quieter push; hold
    run(4.55, 5.0); out.append((5.0, 250))    # slide out; a moment empty
    return out

def build(logo_name, number_colour, unit_colour, path, preview=None, backdrop=None):
    scene = Scene(load_logo(logo_name), number_colour, unit_colour)
    frames, durations = [], []
    for t, ms in timeline():
        frames.append(scene.frame(t)); durations.append(int(round(ms)))
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=durations, loop=0, format="PNG", optimize=True)
    print(f"{path.relative_to(ROOT)}: {len(frames)} frames, {sum(durations) / 1000:.2f} s, {path.stat().st_size / 1e6:.2f} MB")
    if preview:
        # A GIF on a solid backdrop and a contact sheet, for looking at it (not for the repo).
        shown = [Image.alpha_composite(Image.new("RGBA", (W, H), backdrop + (255,)), f).convert("RGB") for f in frames]
        shown[0].save(preview / (path.stem + "-preview.gif"), save_all=True, append_images=shown[1:], duration=durations, loop=0)
        picks = [shown[i] for i in (3, 10, 18, 32, 46, 56, len(shown) - 6, len(shown) - 3)]
        sheet = Image.new("RGB", (W * 2, H * 4))
        for i, im in enumerate(picks): sheet.paste(im, ((i % 2) * W, (i // 2) * H))
        sheet.save(preview / (path.stem + "-sheet.png"))

if __name__ == "__main__":
    preview = Path(sys.argv[sys.argv.index("--preview") + 1]) if "--preview" in sys.argv else None
    if preview: preview.mkdir(parents=True, exist_ok=True)
    build("pushfps-logo.png", GREEN, (205, 210, 200), ROOT / "assets/pushfps-logo-animated-dark.png", preview, (13, 17, 23))
    build("pushfps-logo-light.png", (76, 150, 18), (70, 76, 66), ROOT / "assets/pushfps-logo-animated-light.png", preview, (255, 255, 255))
