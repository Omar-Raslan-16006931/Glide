// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: seg
    property var options: []
    property int currentIndex: 0
    signal picked(int index)
    implicitHeight: 30
    implicitWidth: rowItem.implicitWidth + 6
    radius: 8
    color: Theme.card3
    RowLayout {
        id: rowItem
        anchors.fill: parent
        anchors.margins: 3
        spacing: 2
        Repeater {
            model: seg.options
            Rectangle {
                required property int index
                required property var modelData
                Layout.fillHeight: true
                Layout.fillWidth: true
                implicitWidth: label.implicitWidth + 22
                radius: 6
                color: seg.currentIndex === index ? Theme.accent : (area.containsMouse ? Theme.card2 : "transparent")
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: parent.modelData
                    color: seg.currentIndex === parent.index ? Theme.accentInk : Theme.t2
                    font.pixelSize: Theme.fsBody - 1
                    font.family: Theme.fontUi
                    font.bold: seg.currentIndex === parent.index
                }
                MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: seg.picked(parent.index) }
            }
        }
    }
}
