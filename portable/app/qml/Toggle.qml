// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Item {
    id: t
    property bool checked: false
    property bool enabled2: true
    signal toggled(bool on)
    implicitWidth: 44
    implicitHeight: 24
    opacity: enabled2 ? 1 : 0.4
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked
    activeFocusOnTab: true
    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: t.checked ? Theme.accent : Theme.card3
        border.color: t.activeFocus ? Theme.accent : Theme.stroke2
        Behavior on color { ColorAnimation { duration: 120 } }
        Rectangle {
            width: 18; height: 18; radius: 9
            y: 3
            x: t.checked ? parent.width - width - 3 : 3
            color: t.checked ? Theme.accentInk : Theme.t1
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
    MouseArea { anchors.fill: parent; enabled: t.enabled2; cursorShape: Qt.PointingHandCursor; onClicked: t.toggled(!t.checked) }
    Keys.onSpacePressed: if (enabled2) toggled(!checked)
}
