// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/portable/Convert.h"

#include <algorithm>

namespace veyra::portable {
namespace {

struct Coeffs {
    int y, rv, gu, gv, bu;  // 16.16 fixed point
    int yOff, cOff;
};

Coeffs coeffsFor(Matrix m, bool full) {
    // Limited range: Y 16-235, C 16-240. Full range: Y 0-255, C 0-255 around 128.
    const double ys = full ? 1.0 : 255.0 / 219.0, cs = full ? 1.0 : 255.0 / 224.0;
    double kr, kb;
    if (m == Matrix::Bt601) { kr = 0.299; kb = 0.114; } else { kr = 0.2126; kb = 0.0722; }
    const double kg = 1.0 - kr - kb;
    const double rv = 2.0 * (1.0 - kr), bu = 2.0 * (1.0 - kb);
    const double gu = -bu * kb / kg, gv = -rv * kr / kg;
    auto fx = [](double v) { return int(v * 65536.0 + (v < 0 ? -0.5 : 0.5)); };
    return {fx(ys), fx(rv * cs), fx(gu * cs), fx(gv * cs), fx(bu * cs), full ? 0 : 16, 128};
}

inline uint8_t clamp8(int v) { return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v)); }

inline void put(uint8_t* o, const Coeffs& c, int y, int u, int v) {
    const int yy = (y - c.yOff) * c.y + 32768;
    const int uu = u - c.cOff, vv = v - c.cOff;
    o[0] = clamp8((yy + c.bu * uu) >> 16);
    o[1] = clamp8((yy + c.gu * uu + c.gv * vv) >> 16);
    o[2] = clamp8((yy + c.rv * vv) >> 16);
    o[3] = 255;
}

bool planeOk(const PlaneView& p, int minStride) { return p.data && p.stride >= minStride; }

}  // namespace

bool convertToBgra(const SourceFrame& in, Bgra& out) {
    const int w = in.width, h = in.height;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return false;
    const Coeffs c = coeffsFor(in.matrix, in.fullRange);
    const int cw = (w + 1) / 2;
    switch (in.format) {
    case PixelFormat::Nv12:
        if (!planeOk(in.planes[0], w) || !planeOk(in.planes[1], cw * 2)) return false;
        break;
    case PixelFormat::P010:
        if (!planeOk(in.planes[0], w * 2) || !planeOk(in.planes[1], cw * 4)) return false;
        break;
    case PixelFormat::Yuyv:
    case PixelFormat::Uyvy:
        if (!planeOk(in.planes[0], cw * 4)) return false;
        break;
    case PixelFormat::I420:
        if (!planeOk(in.planes[0], w) || !planeOk(in.planes[1], cw) || !planeOk(in.planes[2], cw)) return false;
        break;
    case PixelFormat::Bgra:
    case PixelFormat::Rgba:
        if (!planeOk(in.planes[0], w * 4)) return false;
        break;
    default:
        return false;
    }
    if (out.width != w || out.height != h) out = Bgra(w, h);
    for (int y = 0; y < h; ++y) {
        uint8_t* o = out.pixel(0, y);
        const uint8_t* p0 = in.planes[0].data + size_t(y) * size_t(in.planes[0].stride);
        switch (in.format) {
        case PixelFormat::Nv12: {
            const uint8_t* uv = in.planes[1].data + size_t(y / 2) * size_t(in.planes[1].stride);
            for (int x = 0; x < w; ++x) put(o + x * 4, c, p0[x], uv[(x / 2) * 2], uv[(x / 2) * 2 + 1]);
            break;
        }
        case PixelFormat::P010: {
            // 10 bits in the high bits of little-endian 16-bit words: the high byte is the top 8 bits.
            const uint8_t* uv = in.planes[1].data + size_t(y / 2) * size_t(in.planes[1].stride);
            for (int x = 0; x < w; ++x) put(o + x * 4, c, p0[x * 2 + 1], uv[(x / 2) * 4 + 1], uv[(x / 2) * 4 + 3]);
            break;
        }
        case PixelFormat::Yuyv:
            for (int x = 0; x < w; ++x) {
                const uint8_t* q = p0 + (x / 2) * 4;
                put(o + x * 4, c, q[(x & 1) ? 2 : 0], q[1], q[3]);
            }
            break;
        case PixelFormat::Uyvy:
            for (int x = 0; x < w; ++x) {
                const uint8_t* q = p0 + (x / 2) * 4;
                put(o + x * 4, c, q[(x & 1) ? 3 : 1], q[0], q[2]);
            }
            break;
        case PixelFormat::I420: {
            const uint8_t* u = in.planes[1].data + size_t(y / 2) * size_t(in.planes[1].stride);
            const uint8_t* v = in.planes[2].data + size_t(y / 2) * size_t(in.planes[2].stride);
            for (int x = 0; x < w; ++x) put(o + x * 4, c, p0[x], u[x / 2], v[x / 2]);
            break;
        }
        case PixelFormat::Bgra:
            for (int x = 0; x < w; ++x) { o[x * 4] = p0[x * 4]; o[x * 4 + 1] = p0[x * 4 + 1]; o[x * 4 + 2] = p0[x * 4 + 2]; o[x * 4 + 3] = 255; }
            break;
        case PixelFormat::Rgba:
            for (int x = 0; x < w; ++x) { o[x * 4] = p0[x * 4 + 2]; o[x * 4 + 1] = p0[x * 4 + 1]; o[x * 4 + 2] = p0[x * 4]; o[x * 4 + 3] = 255; }
            break;
        }
    }
    return true;
}

Gray lumaFromSource(const SourceFrame& in) {
    const int w = in.width, h = in.height;
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || !in.planes[0].data) return {};
    const int cw = (w + 1) / 2;
    int step = 1, offset = 0, minStride = w;
    switch (in.format) {
    case PixelFormat::Nv12: case PixelFormat::I420: break;
    case PixelFormat::P010: step = 2; offset = 1; minStride = w * 2; break;
    case PixelFormat::Yuyv: step = 2; offset = 0; minStride = cw * 4; break;
    case PixelFormat::Uyvy: step = 2; offset = 1; minStride = cw * 4; break;
    case PixelFormat::Bgra: case PixelFormat::Rgba: {
        if (in.planes[0].stride < w * 4) return {};
        Gray g(w, h);
        const bool rgba = in.format == PixelFormat::Rgba;
        for (int y = 0; y < h; ++y) {
            const uint8_t* p = in.planes[0].data + size_t(y) * size_t(in.planes[0].stride);
            for (int x = 0; x < w; ++x) {
                const uint8_t* q = p + x * 4;
                const uint8_t bgra[4] = {rgba ? q[2] : q[0], q[1], rgba ? q[0] : q[2], 255};
                g.at(x, y) = lumaOf(bgra);
            }
        }
        return g;
    }
    default: return {};
    }
    if (in.planes[0].stride < minStride) return {};
    // Limited range (16-235) is stretched to full range so thresholds mean the same for every source.
    uint8_t lut[256];
    for (int v = 0; v < 256; ++v) lut[v] = in.fullRange ? uint8_t(v) : clamp8(((v - 16) * 255 + 109) / 219);
    Gray g(w, h);
    for (int y = 0; y < h; ++y) {
        const uint8_t* p = in.planes[0].data + size_t(y) * size_t(in.planes[0].stride) + offset;
        uint8_t* o = g.px.data() + size_t(y) * size_t(w);
        for (int x = 0; x < w; ++x) o[x] = lut[p[x * step]];
    }
    return g;
}

}  // namespace veyra::portable
