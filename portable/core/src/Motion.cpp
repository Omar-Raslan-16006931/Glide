// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/portable/Motion.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace veyra::portable {
namespace {

constexpr int kBlock = 8;  // block size at every search level

struct IV { int x = 0, y = 0; };

inline int iabs(int v) { return v < 0 ? -v : v; }

int median3(int a, int b, int c) { return std::max(std::min(a, b), std::min(std::max(a, b), c)); }

// Sum of absolute differences of the 8x8 block at (x0, y0) in a against b displaced by (dx, dy).
// The fast path runs when nothing is read outside either picture.
int sad(const Gray& a, const Gray& b, int x0, int y0, int dx, int dy) {
    const int bw = std::min(kBlock, a.width - x0), bh = std::min(kBlock, a.height - y0);
    if (bw <= 0 || bh <= 0) return 0;
    const int bx = x0 + dx, by = y0 + dy;
    int sum = 0;
    if (bx >= 0 && by >= 0 && bx + bw <= b.width && by + bh <= b.height) {
        for (int y = 0; y < bh; ++y) {
            const uint8_t* pa = a.px.data() + size_t(y0 + y) * size_t(a.width) + size_t(x0);
            const uint8_t* pb = b.px.data() + size_t(by + y) * size_t(b.width) + size_t(bx);
            for (int x = 0; x < bw; ++x) sum += iabs(int(pa[x]) - int(pb[x]));
        }
    } else {
        for (int y = 0; y < bh; ++y)
            for (int x = 0; x < bw; ++x) sum += iabs(int(a.at(x0 + x, y0 + y)) - int(b.clamped(bx + x, by + y)));
    }
    // Partial edge blocks are scaled to a full block so their costs compare with the rest.
    if (bw * bh != kBlock * kBlock) sum = sum * (kBlock * kBlock) / (bw * bh);
    return sum;
}

struct Grid {
    int bx = 0, by = 0;
    std::vector<IV> v;
    std::vector<int> cost;  // raw SAD of the chosen vector
    std::vector<int> zero;  // raw SAD without motion
    IV get(int i, int j) const {
        i = std::clamp(i, 0, bx - 1);
        j = std::clamp(j, 0, by - 1);
        return v[size_t(j) * size_t(bx) + size_t(i)];
    }
};

}  // namespace

void MotionField::sample(float x, float y, float& outX, float& outY) const {
    outX = outY = 0;
    if (empty()) return;
    const float gx = std::clamp(x / float(blockSize) - 0.5f, 0.0f, float(blocksX - 1));
    const float gy = std::clamp(y / float(blockSize) - 0.5f, 0.0f, float(blocksY - 1));
    const int i0 = int(gx), j0 = int(gy);
    const int i1 = std::min(i0 + 1, blocksX - 1), j1 = std::min(j0 + 1, blocksY - 1);
    const float fx = gx - float(i0), fy = gy - float(j0);
    auto at = [&](const std::vector<float>& f, int i, int j) { return f[size_t(j) * size_t(blocksX) + size_t(i)]; };
    auto lerp2 = [&](const std::vector<float>& f) {
        const float top = at(f, i0, j0) + (at(f, i1, j0) - at(f, i0, j0)) * fx;
        const float bottom = at(f, i0, j1) + (at(f, i1, j1) - at(f, i0, j1)) * fx;
        return top + (bottom - top) * fy;
    };
    outX = lerp2(vx);
    outY = lerp2(vy);
}

double meanAbsDiff(const Gray& a, const Gray& b) {
    if (a.width != b.width || a.height != b.height || a.empty()) return -1;
    uint64_t sum = 0;
    for (size_t i = 0; i < a.px.size(); ++i) sum += uint64_t(iabs(int(a.px[i]) - int(b.px[i])));
    return double(sum) / double(a.px.size());
}

MotionField MotionEstimator::estimate(const Pyramid& a, const Pyramid& b, const MotionField* previous) const {
    MotionField out;
    const int count = std::min({s_.levels, int(a.levels.size()), int(b.levels.size())});
    if (count <= 0) return out;
    for (int l = 0; l < count; ++l)
        if (a.levels[l].width != b.levels[l].width || a.levels[l].height != b.levels[l].height || a.levels[l].empty()) return out;
    const int coarsest = count - 1;
    const int finest = std::clamp(s_.finestLevel, 0, coarsest);
    const int range = std::clamp(s_.searchRange, 1, 32);
    const float lambda = std::max(0.0f, s_.smoothness);
    const bool usePrevious = previous && !previous->empty() && previous->width == a.levels[0].width && previous->height == a.levels[0].height;

    Grid parent;
    std::vector<float> subX, subY;  // sub-pixel offsets at the finest level
    for (int level = coarsest; level >= finest; --level) {
        const Gray& A = a.levels[level];
        const Gray& B = b.levels[level];
        const float scale = float(1 << level);
        Grid g;
        g.bx = (A.width + kBlock - 1) / kBlock;
        g.by = (A.height + kBlock - 1) / kBlock;
        g.v.assign(size_t(g.bx) * size_t(g.by), {});
        g.cost.assign(g.v.size(), 0);
        g.zero.assign(g.v.size(), 0);
        const bool last = level == finest;
        if (last) { subX.assign(g.v.size(), 0.f); subY.assign(g.v.size(), 0.f); }
        // Search positions stay within the picture plus the search range, so a wild predictor can't
        // send the search far outside the frame.
        const int limX = A.width / 2 + range * 2, limY = A.height / 2 + range * 2;

        for (int j = 0; j < g.by; ++j) {
            for (int i = 0; i < g.bx; ++i) {
                const int x0 = i * kBlock, y0 = j * kBlock;
                const size_t idx = size_t(j) * size_t(g.bx) + size_t(i);
                // Smoothness reference: median of the left, upper and upper-right blocks (already done)
                // and, below the coarsest level, the parent.
                IV left = i > 0 ? g.v[idx - 1] : IV{}, up = j > 0 ? g.v[idx - size_t(g.bx)] : IV{};
                IV upRight = (j > 0 && i + 1 < g.bx) ? g.v[idx - size_t(g.bx) + 1] : up;
                IV par{};
                if (level < coarsest) { const IV p = parent.get(i / 2, j / 2); par = {p.x * 2, p.y * 2}; }
                IV pred;
                if (level < coarsest) {
                    pred = {median3(left.x, up.x, par.x), median3(left.y, up.y, par.y)};
                } else {
                    pred = {median3(left.x, up.x, upRight.x), median3(left.y, up.y, upRight.y)};
                }
                IV best{};
                int bestRaw = 0;
                float bestCost = 1e30f;
                auto consider = [&](IV c) {
                    c.x = std::clamp(c.x, -limX, limX);
                    c.y = std::clamp(c.y, -limY, limY);
                    const int raw = sad(A, B, x0, y0, c.x, c.y);
                    const float cost = float(raw) + lambda * float(iabs(c.x - pred.x) + iabs(c.y - pred.y));
                    if (cost < bestCost) { bestCost = cost; best = c; bestRaw = raw; }
                };
                consider({0, 0});
                consider(pred);
                if (usePrevious) {
                    float px = 0, py = 0;
                    previous->sample((float(x0) + kBlock * 0.5f) * scale, (float(y0) + kBlock * 0.5f) * scale, px, py);
                    consider({int(std::lround(px / scale)), int(std::lround(py / scale))});
                }
                if (level == coarsest) {
                    for (int dy = -range; dy <= range; ++dy)
                        for (int dx = -range; dx <= range; ++dx) consider({dx, dy});
                } else {
                    consider(par);
                    const IV pn1 = parent.get(i / 2 + ((i & 1) ? 1 : -1), j / 2);
                    const IV pn2 = parent.get(i / 2, j / 2 + ((j & 1) ? 1 : -1));
                    consider({pn1.x * 2, pn1.y * 2});
                    consider({pn2.x * 2, pn2.y * 2});
                    consider(left);
                    consider(up);
                }
                // +-1 refinement, repeated while it improves (at most 3 steps).
                for (int step = 0; step < 3; ++step) {
                    const IV centre = best;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (dx || dy) consider({centre.x + dx, centre.y + dy});
                    if (best.x == centre.x && best.y == centre.y) break;
                }
                g.v[idx] = best;
                g.cost[idx] = bestRaw;
                if (last) g.zero[idx] = sad(A, B, x0, y0, 0, 0);
                if (last) {
                    // Parabola through the raw costs left/right and above/below the best position.
                    auto fit = [&](int cm, int c0, int cp) {
                        const int den = cm - 2 * c0 + cp;
                        if (den <= 0) return 0.0f;
                        return std::clamp(float(cm - cp) / float(2 * den), -0.5f, 0.5f);
                    };
                    subX[idx] = fit(sad(A, B, x0, y0, best.x - 1, best.y), bestRaw, sad(A, B, x0, y0, best.x + 1, best.y));
                    subY[idx] = fit(sad(A, B, x0, y0, best.x, best.y - 1), bestRaw, sad(A, B, x0, y0, best.x, best.y + 1));
                }
            }
        }
        if (!last) { parent = std::move(g); continue; }

        // Finest level: vectors to full-size pixels, 3x3 median, costs per pixel.
        out.blocksX = g.bx;
        out.blocksY = g.by;
        out.blockSize = kBlock << level;
        out.width = a.levels[0].width;
        out.height = a.levels[0].height;
        const size_t n = g.v.size();
        std::vector<float> rx(n), ry(n);
        for (size_t k = 0; k < n; ++k) { rx[k] = (float(g.v[k].x) + subX[k]) * scale; ry[k] = (float(g.v[k].y) + subY[k]) * scale; }
        out.vx.resize(n);
        out.vy.resize(n);
        out.cost.resize(n);
        float window[9];
        for (int j = 0; j < g.by; ++j) {
            for (int i = 0; i < g.bx; ++i) {
                const size_t idx = size_t(j) * size_t(g.bx) + size_t(i);
                for (int comp = 0; comp < 2; ++comp) {
                    const std::vector<float>& src = comp ? ry : rx;
                    int m = 0;
                    for (int dj = -1; dj <= 1; ++dj)
                        for (int di = -1; di <= 1; ++di) {
                            const int ii = std::clamp(i + di, 0, g.bx - 1), jj = std::clamp(j + dj, 0, g.by - 1);
                            window[m++] = src[size_t(jj) * size_t(g.bx) + size_t(ii)];
                        }
                    std::nth_element(window, window + 4, window + 9);
                    (comp ? out.vy : out.vx)[idx] = window[4];
                }
                out.cost[idx] = float(g.cost[idx]) / float(kBlock * kBlock);
            }
        }
        // Scene cut: most blocks changed (zero-motion error above the floor) and motion explains little
        // of the change (the matched error stays above sceneCutRatio of it). A pan or a moving object is
        // explained; a new scene, a flash or a fade to another picture is not.
        double sum = 0;
        size_t unexplained = 0;
        for (size_t k = 0; k < n; ++k) {
            sum += out.cost[k];
            const float z = float(g.zero[k]) / float(kBlock * kBlock);
            if (out.cost[k] > s_.sceneCutFloor && out.cost[k] > s_.sceneCutRatio * z) ++unexplained;
        }
        out.meanCost = float(sum / double(n));
        out.zeroCost = float(meanAbsDiff(A, B));
        out.sceneCut = double(unexplained) > double(n) * double(s_.sceneCutShare);
    }
    return out;
}

}  // namespace veyra::portable
