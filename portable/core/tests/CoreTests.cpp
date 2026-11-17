// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Tests for the Linux / macOS core: conversion, motion, interpolation, duplicate removal, HUD lock and
// the engine's timing, plus abuse (odd sizes, clocks going back, settings changed mid-stream).
#include "veyra/portable/Convert.h"
#include "veyra/portable/Dedup.h"
#include "veyra/portable/Engine.h"
#include "veyra/portable/HudLock.h"
#include "veyra/portable/Interpolate.h"
#include "veyra/portable/Motion.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace veyra::portable;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ---- helpers ---------------------------------------------------------------------------------

// A textured scene: smooth noise, so motion search has something to lock onto (like a game picture).
struct Scene {
    int w, h;
    std::vector<float> base;  // larger than the picture so shifted views stay inside
    int margin;
    Scene(int w_, int h_, int margin_, unsigned seed) : w(w_), h(h_), margin(margin_) {
        const int W = w + 2 * margin, H = h + 2 * margin;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> u(0.f, 255.f);
        std::vector<float> n(size_t(W) * H);
        for (auto& v : n) v = u(rng);
        // Two box blurs: features of a few pixels.
        base = n;
        for (int pass = 0; pass < 2; ++pass) {
            std::vector<float> t(base.size());
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    float s = 0; int c = 0;
                    for (int d = -2; d <= 2; ++d) { const int xx = std::clamp(x + d, 0, W - 1); s += base[size_t(y) * W + xx]; ++c; }
                    t[size_t(y) * W + x] = s / c;
                }
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    float s = 0; int c = 0;
                    for (int d = -2; d <= 2; ++d) { const int yy = std::clamp(y + d, 0, H - 1); s += t[size_t(yy) * W + x]; ++c; }
                    base[size_t(y) * W + x] = s / c;
                }
        }
        // Stretch contrast back up.
        float lo = 1e9f, hi = -1e9f;
        for (float v : base) { lo = std::min(lo, v); hi = std::max(hi, v); }
        for (float& v : base) v = (v - lo) / (hi - lo) * 255.f;
    }
    float at(float x, float y) const {  // bilinear
        const int W = w + 2 * margin, H = h + 2 * margin;
        x += margin; y += margin;
        x = std::clamp(x, 0.f, float(W - 1)); y = std::clamp(y, 0.f, float(H - 1));
        const int x0 = int(x), y0 = int(y), x1 = std::min(x0 + 1, W - 1), y1 = std::min(y0 + 1, H - 1);
        const float fx = x - x0, fy = y - y0;
        const float a = base[size_t(y0) * W + x0], b = base[size_t(y0) * W + x1], c = base[size_t(y1) * W + x0], d = base[size_t(y1) * W + x1];
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }
};

// Background shifted by (bx, by), a textured square of size s at (ox, oy) on top.
Bgra render(const Scene& bg, const Scene& fg, float bx, float by, float ox, float oy, int s, bool hud = false) {
    Bgra img(bg.w, bg.h);
    for (int y = 0; y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            float v = bg.at(float(x) - bx, float(y) - by);
            float g = v * 0.8f + 20.f, b = 255.f - v;
            if (x >= ox && x < ox + s && y >= oy && y < oy + s) {
                const float f = fg.at(float(x) - ox, float(y) - oy);
                v = f; g = 255.f - f * 0.5f; b = f * 0.3f;
            }
            if (hud && y >= 8 && y < 24 && x >= 8 && x < 104 && ((x / 8 + y / 8) % 2 == 0)) { v = 250; g = 250; b = 250; }
            uint8_t* p = img.pixel(x, y);
            p[0] = uint8_t(std::clamp(b, 0.f, 255.f)); p[1] = uint8_t(std::clamp(g, 0.f, 255.f)); p[2] = uint8_t(std::clamp(v, 0.f, 255.f)); p[3] = 255;
        }
    return img;
}

double psnr(const Bgra& a, const Bgra& b, int border = 0) {
    double se = 0; size_t n = 0;
    for (int y = border; y < a.height - border; ++y)
        for (int x = border; x < a.width - border; ++x)
            for (int c = 0; c < 3; ++c) { const double d = double(a.pixel(x, y)[c]) - double(b.pixel(x, y)[c]); se += d * d; ++n; }
    if (n == 0) return 0;
    const double mse = se / double(n);
    return mse <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

Bgra blend(const Bgra& a, const Bgra& b, float t) {
    Bgra o(a.width, a.height);
    for (size_t i = 0; i < a.px.size(); ++i) o.px[i] = uint8_t(std::lround(a.px[i] + (b.px[i] - a.px[i]) * t));
    return o;
}

float medianOf(std::vector<float> v) { std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end()); return v[v.size() / 2]; }

// ---- tests -----------------------------------------------------------------------------------

void testConvert() {
    // NV12 limited range: Y=235, U=V=128 is white; Y=16 black.
    for (auto m : {Matrix::Bt709, Matrix::Bt601}) {
        uint8_t y[4] = {235, 16, 235, 16}, uv[2] = {128, 128};
        SourceFrame f; f.format = PixelFormat::Nv12; f.width = 2; f.height = 2; f.matrix = m;
        f.planes[0] = {y, 2}; f.planes[1] = {uv, 2};
        Bgra out;
        CHECK(convertToBgra(f, out), "nv12");
        CHECK(out.pixel(0, 0)[0] == 255 && out.pixel(0, 0)[1] == 255 && out.pixel(0, 0)[2] == 255, "white %d %d %d", out.pixel(0, 0)[0], out.pixel(0, 0)[1], out.pixel(0, 0)[2]);
        CHECK(out.pixel(1, 0)[0] == 0 && out.pixel(1, 0)[2] == 0, "black");
    }
    // BT.709 limited red: Y=63 U=102 V=240.
    {
        uint8_t yuyv[4] = {63, 102, 63, 240};
        SourceFrame f; f.format = PixelFormat::Yuyv; f.width = 2; f.height = 1; f.planes[0] = {yuyv, 4};
        Bgra out;
        CHECK(convertToBgra(f, out), "yuyv");
        const uint8_t* p = out.pixel(0, 0);
        CHECK(p[2] > 245 && p[1] < 12 && p[0] < 12, "red %d %d %d", p[2], p[1], p[0]);
        uint8_t uyvy[4] = {102, 63, 240, 63};
        f.format = PixelFormat::Uyvy; f.planes[0] = {uyvy, 4};
        Bgra out2;
        CHECK(convertToBgra(f, out2) && std::memcmp(out.px.data(), out2.px.data(), out.px.size()) == 0, "uyvy == yuyv");
    }
    // I420 and P010 agree with NV12 on a gradient, odd size 5x3.
    {
        const int w = 5, h = 3, cw = 3, ch = 2;
        std::vector<uint8_t> Y(w * h), U(cw * ch), V(cw * ch), UV(cw * ch * 2);
        std::vector<uint8_t> Y16(w * h * 2), UV16(cw * ch * 4);
        for (int i = 0; i < w * h; ++i) { Y[i] = uint8_t(16 + i * 13); Y16[i * 2] = 0; Y16[i * 2 + 1] = Y[i]; }
        for (int i = 0; i < cw * ch; ++i) {
            U[i] = uint8_t(60 + i * 20); V[i] = uint8_t(200 - i * 17);
            UV[i * 2] = U[i]; UV[i * 2 + 1] = V[i];
            UV16[i * 4 + 1] = U[i]; UV16[i * 4 + 3] = V[i];
        }
        SourceFrame a; a.format = PixelFormat::Nv12; a.width = w; a.height = h; a.planes[0] = {Y.data(), w}; a.planes[1] = {UV.data(), cw * 2};
        SourceFrame b; b.format = PixelFormat::I420; b.width = w; b.height = h; b.planes[0] = {Y.data(), w}; b.planes[1] = {U.data(), cw}; b.planes[2] = {V.data(), cw};
        SourceFrame c; c.format = PixelFormat::P010; c.width = w; c.height = h; c.planes[0] = {Y16.data(), w * 2}; c.planes[1] = {UV16.data(), cw * 4};
        Bgra oa, ob, oc;
        CHECK(convertToBgra(a, oa) && convertToBgra(b, ob) && convertToBgra(c, oc), "odd convert");
        CHECK(oa.px == ob.px, "i420 == nv12");
        CHECK(oa.px == oc.px, "p010 == nv12");
    }
    // Luma straight from the source matches the luma of the converted picture (within rounding).
    {
        const int w = 6, h = 4, cw = 3;
        std::vector<uint8_t> Y(w * h), UV(cw * 2 * 2, 128), yuyv(cw * 4 * h);
        for (int i = 0; i < w * h; ++i) Y[i] = uint8_t(16 + i * 9);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { yuyv[y * cw * 4 + x * 2] = Y[y * w + x]; yuyv[y * cw * 4 + (x / 2) * 4 + 1 + (x & 1) * 2] = 128; }
        SourceFrame a; a.format = PixelFormat::Nv12; a.width = w; a.height = h; a.planes[0] = {Y.data(), w}; a.planes[1] = {UV.data(), cw * 2};
        SourceFrame b; b.format = PixelFormat::Yuyv; b.width = w; b.height = h; b.planes[0] = {yuyv.data(), cw * 4};
        Bgra rgb;
        convertToBgra(a, rgb);
        const Gray la = lumaFromSource(a), lb = lumaFromSource(b), lc = lumaOf(rgb);
        CHECK(la.px == lb.px, "nv12 luma == yuyv luma");
        int worst = 0;
        for (size_t i = 0; i < la.px.size(); ++i) worst = std::max(worst, std::abs(int(la.px[i]) - int(lc.px[i])));
        CHECK(worst <= 2, "source luma vs converted luma %d", worst);
        a.planes[0].stride = 2;
        CHECK(lumaFromSource(a).empty(), "luma short stride");
    }
    // Rejected: zero size, short stride, missing plane, absurd size.
    {
        uint8_t buf[64] = {};
        SourceFrame f; f.format = PixelFormat::Nv12; f.width = 4; f.height = 2; f.planes[0] = {buf, 3}; f.planes[1] = {buf, 4};
        Bgra out;
        CHECK(!convertToBgra(f, out), "short stride");
        f.planes[0] = {buf, 4}; f.planes[1] = {nullptr, 4};
        CHECK(!convertToBgra(f, out), "missing plane");
        f.width = 0;
        CHECK(!convertToBgra(f, out), "zero size");
        f.width = 100000; f.planes[1] = {buf, 4};
        CHECK(!convertToBgra(f, out), "huge");
        CHECK(out.empty(), "out untouched");
    }
}

void testPyramid() {
    Gray g(17, 9, 100);
    const Pyramid p = Pyramid::of(g, 4);
    CHECK(p.levels.size() == 2, "levels %zu", p.levels.size());  // 17x9 -> 9x5 (stops at <= 8 height)
    CHECK(p.levels[1].width == 9 && p.levels[1].height == 5, "halve odd");
    CHECK(Pyramid::of(Gray(), 4).levels.empty(), "empty");
    CHECK(Pyramid::of(Gray(1, 1, 7), 4).levels.size() == 1, "1x1");
}

void testMotion() {
    const Scene bg(320, 192, 80, 1), fg(64, 64, 8, 2);
    const MotionEstimator est;
    struct Case { float dx, dy; } cases[] = {{0, 0}, {6, -3}, {2.5f, 1.5f}, {-17, 9}, {41, -22}, {-60, 30}};
    for (const auto& c : cases) {
        const Bgra a = render(bg, fg, 0, 0, -1000, -1000, 0), b = render(bg, fg, c.dx, c.dy, -1000, -1000, 0);
        const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
        const MotionField f = est.estimate(pa, pb);
        CHECK(!f.empty() && f.blockSize == 16, "field");
        // Interior blocks (edges see the picture border).
        // Interior blocks: the picture edge (where new content enters) has no right answer.
        std::vector<float> xs, ys;
        const int mx0 = 2 + int(std::ceil(std::abs(c.dx) / 16.f)), my0 = 2 + int(std::ceil(std::abs(c.dy) / 16.f));
        for (int j = my0; j < f.blocksY - my0; ++j)
            for (int i = mx0; i < f.blocksX - mx0; ++i) { xs.push_back(f.vx[j * f.blocksX + i]); ys.push_back(f.vy[j * f.blocksX + i]); }
        const float mx = medianOf(xs), my = medianOf(ys);
        CHECK(std::abs(mx - c.dx) < 0.75f && std::abs(my - c.dy) < 0.75f, "shift (%.1f,%.1f) found (%.2f,%.2f)", c.dx, c.dy, mx, my);
        int good = 0;
        for (size_t k = 0; k < xs.size(); ++k) good += std::abs(xs[k] - c.dx) < 1.5f && std::abs(ys[k] - c.dy) < 1.5f;
        CHECK(good > int(xs.size() * 0.9), "shift (%.1f,%.1f): %d/%zu blocks right", c.dx, c.dy, good, xs.size());
        CHECK(!f.sceneCut, "no cut for a pan");
    }
    // Flat picture: vectors stay small (smoothness), no NaN.
    {
        Bgra a(128, 96), b(128, 96);
        for (size_t i = 0; i < a.px.size(); ++i) if (i % 4 != 3) a.px[i] = b.px[i] = 90;
        const MotionField f = est.estimate(Pyramid::of(lumaOf(a), 4), Pyramid::of(lumaOf(b), 4));
        float worst = 0;
        for (size_t k = 0; k < f.vx.size(); ++k) { CHECK(std::isfinite(f.vx[k]) && std::isfinite(f.vy[k]), "finite"); worst = std::max({worst, std::abs(f.vx[k]), std::abs(f.vy[k])}); }
        CHECK(worst < 1.0f, "flat picture vectors %.2f", worst);
    }
    // Scene cut: two unrelated pictures.
    {
        const Scene other(320, 192, 80, 99);
        const Bgra a = render(bg, fg, 0, 0, -1000, -1000, 0), b = render(other, fg, 0, 0, -1000, -1000, 0);
        const MotionField f = est.estimate(Pyramid::of(lumaOf(a), 4), Pyramid::of(lumaOf(b), 4));
        CHECK(f.sceneCut, "scene cut (mean %.1f)", f.meanCost);
    }
    // Size mismatch gives an empty field.
    CHECK(est.estimate(Pyramid::of(Gray(64, 64, 1), 4), Pyramid::of(Gray(64, 32, 1), 4)).empty(), "size mismatch");
}

void testInterpolate() {
    const Scene bg(320, 192, 80, 3), fg(64, 64, 8, 4);
    const MotionEstimator est;
    // Background pans 8 px, the square moves 24 px the other way: the hard case for a plain blend.
    const Bgra a = render(bg, fg, 0, 0, 100, 60, 64), b = render(bg, fg, 8, 2, 76, 60, 64);
    const Bgra truth = render(bg, fg, 4, 1, 88, 60, 64);
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    Bgra out;
    CHECK(interpolate(a, b, f, k, nullptr, 0.5f, out, 3), "interpolate");
    const double mc = psnr(out, truth, 8), lin = psnr(blend(a, b, 0.5f), truth, 8), hold = psnr(a, truth, 8);
    std::printf("  interpolation PSNR at t=0.5: motion %.1f dB, blend %.1f dB, repeat %.1f dB\n", mc, lin, hold);
    CHECK(mc > lin + 6.0 && mc > hold + 6.0, "motion-compensated beats blend/repeat");
    CHECK(mc > 26.0, "motion-compensated quality %.1f", mc);
    // Same result with 1 and many threads.
    Bgra out1;
    interpolate(a, b, f, k, nullptr, 0.5f, out1, 1);
    CHECK(out1.px == out.px, "threads give the same picture");
    // t = 0 and 1 are the real frames exactly.
    interpolate(a, b, f, k, nullptr, 0.f, out1, 2);
    CHECK(out1.px == a.px, "t=0 is A");
    interpolate(a, b, f, k, nullptr, 1.f, out1, 2);
    CHECK(out1.px == b.px, "t=1 is B");
    // NaN phase and missing fields fall back to a real frame.
    interpolate(a, b, f, k, nullptr, std::nanf(""), out1, 2);
    CHECK(out1.px == a.px, "NaN t");
    interpolate(a, b, MotionField{}, k, nullptr, 0.7f, out1, 2);
    CHECK(out1.px == b.px, "no field: nearer frame");
    // A full mask locks everything to the nearer real frame.
    Gray full(10, 10, 255);
    interpolate(a, b, f, k, &full, 0.25f, out1, 2);
    CHECK(psnr(out1, a) > 45.0, "full HUD mask = A");
    // Different sizes are refused.
    CHECK(!interpolate(a, Bgra(10, 10), f, k, nullptr, 0.5f, out1), "size mismatch");
}

void testDedup() {
    Gray a(64, 36, 50);
    for (int i = 0; i < 64 * 36; ++i) a.px[i] = uint8_t((i * 37) % 200);
    Gray b = a;
    CHECK(compareForDuplicate(a, b).duplicate, "identical");
    std::mt19937 rng(5);
    for (auto& v : b.px) v = uint8_t(std::clamp(int(v) + int(rng() % 3) - 1, 0, 255));
    CHECK(compareForDuplicate(a, b).duplicate, "capture noise +-1");
    Gray c = a;
    for (int y = 10; y < 13; ++y) for (int x = 20; x < 23; ++x) c.at(x, y) = 255;  // a 3x3 cursor at quarter size
    CHECK(!compareForDuplicate(a, c).duplicate, "small change is new");
    Gray d = a;
    for (auto& v : d.px) v = uint8_t(std::min(255, v + 2));  // slow fade
    CHECK(!compareForDuplicate(a, d).duplicate, "fade is new");
    CHECK(!compareForDuplicate(a, Gray(32, 18, 50)).duplicate, "other size");
    CHECK(!compareForDuplicate(Gray(), a).duplicate, "empty");
}

void testHudLock() {
    const Scene bg(320, 192, 80, 7), fg(32, 32, 4, 8);
    HudLock hud;
    Gray mask;
    for (int i = 0; i < 40; ++i) {
        const Bgra f = render(bg, fg, float(i * 3), 0, -1000, -1000, 0, true);
        mask = hud.update(halve(lumaOf(f)));
    }
    // HUD cells: 8x8 at full size (4x4 at half size) in a checkerboard from (8, 8); the inner 2x2 of
    // each white cell is fully HUD.
    int hudOn = 0, hudTotal = 0, bgOn = 0, bgTotal = 0;
    for (int y = 0; y < mask.height; ++y)
        for (int x = 0; x < mask.width; ++x) {
            const int fx = x * 2, fy = y * 2;
            const bool inHud = fx >= 8 && fx < 104 && fy >= 8 && fy < 24 && ((fx / 8 + fy / 8) % 2 == 0) && (x % 4 == 1 || x % 4 == 2) && (y % 4 == 1 || y % 4 == 2);
            const bool farFromHud = y > 20;
            if (inHud) { ++hudTotal; hudOn += mask.at(x, y) > 128; }
            if (farFromHud) { ++bgTotal; bgOn += mask.at(x, y) > 0; }
        }
    CHECK(hudOn > hudTotal * 0.9, "HUD locked %d/%d", hudOn, hudTotal);
    CHECK(bgOn < bgTotal / 100, "moving background not locked %d/%d", bgOn, bgTotal);
    // HUD disappears: released on the next frame.
    mask = hud.update(halve(lumaOf(render(bg, fg, 200, 0, -1000, -1000, 0, false))));
    int still = 0;
    for (int y = 4; y < 12; ++y) for (int x = 4; x < 52; ++x) still += mask.at(x, y) > 0;
    CHECK(still == 0, "released at once (%d)", still);
    // Size change resets.
    CHECK(hud.update(Gray(10, 10, 3)).px == std::vector<uint8_t>(100, 0), "size change resets");
}

// Simulated capture: 60 Hz transport, 30 FPS content, with optional 17/50 ms (1 then 3 refresh) pacing.
void testEngineTiming(bool uneven, bool repair, double target, int64_t refreshNs, double expectOut) {
    const Scene bg(160, 96, 420, 11), fg(16, 16, 2, 12);
    EngineSettings s;
    s.pacingRepair = repair;
    s.targetFps = target;
    s.estimateBudgetMs = 6;  // simulated clock: real motion estimation time (slow under sanitizers) must not move the delay
    Engine e(s);
    const int64_t T = 16666667;  // 60 Hz
    int64_t now = 1'000'000'000;
    // Content index per 60 Hz slot: even = each picture twice, uneven = 1, 3, 1, 3 ... repeats.
    std::vector<int> slots;
    for (int c = 1; slots.size() < 360; ++c)
        for (int r = 0; r < (uneven ? ((c % 2) ? 3 : 1) : 2); ++r) slots.push_back(c);
    std::vector<float> ts;
    int64_t display = now + 300 * 1000000LL;
    for (int slot = 0; slot < 360; ++slot) {
        const int contentIndex = slots[size_t(slot)];
        e.push(render(bg, fg, float(contentIndex * 2), 0, 50.f + float(contentIndex), 40, 16), now);
        e.waitIdle();
        now += T;
        while (display <= now - 100 * 1000000LL) {
            const Pick p = e.pick(display);
            if (slot > 120 && p.valid()) ts.push_back(float(p.aId) + p.t);
            display += refreshNs;
        }
    }
    const EngineStats st = e.stats();
    std::printf("  uneven=%d repair=%d target=%.0f: transport %.1f content %.1f output %.1f fps, delay %.1f ms, latency %.1f ms, late %llu\n",
                uneven, repair, target, st.transportFps, st.contentFps, st.outputFps, st.delayMs, st.latencyMs, (unsigned long long)st.lateFields);
    CHECK(std::abs(st.transportFps - 60.0) < 1.0, "transport");
    CHECK(std::abs(st.contentFps - 30.0) < 1.5, "content %.1f", st.contentFps);
    CHECK(std::abs(st.outputFps - expectOut) < expectOut * 0.06, "output %.1f expected %.1f", st.outputFps, expectOut);
    CHECK(st.duplicates > 100, "duplicates removed");
    CHECK(st.lateFields == 0, "fields ready in time");
    // Smoothness: steps of content position between consecutive pictures.
    std::vector<float> steps;
    for (size_t i = 1; i < ts.size(); ++i) if (ts[i] != ts[i - 1]) steps.push_back(ts[i] - ts[i - 1]);
    double mean = 0, var = 0;
    for (float d : steps) mean += d;
    mean /= double(steps.size());
    for (float d : steps) var += (d - mean) * (d - mean);
    const double sd = std::sqrt(var / double(steps.size()));
    std::printf("    picture step %.3f +- %.3f content frames\n", mean, sd);
    if (!uneven || repair) CHECK(sd < mean * 0.15, "even motion (sd %.3f of %.3f)", sd, mean);
    else CHECK(sd > mean * 0.3, "uneven input stays uneven without repair (sd %.3f of %.3f)", sd, mean);
}

void testEngineOff() {
    const Scene bg(64, 48, 40, 21), fg(8, 8, 1, 22);
    EngineSettings s;
    s.frameGeneration = false;
    Engine e(s);
    CHECK(!e.pick(0).valid(), "nothing before the first frame");
    int64_t now = 1'000'000;
    for (int i = 0; i < 10; ++i) {
        e.push(render(bg, fg, float(i * 4), 0, 10, 10, 8), now);
        const Pick p = e.pick(now);
        CHECK(p.valid() && p.a == p.b && !p.generated() && p.aId == uint64_t(i + 1), "newest frame at once");
        now += 33'000'000;
    }
}

// Abuse: random sizes, clocks going back, resolution changes, settings flipped from another thread,
// picks from a third thread. Run under the sanitizers in CI.
void testEngineAbuse() {
    Engine e;
    std::atomic<bool> done{false};
    std::thread picker([&] {
        int64_t t = 0;
        while (!done) { const Pick p = e.pick(t); if (p.valid()) { (void)p.a->px.size(); } t += 3'000'000; std::this_thread::yield(); }
    });
    std::thread flipper([&] {
        std::mt19937 rng(9);
        while (!done) {
            EngineSettings s;
            s.frameGeneration = rng() % 4 != 0;
            s.targetFps = double(rng() % 3) * 60.0;
            s.pacingRepair = rng() % 2;
            s.hudLock = rng() % 2;
            s.quality = int(rng() % 2);
            s.removeDuplicates = rng() % 3 != 0;
            e.setSettings(s);
            if (rng() % 50 == 0) e.reset();
            std::this_thread::sleep_for(std::chrono::microseconds(300));
        }
    });
    std::mt19937 rng(77);
    const int sizes[][2] = {{1, 1}, {2, 3}, {17, 9}, {64, 36}, {160, 90}, {7, 200}, {300, 2}, {33, 33}};
    int64_t clock = 0;
    for (int i = 0; i < 600; ++i) {
        const auto& sz = sizes[(i / 40) % 8];
        Bgra img(sz[0], sz[1]);
        for (auto& v : img.px) v = uint8_t(rng());
        if (rng() % 5 == 0 && i > 0) { /* duplicate content */ }
        const int jump = int(rng() % 10);
        clock += jump == 0 ? -50'000'000 : (jump == 1 ? 0 : 16'666'667);  // backwards, equal, normal
        e.push(std::move(img), clock);
        if (i % 7 == 0) e.push(Bgra(), clock);  // empty frames are ignored
    }
    e.waitIdle();
    done = true;
    picker.join();
    flipper.join();
    const EngineStats st = e.stats();
    CHECK(st.framesIn == 600, "frames counted %llu", (unsigned long long)st.framesIn);
    CHECK(std::isfinite(st.contentFps) && std::isfinite(st.latencyMs) && std::isfinite(st.delayMs), "stats finite");
}

// Big motion, flash frames, black frames: no crash, sane output.
void testEngineHardContent() {
    const Scene bg(200, 120, 400, 31), fg(16, 16, 2, 32);
    Engine e;
    int64_t now = 0;
    for (int i = 0; i < 40; ++i) {
        Bgra img;
        if (i % 13 == 5) img = Bgra(200, 120);                        // black frame
        else if (i % 11 == 7) { img = Bgra(200, 120); for (size_t k = 0; k < img.px.size(); ++k) if (k % 4 != 3) img.px[k] = 255; }  // flash
        else img = render(bg, fg, float(i * 150 % 700) - 350.f, float(i * 37 % 200) - 100.f, 20, 20, 16);  // huge jumps
        e.push(std::move(img), now);
        e.waitIdle();
        for (int k = 0; k < 4; ++k) {
            const Pick p = e.pick(now + k * 8'333'333);
            if (p.generated()) {
                Bgra out;
                CHECK(interpolate(*p.a, *p.b, p.fields->forward, p.fields->backward, p.fields->hudMask.get(), p.t, out, 2), "interp");
            }
        }
        now += 33'333'333;
    }
    CHECK(e.stats().sceneCuts > 0, "cuts seen in random jumps");
}

void benchmark() {
    const Scene bg(1920, 1080, 64, 41), fg(200, 200, 8, 42);
    const Bgra a = render(bg, fg, 0, 0, 800, 400, 200), b = render(bg, fg, 12, 4, 760, 400, 200);
    auto t0 = std::chrono::steady_clock::now();
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    auto t1 = std::chrono::steady_clock::now();
    const MotionEstimator est;
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    auto t2 = std::chrono::steady_clock::now();
    MotionSettings fast; fast.finestLevel = 2;
    const MotionEstimator estFast(fast);
    (void)estFast.estimate(pa, pb);
    auto t3 = std::chrono::steady_clock::now();
    Bgra out;
    interpolate(a, b, f, k, nullptr, 0.5f, out, int(std::max(1u, std::thread::hardware_concurrency())));
    auto t4 = std::chrono::steady_clock::now();
    auto ms = [](auto x, auto y) { return std::chrono::duration<double, std::milli>(y - x).count(); };
    std::printf("  1080p: pyramids %.1f ms, motion both ways %.1f ms (fast one way %.1f ms), CPU interpolation %.1f ms on %u threads\n",
                ms(t0, t1), ms(t1, t2), ms(t2, t3), ms(t3, t4), std::thread::hardware_concurrency());
}

int main(int argc, char** argv) {
    const bool bench = argc > 1 && std::string(argv[1]) == "--bench";
    testConvert();
    testPyramid();
    testMotion();
    testInterpolate();
    testDedup();
    testHudLock();
    testEngineTiming(false, false, 0, 8333333, 120.0);   // 30 -> 120 Hz display, match display
    testEngineTiming(false, false, 60, 8333333, 60.0);   // capped at 60
    testEngineTiming(true, false, 0, 8333333, 120.0);    // uneven 17/50 ms, no repair
    testEngineTiming(true, true, 0, 8333333, 120.0);     // uneven, repaired
    testEngineTiming(false, false, 0, 16666667, 60.0);   // 60 Hz display
    testEngineOff();
    testEngineHardContent();
    testEngineAbuse();
    if (bench) benchmark();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
