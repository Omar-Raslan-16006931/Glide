// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide automatic HUD detection (engine/PushHud.h) on synthetic pictures:
// a moving textured scene with static HUD elements is found, a still picture
// reports tooStill, flat static bars are not HUD, the result keeps at most four
// boxes (the largest), and reduce() reads every supported pixel layout.
#include "veyra/engine/PushHud.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace veyra::engine::hud;

namespace {
int failures = 0;
void check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok) ++failures;
}

// Deterministic texture value (a "game scene"), 0..255.
uint8_t scene(int x, int y) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    const int smooth = int(128 + 60 * std::sin(x * 0.05) + 40 * std::cos(y * 0.07));
    return uint8_t(std::clamp(smooth + int(h >> 28) * 4 - 30, 0, 255));
}

// A HUD-like pattern: bright strokes on dark, like text.
uint8_t hudPattern(int x, int y) { return ((x / 3 + y / 4) % 3 == 0 || (x % 7) == 0) ? 235 : 20; }

struct Rect { int x0, y0, x1, y1; }; // grid points, exclusive end

std::vector<LumaFrame> makeFrames(size_t count, int shiftPerFrame, const std::vector<Rect>& hud, const std::vector<Rect>& flat = {}, int noise = 0) {
    uint32_t rng = 12345;
    std::vector<LumaFrame> frames(count);
    for (size_t f = 0; f < count; ++f) {
        auto& luma = frames[f].luma;
        luma.resize(size_t(kGridWidth) * kGridHeight);
        for (int y = 0; y < int(kGridHeight); ++y)
            for (int x = 0; x < int(kGridWidth); ++x) {
                uint8_t v = scene(x + int(f) * shiftPerFrame, y + int(f) * shiftPerFrame / 2);
                for (const auto& r : hud) if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1) v = hudPattern(x, y);
                for (const auto& r : flat) if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1) v = 16;
                if (noise) {
                    rng = rng * 1664525u + 1013904223u;
                    v = uint8_t(std::clamp(int(v) + int((rng >> 24) % uint32_t(2 * noise + 1)) - noise, 0, 255));
                }
                luma[size_t(y) * kGridWidth + size_t(x)] = v;
            }
    }
    return frames;
}

Region toRegion(const Rect& r) {
    return {float(r.x0) / kGridWidth, float(r.y0) / kGridHeight, float(r.x1) / kGridWidth, float(r.y1) / kGridHeight};
}
float iou(const Region& a, const Region& b) {
    const float ix = std::max(0.0f, std::min(a.right, b.right) - std::max(a.left, b.left));
    const float iy = std::max(0.0f, std::min(a.bottom, b.bottom) - std::max(a.top, b.top));
    const float inter = ix * iy;
    const float uni = (a.right - a.left) * (a.bottom - a.top) + (b.right - b.left) * (b.bottom - b.top) - inter;
    return uni > 0 ? inter / uni : 0;
}
float bestIou(const Detection& d, const Rect& truth) {
    float best = 0;
    for (const auto& r : d.regions) best = std::max(best, iou(r, toRegion(truth)));
    return best;
}
bool covers(const Detection& d, const Rect& truth) {
    const Region t = toRegion(truth);
    for (const auto& r : d.regions)
        if (r.left <= t.left + 1e-4f && r.top <= t.top + 1e-4f && r.right >= t.right - 1e-4f && r.bottom >= t.bottom - 1e-4f) return true;
    return false;
}
} // namespace

int main() {
    {
        const Rect minimap{300, 150, 370, 206};  // bottom right
        const Rect health{12, 10, 90, 24};       // top left
        const auto frames = makeFrames(10, 5, {minimap, health});
        const auto d = detect(frames);
        std::printf("INFO motion=%.2f candidates=%u regions=%zu\n", d.motion, d.candidateCells, d.regions.size());
        check(!d.tooStill, "moving scene is not too still");
        check(d.regions.size() == 2, "two HUD elements give two regions");
        check(covers(d, minimap) && bestIou(d, minimap) > 0.6f, "minimap box covers it closely");
        check(covers(d, health) && bestIou(d, health) > 0.5f, "health bar box covers it closely");
        bool inside = true;
        for (const auto& r : d.regions)
            inside = inside && r.left >= 0 && r.top >= 0 && r.right <= 1 && r.bottom <= 1 && r.left < r.right && r.top < r.bottom;
        check(inside, "regions are inside the picture and ordered");
        check(d.regions.size() == 2 && (d.regions[0].right - d.regions[0].left) * (d.regions[0].bottom - d.regions[0].top) >=
                                           (d.regions[1].right - d.regions[1].left) * (d.regions[1].bottom - d.regions[1].top),
              "largest region first");
    }
    {
        const auto frames = makeFrames(10, 0, {{12, 10, 90, 24}});
        const auto d = detect(frames);
        check(d.tooStill && d.regions.empty(), "a still picture reports tooStill and no regions");
    }
    {
        const auto frames = makeFrames(10, 5, {});
        const auto d = detect(frames);
        check(!d.tooStill && d.regions.empty(), "a moving scene without HUD gives no regions");
    }
    {
        // Letterbox bars: static but flat.
        const auto frames = makeFrames(10, 5, {}, {{0, 0, int(kGridWidth), 24}, {0, int(kGridHeight) - 24, int(kGridWidth), int(kGridHeight)}});
        const auto d = detect(frames);
        check(d.regions.empty(), "flat static letterbox bars are not HUD");
    }
    {
        // Six separate elements: the four largest are kept.
        std::vector<Rect> hud = {{10, 10, 80, 40}, {300, 10, 374, 40}, {10, 170, 70, 206}, {310, 160, 374, 206},
                                 {170, 10, 186, 18}, {175, 190, 190, 198}};
        const auto d = detect(makeFrames(10, 5, hud));
        check(d.regions.size() == 4, "at most four regions");
        bool big = true;
        for (size_t i = 0; i < 4; ++i) big = big && covers(d, hud[i]);
        check(big, "the four largest elements are the ones kept");
    }
    {
        // Capture noise of +-2 levels on every point.
        const Rect minimap{300, 150, 370, 206};
        const auto d = detect(makeFrames(12, 5, {minimap}, {}, 2));
        check(d.regions.size() == 1 && covers(d, minimap), "the HUD is still found with capture noise");
    }
    {
        // Two parts of one element with a small gap merge into one box.
        const auto d = detect(makeFrames(10, 5, {{20, 20, 60, 34}, {64, 20, 100, 34}}));
        check(d.regions.size() == 1, "close parts merge into one region");
    }
    {
        check(detect(makeFrames(4, 5, {{20, 20, 60, 34}})).regions.empty(), "fewer than six frames give no regions");
        auto frames = makeFrames(8, 5, {{20, 20, 60, 34}});
        frames[3].luma.resize(10);
        check(detect(frames).regions.empty(), "a wrongly sized frame gives no regions");
    }
    {
        // reduce(): left half white, right half black, every layout.
        const uint32_t w = 1280, h = 720;
        auto fill = [&](PixelLayout layout) {
            const size_t bpp = layout == PixelLayout::Rgba16f ? 8 : 4;
            std::vector<uint8_t> px(size_t(w) * h * bpp, 0);
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w / 2; ++x) {
                    uint8_t* p = px.data() + (size_t(y) * w + x) * bpp;
                    if (layout == PixelLayout::Rgba16f) { const uint16_t one = 0x3C00; for (int c = 0; c < 3; ++c) std::memcpy(p + c * 2, &one, 2); }
                    else if (layout == PixelLayout::Rgb10a2) { const uint32_t v = 0x3FFu | (0x3FFu << 10) | (0x3FFu << 20); std::memcpy(p, &v, 4); }
                    else { p[0] = p[1] = p[2] = 255; }
                }
            return px;
        };
        const PixelLayout layouts[] = {PixelLayout::Rgba8, PixelLayout::Bgra8, PixelLayout::Rgb10a2, PixelLayout::Rgba16f};
        const char* names[] = {"RGBA8", "BGRA8", "RGB10A2", "RGBA16F"};
        for (int i = 0; i < 4; ++i) {
            const auto px = fill(layouts[i]);
            LumaFrame out;
            std::vector<uint8_t> preview;
            const size_t pitch = size_t(w) * (layouts[i] == PixelLayout::Rgba16f ? 8 : 4);
            const bool ok = reduce(px.data(), pitch, w, h, layouts[i], out, &preview);
            check(ok && out.luma.size() == size_t(kGridWidth) * kGridHeight, std::string("reduce ") + names[i] + " fills the grid");
            check(ok && out.luma[100 * kGridWidth + 10] >= 254 && out.luma[100 * kGridWidth + kGridWidth - 10] == 0,
                  std::string("reduce ") + names[i] + " keeps white left and black right");
            check(ok && preview.size() == size_t(kPreviewWidth) * kPreviewHeight * 3 && preview[(180 * kPreviewWidth + 20) * 3] >= 254 &&
                      preview[(180 * kPreviewWidth + kPreviewWidth - 20) * 3] == 0,
                  std::string("reduce ") + names[i] + " preview");
        }
        // Channel order: pure red in BGRA8 memory order is byte 2.
        std::vector<uint8_t> red(size_t(w) * h * 4, 0);
        for (size_t i = 0; i < red.size(); i += 4) red[i + 2] = 255;
        LumaFrame a, b;
        reduce(red.data(), size_t(w) * 4, w, h, PixelLayout::Bgra8, a);
        reduce(red.data(), size_t(w) * 4, w, h, PixelLayout::Rgba8, b);
        check(std::abs(int(a.luma[0]) - 54) <= 1 && std::abs(int(b.luma[0]) - 18) <= 1, "BGRA and RGBA channel order");
        LumaFrame c;
        check(!reduce(red.data(), 16, w, h, PixelLayout::Rgba8, c), "a too small pitch is refused");
        check(!reduce(red.data(), 4 * 32, 32, 32, PixelLayout::Rgba8, c), "a tiny frame is refused");
    }
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
