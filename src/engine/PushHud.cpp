// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/engine/PushHud.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace veyra::engine::hud {
namespace {

float halfToFloat(uint16_t h) {
    const uint32_t sign = uint32_t(h >> 15) << 31;
    uint32_t exponent = (h >> 10) & 0x1F, mantissa = h & 0x3FF, bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) bits = sign;
        else { // subnormal
            exponent = 127 - 15 + 1;
            while (!(mantissa & 0x400)) { mantissa <<= 1; --exponent; }
            mantissa &= 0x3FF;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) bits = sign | 0x7F800000 | (mantissa << 13);
    else bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

struct Rgb { float r, g, b; }; // 0..255

Rgb pixel(const uint8_t* row, uint32_t x, PixelLayout layout) {
    switch (layout) {
    case PixelLayout::Rgba8: { const uint8_t* p = row + size_t(x) * 4; return {float(p[0]), float(p[1]), float(p[2])}; }
    case PixelLayout::Bgra8: { const uint8_t* p = row + size_t(x) * 4; return {float(p[2]), float(p[1]), float(p[0])}; }
    case PixelLayout::Rgb10a2: {
        uint32_t v; std::memcpy(&v, row + size_t(x) * 4, 4);
        return {float((v & 0x3FF) >> 2), float(((v >> 10) & 0x3FF) >> 2), float(((v >> 20) & 0x3FF) >> 2)};
    }
    case PixelLayout::Rgba16f: {
        uint16_t c[3]; std::memcpy(c, row + size_t(x) * 8, 6);
        const auto encode = [](float linear) {
            if (!std::isfinite(linear)) return 0.0f;
            return std::sqrt(std::clamp(linear, 0.0f, 1.0f)) * 255.0f;
        };
        return {encode(halfToFloat(c[0])), encode(halfToFloat(c[1])), encode(halfToFloat(c[2]))};
    }
    }
    return {0, 0, 0};
}

float lumaOf(const Rgb& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

struct Box { uint32_t x0, y0, x1, y1; uint32_t cells; }; // inclusive cell bounds

uint32_t area(const Box& b) { return (b.x1 - b.x0 + 1) * (b.y1 - b.y0 + 1); }

} // namespace

bool reduce(const uint8_t* data, size_t pitch, uint32_t width, uint32_t height, PixelLayout layout,
            LumaFrame& out, std::vector<uint8_t>* previewRgb) {
    if (!data || width < kGridWidth / 4 || height < kGridHeight / 4) return false;
    const size_t bpp = layout == PixelLayout::Rgba16f ? 8 : 4;
    if (pitch < size_t(width) * bpp) return false;
    out.luma.assign(size_t(kGridWidth) * kGridHeight, 0);
    for (uint32_t gy = 0; gy < kGridHeight; ++gy) {
        const double y0 = double(gy) * height / kGridHeight, y1 = double(gy + 1) * height / kGridHeight;
        for (uint32_t gx = 0; gx < kGridWidth; ++gx) {
            const double x0 = double(gx) * width / kGridWidth, x1 = double(gx + 1) * width / kGridWidth;
            float sum = 0;
            for (int sy = 0; sy < 3; ++sy) {
                const uint32_t y = std::min<uint32_t>(height - 1, uint32_t(y0 + (y1 - y0) * (2 * sy + 1) / 6.0));
                const uint8_t* row = data + size_t(y) * pitch;
                for (int sx = 0; sx < 3; ++sx) {
                    const uint32_t x = std::min<uint32_t>(width - 1, uint32_t(x0 + (x1 - x0) * (2 * sx + 1) / 6.0));
                    sum += lumaOf(pixel(row, x, layout));
                }
            }
            out.luma[size_t(gy) * kGridWidth + gx] = uint8_t(std::clamp(std::lround(sum / 9.0f), 0L, 255L));
        }
    }
    if (previewRgb) {
        previewRgb->assign(size_t(kPreviewWidth) * kPreviewHeight * 3, 0);
        for (uint32_t py = 0; py < kPreviewHeight; ++py) {
            const uint32_t y = std::min<uint32_t>(height - 1, uint32_t((py + 0.5) * height / kPreviewHeight));
            const uint8_t* row = data + size_t(y) * pitch;
            for (uint32_t px = 0; px < kPreviewWidth; ++px) {
                const uint32_t x = std::min<uint32_t>(width - 1, uint32_t((px + 0.5) * width / kPreviewWidth));
                const Rgb c = pixel(row, x, layout);
                uint8_t* o = previewRgb->data() + (size_t(py) * kPreviewWidth + px) * 3;
                o[0] = uint8_t(std::clamp(c.r, 0.0f, 255.0f));
                o[1] = uint8_t(std::clamp(c.g, 0.0f, 255.0f));
                o[2] = uint8_t(std::clamp(c.b, 0.0f, 255.0f));
            }
        }
    }
    return true;
}

Detection detect(const std::vector<LumaFrame>& frames, size_t maxRegions) {
    Detection result;
    const size_t points = size_t(kGridWidth) * kGridHeight;
    if (frames.size() < kMinFrames || maxRegions == 0) return result;
    for (const auto& f : frames) if (f.luma.size() != points) return result;

    // Per grid point: mean and standard deviation over time.
    std::vector<float> mean(points, 0), deviation(points, 0);
    const float n = float(frames.size());
    for (const auto& f : frames)
        for (size_t i = 0; i < points; ++i) mean[i] += f.luma[i];
    for (auto& m : mean) m /= n;
    for (const auto& f : frames)
        for (size_t i = 0; i < points; ++i) { const float d = f.luma[i] - mean[i]; deviation[i] += d * d; }
    for (auto& d : deviation) d = std::sqrt(d / n);

    // Cells of 2x2 grid points: the largest deviation inside, then the
    // picture's typical motion (median cell).
    const size_t cells = size_t(kCellsX) * kCellsY;
    std::vector<float> cellDeviation(cells, 0), cellDetail(cells, 0);
    for (uint32_t cy = 0; cy < kCellsY; ++cy)
        for (uint32_t cx = 0; cx < kCellsX; ++cx) {
            float dev = 0;
            for (uint32_t oy = 0; oy < 2; ++oy)
                for (uint32_t ox = 0; ox < 2; ++ox) dev = std::max(dev, deviation[size_t(cy * 2 + oy) * kGridWidth + cx * 2 + ox]);
            cellDeviation[size_t(cy) * kCellsX + cx] = dev;
        }
    std::vector<float> sorted = cellDeviation;
    std::nth_element(sorted.begin(), sorted.begin() + std::ptrdiff_t(sorted.size() / 2), sorted.end());
    result.motion = sorted[sorted.size() / 2];
    if (result.motion < 1.5f) { result.tooStill = true; return result; }

    // Static: near capture noise and well below the picture's typical motion.
    const float stillBelow = std::clamp(0.3f * result.motion, 1.0f, 2.5f);

    // Detail: luma steps of the mean picture between two neighbouring points
    // that both stay still. Static text and icons keep their edges; the edge
    // between a flat static bar and the moving scene does not count, since one
    // side of it moves.
    std::vector<float> detail(points, 0);
    for (uint32_t y = 0; y < kGridHeight; ++y)
        for (uint32_t x = 0; x < kGridWidth; ++x) {
            const size_t i = size_t(y) * kGridWidth + x;
            if (deviation[i] > stillBelow) continue;
            const auto step = [&](size_t j) {
                if (deviation[j] > stillBelow) return;
                const float d = std::fabs(mean[i] - mean[j]);
                detail[i] = std::max(detail[i], d);
                detail[j] = std::max(detail[j], d);
            };
            if (x + 1 < kGridWidth) step(i + 1);
            if (y + 1 < kGridHeight) step(i + kGridWidth);
        }
    for (uint32_t cy = 0; cy < kCellsY; ++cy)
        for (uint32_t cx = 0; cx < kCellsX; ++cx) {
            float det = 0;
            for (uint32_t oy = 0; oy < 2; ++oy)
                for (uint32_t ox = 0; ox < 2; ++ox) det = std::max(det, detail[size_t(cy * 2 + oy) * kGridWidth + cx * 2 + ox]);
            cellDetail[size_t(cy) * kCellsX + cx] = det;
        }

    constexpr float kDetail = 6.0f;
    std::vector<uint8_t> candidate(cells, 0);
    for (size_t i = 0; i < cells; ++i)
        if (cellDeviation[i] <= stillBelow && cellDetail[i] >= kDetail) { candidate[i] = 1; ++result.candidateCells; }

    // Close gaps between letters and icon parts (3x3 dilation), then group.
    std::vector<uint8_t> grown(cells, 0);
    for (uint32_t cy = 0; cy < kCellsY; ++cy)
        for (uint32_t cx = 0; cx < kCellsX; ++cx) {
            if (!candidate[size_t(cy) * kCellsX + cx]) continue;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int x = int(cx) + dx, y = int(cy) + dy;
                    if (x >= 0 && y >= 0 && x < int(kCellsX) && y < int(kCellsY)) grown[size_t(y) * kCellsX + size_t(x)] = 1;
                }
        }
    std::vector<int> label(cells, -1);
    std::vector<Box> boxes;
    std::vector<uint32_t> stack;
    for (uint32_t start = 0; start < cells; ++start) {
        if (!grown[start] || label[start] >= 0) continue;
        Box box{start % kCellsX, start / kCellsX, start % kCellsX, start / kCellsX, 0};
        const int id = int(boxes.size());
        stack.assign(1, start);
        label[start] = id;
        while (!stack.empty()) {
            const uint32_t i = stack.back(); stack.pop_back();
            const uint32_t x = i % kCellsX, y = i / kCellsX;
            box.x0 = std::min(box.x0, x); box.x1 = std::max(box.x1, x);
            box.y0 = std::min(box.y0, y); box.y1 = std::max(box.y1, y);
            box.cells += candidate[i];
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = int(x) + dx, ny = int(y) + dy;
                    if (nx < 0 || ny < 0 || nx >= int(kCellsX) || ny >= int(kCellsY)) continue;
                    const uint32_t j = uint32_t(ny) * kCellsX + uint32_t(nx);
                    if (grown[j] && label[j] < 0) { label[j] = id; stack.push_back(j); }
                }
        }
        boxes.push_back(box);
    }

    // A HUD element covers a modest part of the picture and is reasonably
    // dense; a whole static half (a still scene part) is not one.
    const uint32_t maxArea = uint32_t(cells / 5);
    std::vector<Box> kept;
    for (const auto& b : boxes)
        if (b.cells >= 4 && area(b) <= maxArea && float(b.cells) / float(area(b)) >= 0.12f) kept.push_back(b);

    // Merge boxes closer than three cells (a health bar next to its icon).
    for (bool merged = true; merged;) {
        merged = false;
        for (size_t a = 0; a < kept.size() && !merged; ++a)
            for (size_t b = a + 1; b < kept.size() && !merged; ++b) {
                const auto& p = kept[a];
                const auto& q = kept[b];
                const int gapX = std::max(int(q.x0) - int(p.x1), int(p.x0) - int(q.x1)) - 1;
                const int gapY = std::max(int(q.y0) - int(p.y1), int(p.y0) - int(q.y1)) - 1;
                if (std::max(gapX, gapY) > 3) continue;
                const Box u{std::min(p.x0, q.x0), std::min(p.y0, q.y0), std::max(p.x1, q.x1), std::max(p.y1, q.y1), p.cells + q.cells};
                if (area(u) > maxArea) continue;
                kept[a] = u;
                kept.erase(kept.begin() + std::ptrdiff_t(b));
                merged = true;
            }
    }
    std::sort(kept.begin(), kept.end(), [](const Box& a, const Box& b) { return a.cells > b.cells; });
    if (kept.size() > maxRegions) kept.resize(maxRegions);
    for (const auto& b : kept) {
        const uint32_t x0 = b.x0 ? b.x0 - 1 : 0, y0 = b.y0 ? b.y0 - 1 : 0;
        const uint32_t x1 = std::min(kCellsX - 1, b.x1 + 1), y1 = std::min(kCellsY - 1, b.y1 + 1);
        result.regions.push_back({float(x0) / kCellsX, float(y0) / kCellsY, float(x1 + 1) / kCellsX, float(y1 + 1) / kCellsY});
    }
    return result;
}

} // namespace veyra::engine::hud
