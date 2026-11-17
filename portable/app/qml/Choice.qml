// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic

ComboBox {
    id: box
    implicitWidth: 300
    implicitHeight: 32
    font.pixelSize: Theme.fsBody
    font.family: Theme.fontUi
    background: Rectangle { radius: 8; color: box.hovered ? Theme.card3 : Theme.card; border.color: box.activeFocus ? Theme.accent : Theme.stroke2 }
    contentItem: Text {
        leftPadding: 10
        rightPadding: 28
        text: box.displayText
        color: Theme.t1
        font: box.font
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Text { x: box.width - width - 10; anchors.verticalCenter: parent.verticalCenter; text: "▾"; color: Theme.t2; font.pixelSize: 12 }
    delegate: ItemDelegate {
        id: entry
        required property int index
        required property var modelData
        width: box.width
        highlighted: box.highlightedIndex === index
        contentItem: Text { text: entry.modelData; color: Theme.t1; font: box.font; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: entry.highlighted ? Theme.card3 : Theme.card }
    }
    popup.background: Rectangle { color: Theme.card; radius: 8; border.color: Theme.stroke2 }
}
