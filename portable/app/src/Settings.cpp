// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace pushfps {

QString Storage::portableFolder() {
    const QString exeDir = QCoreApplication::applicationDirPath();
    QStringList candidates{exeDir};
#ifdef Q_OS_MACOS
    // Glide.app/Contents/MacOS -> the folder holding Glide.app
    candidates << QDir(exeDir + QStringLiteral("/../../..")).absolutePath();
#endif
    // The Linux folder keeps the program in usr/bin: portable.txt sits two folders up, next to AppRun.
    candidates << QDir(exeDir + QStringLiteral("/..")).absolutePath() << QDir(exeDir + QStringLiteral("/../..")).absolutePath();
    for (const QString& dir : candidates)
        if (QFileInfo::exists(dir + QStringLiteral("/portable.txt"))) return dir;
    return {};
}

bool Storage::portable() { return !portableFolder().isEmpty(); }

std::unique_ptr<QSettings> Storage::open() {
    const QString folder = portableFolder();
    if (!folder.isEmpty()) return std::make_unique<QSettings>(folder + QStringLiteral("/Glide.ini"), QSettings::IniFormat);
    return std::make_unique<QSettings>(QStringLiteral("Glide"), QStringLiteral("Glide"));
}

QString Storage::screenshotFolder() {
    const QString folder = portableFolder();
    QString dir = folder.isEmpty()
                      ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Glide")
                      : folder + QStringLiteral("/Screenshots");
    QDir().mkpath(dir);
    return dir;
}

}  // namespace pushfps
