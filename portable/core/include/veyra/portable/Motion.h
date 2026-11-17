// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Block motion estimation for the Linux / macOS frame generation (Glide's own, no vendor SDK).
//
// Hierarchical block matching on a luma pyramid: an exhaustive search on the smallest level finds
// large motion, every finer level refines the parent's vector with a few candidates (parent and its
// neighbours, the left and upper block, the same block in the previous field, zero) and a +-1 search,
// and the finest level adds a sub-pixel fit. A small smoothness penalty keeps flat areas (sky, walls)
// from picking random vectors. A 3x3 vector median removes single wrong blocks.
//
// The field is low resolution (one vector per 16x16 pixels at the default settings); the GPU shader
// and Interpolate.h sample it bilinearly.
#include "veyra/portable/Image.h"

#include <cstdint>
#include <vector>

namespace veyra::portable {

struct MotionField {
    int blocksX = 0, blocksY = 0;
    int blockSize = 16;          // in full-size pixels
    int width = 0, height = 0;   // full-size picture the field belongs to
    std::vector<float> vx, vy;   // motion in full-size pixels: a point at p in frame A is at p + v in frame B
    std::vector<float> cost;     // mean absolute luma error per pixel of each block after matching (0..255)
    float meanCost = 0;          // over all blocks
    float zeroCost = 0;          // the same without motion (how much the picture changed at all)
    bool sceneCut = false;       // the two frames show different scenes: don't interpolate between them

    bool empty() const { return blocksX <= 0 || blocksY <= 0; }
    // Bilinear between block centres, clamped at the edges; x, y in full-size pixels.
    void sample(float x, float y, float& outX, float& outY) const;
};

struct MotionSettings {
    int levels = 4;             // pyramid levels used (full, half, quarter, eighth)
    int finestLevel = 1;        // 1 = vectors from the half-size picture (16 px blocks at full size)
    int searchRange = 8;        // exhaustive search on the smallest level, in its pixels (+-64 px at 1080p)
    float smoothness = 3.0f;    // SAD units per pixel of distance from the neighbours' median vector
    float sceneCutFloor = 5.0f;  // a block's matched error (0..255 per pixel) must be above this ...
    float sceneCutRatio = 0.6f;  // ... and above this share of its error without motion to count as unexplained
    float sceneCutShare = 0.5f;  // share of unexplained blocks that makes a scene cut
};

class MotionEstimator {
public:
    explicit MotionEstimator(MotionSettings settings = {}) : s_(settings) {}
    const MotionSettings& settings() const { return s_; }
    // a, b: pyramids of the two frames (same size). previous: the last field for the same direction
    // (temporal predictor), may be null or of another size.
    MotionField estimate(const Pyramid& a, const Pyramid& b, const MotionField* previous = nullptr) const;

private:
    MotionSettings s_;
};

// Mean absolute difference between two same-size luma images (0..255); -1 if sizes differ.
double meanAbsDiff(const Gray& a, const Gray& b);

}  // namespace veyra::portable
