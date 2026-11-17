// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The Glide wordmark as in the logo: a bold, slanted, slightly condensed
// face, "Push" in white (dark text in the light theme) and "FPS" in the logo's
// green. The name comes from the bridge (Brand.h), split before its last three
// letters. Bahnschrift ships with Windows 10 and 11; elsewhere the UI font is used.
import QtQuick

Text {
    id: mark
    objectName: "pushfps-wordmark"
    property string name: (typeof veyra !== "undefined" && veyra) ? veyra.appName : "Glide"
    property real pixelSize: 30
    readonly property bool split: name.length > 3 && name.slice(-3) === "FPS"
    readonly property string head: split ? name.slice(0, -3) : name
    readonly property string tail: split ? "FPS" : ""
    readonly property string family: Qt.fontFamilies().indexOf("Bahnschrift") >= 0 ? "Bahnschrift" : Theme.fontUi
    readonly property color headColor: Theme.light ? Theme.t1 : "#FFFFFF"
    readonly property color tailColor: Theme.brandGreen
    // One run of text, so the slanted "h" and "F" keep normal spacing between them.
    textFormat: Text.StyledText
    text: tail.length > 0 ? head + "<font color=\"" + tailColor + "\">" + tail + "</font>" : head
    color: headColor
    font.family: family
    font.pixelSize: pixelSize
    font.weight: Font.Bold
    font.italic: true
    font.letterSpacing: -0.02 * pixelSize
    Accessible.role: Accessible.Heading
    Accessible.name: name
}
