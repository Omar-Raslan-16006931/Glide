// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
#include "veyra/source/CaptureFormatSelection.h"
#include <cmath>
#include <span>
#include <tuple>

namespace veyra::source {
// Glide plug and play: the format a capture card connects with when nobody picked one.
// A console sends a 60 Hz signal (a 30 FPS game repeats each picture), so the best start is:
//   1. a mode that follows the real signal, when the card offers one (it cannot be wrong);
//   2. 60 fps (then 120, 50, 30), progressive, never a compressed (MJPEG) mode if anything else exists;
//   3. 1080p (light on the graphics card, every card does it), then 1440p, 2160p, 720p;
//   4. the cheapest pixel format (NV12 < P010 < YUY2 < RGB < packed < compressed);
//   5. the driver's current mode, then list order.
// Unsupported modes are never picked. Returns null for an empty or fully unsupported list.
inline int pushAutoFpsClass(double fps) {
    auto near = [fps](double rate) { return std::abs(fps - rate) <= rate * 0.0015; };
    if (near(60)) return 0;
    if (near(120)) return 1;
    if (near(50)) return 2;
    if (near(30)) return 3;
    return 4;
}
inline int pushAutoHeightClass(unsigned height) {
    switch (height) {
    case 1080: return 0;
    case 1440: return 1;
    case 2160: return 2;
    case 720: return 3;
    default: return 4;
    }
}
inline const CaptureFormat* pushAutoCaptureFormat(std::span<const CaptureFormat> formats) {
    const CaptureFormat* best = nullptr;
    auto score = [](const CaptureFormat& f) {
        return std::make_tuple(f.automatic ? 0 : 1, captureMjpegFormat(f) || f.tier >= 3 ? 1 : 0, f.interlaced ? 1 : 0,
                               pushAutoFpsClass(f.fps), pushAutoHeightClass(f.height), f.tier, f.rank,
                               f.rangeCandidate ? 1 : 0, f.driverCurrent ? 0 : 1);
    };
    for (const auto& f : formats) {
        if (!f.supported || f.key.empty()) continue;
        if (!best || score(f) < score(*best)) best = &f;
    }
    return best;
}
}
