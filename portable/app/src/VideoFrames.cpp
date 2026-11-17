// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "VideoFrames.h"

#include "veyra/portable/Convert.h"

#include <QVideoFrameFormat>

#include <algorithm>
#include <chrono>
#include <cstring>

using namespace veyra::portable;

namespace pushfps {

qint64 nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Bgra bgraFromImage(const QImage& image) {
    const QImage rgb = image.format() == QImage::Format_RGB32 || image.format() == QImage::Format_ARGB32
                           ? image
                           : image.convertToFormat(QImage::Format_RGB32);
    Bgra out(rgb.width(), rgb.height());
    // Format_RGB32 is 0xffRRGGBB per pixel: B, G, R, A in memory on little-endian machines (every
    // platform this build runs on: x86-64 and Apple Silicon / ARM64).
    for (int y = 0; y < rgb.height(); ++y) {
        const uchar* src = rgb.constScanLine(y);
        uint8_t* dst = out.pixel(0, y);
        std::memcpy(dst, src, size_t(rgb.width()) * 4);
        for (int x = 0; x < rgb.width(); ++x) dst[x * 4 + 3] = 255;
    }
    return out;
}

QImage imageFromBgra(const Bgra& image) {
    QImage out(image.width, image.height, QImage::Format_RGB32);
    for (int y = 0; y < image.height; ++y) std::memcpy(out.scanLine(y), image.pixel(0, y), size_t(image.width) * 4);
    return out;
}

namespace {

bool describe(const QVideoFrame& frame, SourceFrame& f) {
    switch (frame.pixelFormat()) {
    case QVideoFrameFormat::Format_NV12: f.format = PixelFormat::Nv12; break;
    case QVideoFrameFormat::Format_YUYV: f.format = PixelFormat::Yuyv; break;
    case QVideoFrameFormat::Format_UYVY: f.format = PixelFormat::Uyvy; break;
    case QVideoFrameFormat::Format_YUV420P: f.format = PixelFormat::I420; break;
    case QVideoFrameFormat::Format_P010: f.format = PixelFormat::P010; break;
    case QVideoFrameFormat::Format_BGRA8888:
    case QVideoFrameFormat::Format_BGRX8888: f.format = PixelFormat::Bgra; break;
    case QVideoFrameFormat::Format_RGBA8888:
    case QVideoFrameFormat::Format_RGBX8888: f.format = PixelFormat::Rgba; break;
    default: return false;  // everything else goes through QVideoFrame::toImage()
    }
    f.width = frame.width();
    f.height = frame.height();
    const QVideoFrameFormat format = frame.surfaceFormat();
    f.matrix = format.colorSpace() == QVideoFrameFormat::ColorSpace_BT601 ? Matrix::Bt601 : Matrix::Bt709;
    f.fullRange = format.colorRange() == QVideoFrameFormat::ColorRange_Full;
    const int planes = std::min(frame.planeCount(), 3);
    for (int i = 0; i < planes; ++i) f.planes[i] = {frame.bits(i), frame.bytesPerLine(i)};
    return true;
}

}  // namespace

bool pushVideoFrame(Engine& engine, const QVideoFrame& input, qint64 arrivalNs) {
    if (!input.isValid()) return false;
    QVideoFrame frame(input);  // shallow copy: map() needs a non-const frame
    if (!frame.map(QVideoFrame::ReadOnly)) return false;
    SourceFrame f;
    bool ok = false;
    if (describe(frame, f)) {
        Gray luma = lumaFromSource(f);
        if (!luma.empty()) {
            engine.push(std::move(luma), [&] {
                Bgra out;
                if (!convertToBgra(f, out)) return Bgra();
                return out;
            }, arrivalNs);
            ok = true;
        }
    }
    frame.unmap();
    if (ok) return true;
    // Compressed (MJPEG) or unusual formats: Qt converts them.
    const QImage image = input.toImage();
    if (image.isNull()) return false;
    engine.push(bgraFromImage(image), arrivalNs);
    return true;
}

}  // namespace pushfps
