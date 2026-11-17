// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Duplicate removal: a 30 FPS game on a 60 Hz HDMI signal sends every picture twice (or 1/2/1/2 times
// when its pacing is uneven). Frame generation must see the real pictures only. A frame is a
// duplicate when no 16x16 area of it changed (compared on the quarter-size luma), so a small moving
// object or a blinking cursor still counts as new.
#include "veyra/portable/Image.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace veyra::portable {

struct DedupSettings {
    float blockThreshold = 3.0f;  // mean abs luma difference of a 4x4 block (quarter size) that counts as change
    float meanThreshold = 0.8f;   // mean over the picture that counts as change (slow fades)
};

struct DedupResult {
    bool duplicate = false;
    float maxBlock = 0;   // largest block difference
    float mean = 0;       // mean difference
};

// Compares two same-size quarter-size luma images. Different sizes are never duplicates.
inline DedupResult compareForDuplicate(const Gray& previous, const Gray& current, const DedupSettings& s = {}) {
    DedupResult r;
    if (previous.empty() || previous.width != current.width || previous.height != current.height) return r;
    const int bw = (current.width + 3) / 4, bh = (current.height + 3) / 4;
    double total = 0;
    float worst = 0;
    for (int by = 0; by < bh; ++by) {
        for (int bx = 0; bx < bw; ++bx) {
            int sum = 0, count = 0;
            for (int y = by * 4; y < std::min(by * 4 + 4, current.height); ++y)
                for (int x = bx * 4; x < std::min(bx * 4 + 4, current.width); ++x) {
                    sum += std::abs(int(previous.at(x, y)) - int(current.at(x, y)));
                    ++count;
                }
            total += sum;
            worst = std::max(worst, float(sum) / float(count));
        }
    }
    r.maxBlock = worst;
    r.mean = float(total / double(current.px.size()));
    r.duplicate = worst < s.blockThreshold && r.mean < s.meanThreshold;
    return r;
}

}  // namespace veyra::portable
