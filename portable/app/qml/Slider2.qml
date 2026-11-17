// SPDX-License-Identifier: GPL-3.0-or-later
// A slider with its value and a reset mark.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: s
    property real from: 0
    property real to: 1
    property real value: 0
    property real neutral: 0
    property int decimals: 2
    signal moved(real v)
    spacing: 8
    Slider {
        id: slider
        Layout.preferredWidth: 180
        from: s.from; to: s.to; value: s.value
        onMoved: s.moved(value)
        background: Rectangle {
            x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth; height: 4; radius: 2; color: Theme.card3
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16; height: 16; radius: 8; color: Theme.t1; border.color: Theme.accent
        }
    }
    Text { text: s.value.toFixed(s.decimals); color: Theme.t2; font.pixelSize: Theme.fsHint; font.family: Theme.fontUi; Layout.preferredWidth: 36 }
    Text {
        text: "↺"; color: Math.abs(s.value - s.neutral) > 0.001 ? Theme.t1 : Theme.t3; font.pixelSize: 15
        MouseArea { anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.PointingHandCursor; onClicked: s.moved(s.neutral) }
    }
}
