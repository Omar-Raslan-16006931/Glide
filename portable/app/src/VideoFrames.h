// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Qt video frames (capture card, video file) into the core: luma first, colour only for real frames.
#include "veyra/portable/Engine.h"

#include <QImage>
#include <QVideoFrame>

namespace pushfps {

// Pushes one frame into the engine. Returns false if the frame could not be read.
bool pushVideoFrame(veyra::portable::Engine& engine, const QVideoFrame& frame, qint64 arrivalNs);

// BGRA picture from a QImage (test pattern, screenshots in tests).
veyra::portable::Bgra bgraFromImage(const QImage& image);
QImage imageFromBgra(const veyra::portable::Bgra& image);

// Steady clock in nanoseconds: the time base shared by capture and display.
qint64 nowNs();

}  // namespace pushfps
