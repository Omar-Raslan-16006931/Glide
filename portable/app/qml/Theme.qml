// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide colours (the Windows app's dark default, qml/Veyra/Theme.qml) for the Linux / macOS build.
pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0C0C10"
    readonly property color bgDeep: "#070709"
    readonly property color card: "#131317"
    readonly property color card2: "#19191E"
    readonly property color card3: "#24242A"
    readonly property color stroke: Qt.rgba(1, 1, 1, 0.07)
    readonly property color stroke2: Qt.rgba(1, 1, 1, 0.13)
    readonly property color t1: "#F3F3F5"
    readonly property color t2: Qt.rgba(0.953, 0.953, 0.961, 0.62)
    readonly property color t3: Qt.rgba(0.953, 0.953, 0.961, 0.48)
    readonly property color accent: "#89F52A"
    readonly property color accentInk: "#0D1A04"
    readonly property color accentSoft: Qt.rgba(0.537, 0.961, 0.165, 0.14)
    readonly property color err: "#FF5D5D"
    readonly property color errSoft: Qt.rgba(1, 0.365, 0.365, 0.14)
    readonly property color warn: "#F5C84B"
    // Bahnschrift is the Windows wordmark face; the closest wide sans on each system follows.
    readonly property string fontBrand: Qt.platform.os === "osx" ? "Avenir Next" : "Bahnschrift"
    // The system's own interface font (San Francisco on macOS, the desktop's choice on Linux).
    readonly property string fontUi: Qt.application.font.family
    readonly property real fsBody: 13
    readonly property real fsHint: 11.5
    readonly property real fsTitle: 15
    readonly property real radius: 12
}
