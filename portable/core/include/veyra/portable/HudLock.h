// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Live HUD Lock for the Linux / macOS build: the CPU version of shaders/PushHudLockUpdate.hlsl and
// PushHudLockCompose.hlsl. Per pixel of the half-size luma of every real frame: how long it has stayed
// still (rises toward 1 while unchanged, drops to 0 the moment it changes) and the contrast it held
// while still. The mask (0..255) marks still pixels with an edge (text, crosshairs, bars) and their
// neighbours; generated frames take those pixels from the nearer real frame.
#include "veyra/portable/Image.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace veyra::portable {

class HudLock {
public:
    void reset() { still_.clear(); held_.clear(); previous_ = {}; }

    // Feed the half-size luma of each real (kept) frame in order. Returns the mask for generated frames
    // between this frame and the next.
    Gray update(const Gray& luma) {
        const int w = luma.width, h = luma.height;
        Gray mask(w, h, 0);
        if (luma.empty()) return mask;
        const size_t n = size_t(w) * size_t(h);
        if (previous_.width != w || previous_.height != h) {
            still_.assign(n, 0.f);
            held_.assign(n, 0.f);
            previous_ = luma;
            return mask;
        }
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const size_t i = size_t(y) * size_t(w) + size_t(x);
                const int v = luma.at(x, y);
                const bool still = std::abs(v - int(previous_.at(x, y))) < 5;
                float& s = still_[i];
                s = still ? s + (1.f - s) * 0.06f : 0.f;
                // Contrast: largest difference to the 4 neighbours.
                int c = 0;
                if (x > 0) c = std::max(c, std::abs(v - int(luma.at(x - 1, y))));
                if (x + 1 < w) c = std::max(c, std::abs(v - int(luma.at(x + 1, y))));
                if (y > 0) c = std::max(c, std::abs(v - int(luma.at(x, y - 1))));
                if (y + 1 < h) c = std::max(c, std::abs(v - int(luma.at(x, y + 1))));
                float& hc = held_[i];
                hc = still ? std::max(float(c), hc * 0.97f) : float(c);
            }
        previous_ = luma;
        // Locked: still for long enough (smoothstep 0.80..0.95), with an edge within 1 pixel.
        auto smooth = [](float e0, float e1, float x) {
            const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        };
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const size_t i = size_t(y) * size_t(w) + size_t(x);
                const float stillness = smooth(0.80f, 0.95f, still_[i]);
                if (stillness <= 0.f) continue;
                float edge = 0.f;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int xx = x + dx, yy = y + dy;
                        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                        const size_t j = size_t(yy) * size_t(w) + size_t(xx);
                        edge = std::max(edge, smooth(10.f, 25.f, held_[j]) * smooth(0.80f, 0.95f, still_[j]));
                    }
                mask.at(x, y) = uint8_t(std::lround(stillness * edge * 255.f));
            }
        return mask;
    }

private:
    std::vector<float> still_, held_;
    Gray previous_;
};

}  // namespace veyra::portable
