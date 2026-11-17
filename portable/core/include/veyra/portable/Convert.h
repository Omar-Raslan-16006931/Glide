// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Capture-card pixel formats to BGRA. Capture cards deliver NV12, YUYV (YUY2) or UYVY over USB (UVC);
// files and some cards give I420 or packed RGB. Planes come in with their own strides, as the
// operating system maps them.
#include "veyra/portable/Image.h"

namespace veyra::portable {

enum class PixelFormat { Nv12, Yuyv, Uyvy, I420, Bgra, Rgba, P010 };
enum class Matrix { Bt709, Bt601 };

struct PlaneView {
    const uint8_t* data = nullptr;
    int stride = 0;  // bytes per row
};

struct SourceFrame {
    PixelFormat format = PixelFormat::Bgra;
    int width = 0, height = 0;
    PlaneView planes[3];
    Matrix matrix = Matrix::Bt709;
    bool fullRange = false;  // HDMI video is limited range (16-235) unless the card says otherwise
};

// Returns false (and leaves out unchanged) for a frame it can't read: unknown format, zero size,
// missing plane or a stride smaller than the row.
bool convertToBgra(const SourceFrame& in, Bgra& out);

// Full-range luma straight from the frame (the Y plane of YUV formats, no colour conversion): enough
// for duplicate removal, motion and HUD lock, and much cheaper than a full conversion. Empty on error.
Gray lumaFromSource(const SourceFrame& in);

}  // namespace veyra::portable
