#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide automatic HUD detection: finds the parts of the picture that keep
// their detail while the rest of the game moves (minimap, health, ammo,
// prompts) so frame generation leaves them alone (the "Exclude all effects"
// regions, at most four).
//
// Input: a few real (not generated) presented frames a few hundred
// milliseconds apart, each reduced to a small luma grid (reduce()). A grid
// cell is HUD when its luma stays put (temporal deviation near capture noise
// and well below the picture's typical motion) while it has visible detail
// (edges); flat static areas such as letterbox bars or sky are not HUD.
// Neighbouring HUD cells grow into boxes, small or sparse ones are dropped,
// close ones merge, and the largest four are kept with a small margin.
//
// When the whole picture hardly moved (a menu, a paused game, a still camera)
// nothing tells the HUD apart from the scene: detect() reports tooStill and no
// regions, and the interface asks the player to move the camera while it
// watches. The player can always correct the boxes by hand.
//
// Pure CPU code with no Windows headers; tests/unit/PushHudTests.cpp.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace veyra::engine::hud {

constexpr uint32_t kGridWidth = 384, kGridHeight = 216;       // luma grid of one frame
constexpr uint32_t kCellsX = kGridWidth / 2, kCellsY = kGridHeight / 2; // decision cells (2x2 grid points)
constexpr uint32_t kPreviewWidth = 640, kPreviewHeight = 360;  // RGB picture for the box editor
constexpr size_t kMinFrames = 6;
constexpr size_t kMaxRegions = 4;                               // Exclusion node limit

enum class PixelLayout { Rgba8, Bgra8, Rgb10a2, Rgba16f };

struct LumaFrame {
    std::vector<uint8_t> luma; // kGridWidth * kGridHeight, row major
};

struct Region {
    float left = 0, top = 0, right = 0, bottom = 0; // 0..1 of the picture
};

struct Detection {
    std::vector<Region> regions; // largest first, at most kMaxRegions
    bool tooStill = false;       // the picture hardly moved: no regions can be told apart
    float motion = 0;            // median cell deviation over time, 8-bit levels
    uint32_t candidateCells = 0; // static detailed cells before grouping
};

// Reduces one frame to the luma grid (3x3 samples per grid point, BT.709
// weights; FP16 scRGB is clamped to 0..1 and given a 1/2 gamma; 10-bit keeps
// its top 8 bits). With `previewRgb`, also fills kPreviewWidth x kPreviewHeight
// RGB bytes. Returns false for an empty or too small frame.
bool reduce(const uint8_t* data, size_t pitch, uint32_t width, uint32_t height, PixelLayout layout,
            LumaFrame& out, std::vector<uint8_t>* previewRgb = nullptr);

// Needs at least kMinFrames frames of the grid size; fewer give no regions.
Detection detect(const std::vector<LumaFrame>& frames, size_t maxRegions = kMaxRegions);

} // namespace veyra::engine::hud
