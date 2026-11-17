// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/portable/Interpolate.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace veyra::portable {
namespace {

inline float smoothstep(float e0, float e1, float x) {
    if (e1 <= e0) return x < e0 ? 0.f : 1.f;
    const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

float sampleMask(const Gray& m, float u, float v) {
    // u, v in 0..1 over the picture.
    const float x = std::clamp(u * float(m.width) - 0.5f, 0.f, float(m.width - 1));
    const float y = std::clamp(v * float(m.height) - 0.5f, 0.f, float(m.height - 1));
    const int x0 = int(x), y0 = int(y), x1 = std::min(x0 + 1, m.width - 1), y1 = std::min(y0 + 1, m.height - 1);
    const float fx = x - float(x0), fy = y - float(y0);
    const float top = float(m.at(x0, y0)) + (float(m.at(x1, y0)) - float(m.at(x0, y0))) * fx;
    const float bottom = float(m.at(x0, y1)) + (float(m.at(x1, y1)) - float(m.at(x0, y1))) * fx;
    return (top + (bottom - top) * fy) / 255.f;
}

}  // namespace

void sampleBilinear(const Bgra& image, float x, float y, float out[4]) {
    if (!std::isfinite(x)) x = 0;
    if (!std::isfinite(y)) y = 0;
    const float fxc = std::clamp(x - 0.5f, 0.f, float(image.width - 1));
    const float fyc = std::clamp(y - 0.5f, 0.f, float(image.height - 1));
    const int x0 = int(fxc), y0 = int(fyc);
    const int x1 = std::min(x0 + 1, image.width - 1), y1 = std::min(y0 + 1, image.height - 1);
    const float fx = fxc - float(x0), fy = fyc - float(y0);
    const uint8_t *p00 = image.pixel(x0, y0), *p10 = image.pixel(x1, y0), *p01 = image.pixel(x0, y1), *p11 = image.pixel(x1, y1);
    for (int c = 0; c < 4; ++c) {
        const float top = float(p00[c]) + (float(p10[c]) - float(p00[c])) * fx;
        const float bottom = float(p01[c]) + (float(p11[c]) - float(p01[c])) * fx;
        out[c] = top + (bottom - top) * fy;
    }
}

bool interpolate(const Bgra& a, const Bgra& b, const MotionField& forward, const MotionField& backward,
                 const Gray* mask, float t, Bgra& out, int threads, InterpolateSettings s) {
    if (a.empty() || a.width != b.width || a.height != b.height) return false;
    if (!std::isfinite(t)) t = 0;
    t = std::clamp(t, 0.f, 1.f);
    if (out.width != a.width || out.height != a.height) out = Bgra(a.width, a.height);
    const Bgra& nearer = t < 0.5f ? a : b;
    const bool fieldsOk = !forward.empty() && !backward.empty() && !forward.sceneCut && !backward.sceneCut &&
                          forward.width == a.width && forward.height == a.height &&
                          backward.width == a.width && backward.height == a.height;
    if (!fieldsOk || t == 0.f || t == 1.f) {
        out.px = (t == 1.f ? b : nearer).px;
        return true;
    }
    const bool useMask = mask && !mask->empty();
    const float W = float(a.width), H = float(a.height);
    auto rows = [&](int y0, int y1) {
        float a1[4], b1[4], a2[4], b2[4];
        for (int y = y0; y < y1; ++y) {
            uint8_t* o = out.pixel(0, y);
            for (int x = 0; x < a.width; ++x, o += 4) {
                const float px = float(x) + 0.5f, py = float(y) + 0.5f;
                float fx, fy, kx, ky;
                forward.sample(px, py, fx, fy);
                backward.sample(px, py, kx, ky);
                kx = -kx; ky = -ky;
                sampleBilinear(a, px - t * fx, py - t * fy, a1);
                sampleBilinear(b, px + (1.f - t) * fx, py + (1.f - t) * fy, b1);
                sampleBilinear(a, px - t * kx, py - t * ky, a2);
                sampleBilinear(b, px + (1.f - t) * kx, py + (1.f - t) * ky, b2);
                const float e1 = (std::abs(a1[0] - b1[0]) + std::abs(a1[1] - b1[1]) + std::abs(a1[2] - b1[2])) / 3.f;
                const float e2 = (std::abs(a2[0] - b2[0]) + std::abs(a2[1] - b2[1]) + std::abs(a2[2] - b2[2])) / 3.f;
                const float w1 = 1.f / ((e1 + 4.f) * (e1 + 4.f)), w2 = 1.f / ((e2 + 4.f) * (e2 + 4.f));
                const float occlusion = smoothstep(s.errorLow, s.errorHigh, std::min(e1, e2));
                const bool firstBetter = e1 <= e2;
                const float* nearSample = t < 0.5f ? (firstBetter ? a1 : a2) : (firstBetter ? b1 : b2);
                float lock = 0.f;
                if (useMask) lock = sampleMask(*mask, px / W, py / H);
                const uint8_t* held = nearer.pixel(x, y);
                for (int c = 0; c < 3; ++c) {
                    const float c1 = a1[c] + (b1[c] - a1[c]) * t;
                    const float c2 = a2[c] + (b2[c] - a2[c]) * t;
                    float v = (c1 * w1 + c2 * w2) / (w1 + w2);
                    v += (nearSample[c] - v) * occlusion;
                    v += (float(held[c]) - v) * lock;
                    o[c] = uint8_t(std::clamp(v + 0.5f, 0.f, 255.f));
                }
                o[3] = 255;
            }
        }
    };
    threads = std::clamp(threads, 1, 64);
    if (threads == 1 || a.height < threads * 8) {
        rows(0, a.height);
        return true;
    }
    std::vector<std::thread> pool;
    const int per = (a.height + threads - 1) / threads;
    for (int k = 0; k < threads; ++k) {
        const int y0 = k * per, y1 = std::min(a.height, y0 + per);
        if (y0 >= y1) break;
        pool.emplace_back(rows, y0, y1);
    }
    for (auto& th : pool) th.join();
    return true;
}

}  // namespace veyra::portable
