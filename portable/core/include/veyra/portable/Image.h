// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Plain images for the Linux / macOS build (portable/). No Qt, no GPU: the core is tested on its own.
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace veyra::portable {

// 8-bit single-channel image (luma), rows packed (stride == width).
struct Gray {
    int width = 0, height = 0;
    std::vector<uint8_t> px;
    Gray() = default;
    Gray(int w, int h, uint8_t fill = 0) : width(w > 0 ? w : 0), height(h > 0 ? h : 0), px(size_t(width) * size_t(height), fill) {}
    bool empty() const { return width <= 0 || height <= 0; }
    uint8_t at(int x, int y) const { return px[size_t(y) * size_t(width) + size_t(x)]; }
    uint8_t& at(int x, int y) { return px[size_t(y) * size_t(width) + size_t(x)]; }
    // Clamped to the edge: motion search reads outside the picture.
    uint8_t clamped(int x, int y) const {
        x = x < 0 ? 0 : (x >= width ? width - 1 : x);
        y = y < 0 ? 0 : (y >= height ? height - 1 : y);
        return at(x, y);
    }
};

// 8-bit BGRA, rows packed (stride == 4 * width). Alpha is always 255.
struct Bgra {
    int width = 0, height = 0;
    std::vector<uint8_t> px;
    Bgra() = default;
    Bgra(int w, int h) : width(w > 0 ? w : 0), height(h > 0 ? h : 0), px(size_t(width) * size_t(height) * 4, 0) {
        for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
    }
    bool empty() const { return width <= 0 || height <= 0; }
    const uint8_t* pixel(int x, int y) const { return px.data() + (size_t(y) * size_t(width) + size_t(x)) * 4; }
    uint8_t* pixel(int x, int y) { return px.data() + (size_t(y) * size_t(width) + size_t(x)) * 4; }
};

// BT.709 luma from BGRA, integer weights (sum 256).
inline uint8_t lumaOf(const uint8_t* bgra) { return uint8_t((bgra[2] * 54 + bgra[1] * 183 + bgra[0] * 19 + 128) >> 8); }

inline Gray lumaOf(const Bgra& image) {
    Gray g(image.width, image.height);
    const size_t n = size_t(image.width) * size_t(image.height);
    for (size_t i = 0; i < n; ++i) g.px[i] = lumaOf(image.px.data() + i * 4);
    return g;
}

// Half size, 2x2 box filter (odd edges repeat the last row / column).
inline Gray halve(const Gray& s) {
    if (s.empty()) return {};
    const int w = (s.width + 1) / 2, h = (s.height + 1) / 2;
    Gray d(w, h);
    for (int y = 0; y < h; ++y) {
        const int y0 = 2 * y, y1 = y0 + 1 < s.height ? y0 + 1 : y0;
        for (int x = 0; x < w; ++x) {
            const int x0 = 2 * x, x1 = x0 + 1 < s.width ? x0 + 1 : x0;
            d.at(x, y) = uint8_t((s.at(x0, y0) + s.at(x1, y0) + s.at(x0, y1) + s.at(x1, y1) + 2) >> 2);
        }
    }
    return d;
}

// Luma pyramid: level 0 is the full-size luma, each next level half the size.
struct Pyramid {
    std::vector<Gray> levels;
    static Pyramid of(const Gray& full, int count) { return of(Gray(full), count); }
    static Pyramid of(Gray&& full, int count) {
        Pyramid p;
        if (full.empty() || count <= 0) return p;
        p.levels.push_back(std::move(full));
        for (int i = 1; i < count && p.levels.back().width > 8 && p.levels.back().height > 8; ++i) p.levels.push_back(halve(p.levels.back()));
        return p;
    }
};

}  // namespace veyra::portable
