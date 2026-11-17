// SPDX-License-Identifier: GPL-3.0-or-later
// GPU check scene (Glide --gpucheck): one FrameView filling the window, neutral picture settings.
import QtQuick
import Glide

Rectangle {
    color: "black"
    FrameView {
        anchors.fill: parent
    }
}
