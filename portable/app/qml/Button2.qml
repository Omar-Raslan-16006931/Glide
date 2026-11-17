// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: b
    property string text: ""
    property bool primary: false
    property bool enabled2: true
    signal clicked()
    implicitWidth: label.implicitWidth + 32
    implicitHeight: 34
    radius: 9
    opacity: enabled2 ? 1 : 0.45
    color: primary ? (area.containsMouse ? Qt.lighter(Theme.accent, 1.08) : Theme.accent) : (area.containsMouse ? Theme.card3 : Theme.card)
    border.color: primary ? "transparent" : (b.activeFocus ? Theme.accent : Theme.stroke2)
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Text {
        id: label
        anchors.centerIn: parent
        text: b.text
        color: b.primary ? Theme.accentInk : Theme.t1
        font.pixelSize: Theme.fsBody
        font.bold: b.primary
        font.family: Theme.fontUi
    }
    MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; enabled: b.enabled2; cursorShape: Qt.PointingHandCursor; onClicked: b.clicked() }
    Keys.onReturnPressed: if (enabled2) clicked()
    Keys.onSpacePressed: if (enabled2) clicked()
}
