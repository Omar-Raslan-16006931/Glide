// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Settings storage. Portable mode: a file named portable.txt next to the program (or next to
// Glide.app on macOS) keeps the settings in Glide.ini beside it, so the folder can live on a USB
// stick and deleting it removes everything.
#include <QSettings>
#include <QString>

#include <memory>

namespace pushfps {

struct Storage {
    static bool portable();
    static QString portableFolder();   // empty when not portable
    static std::unique_ptr<QSettings> open();
    static QString screenshotFolder();
};

}  // namespace pushfps
