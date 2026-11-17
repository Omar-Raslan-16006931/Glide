// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    default property alias content: inner.data
    property string title: ""
    property string subtitle: ""
    property real padding: 16
    color: Theme.card2
    radius: Theme.radius
    border.color: Theme.stroke
    border.width: 1
    implicitHeight: inner.implicitHeight + padding * 2 + (header.visible ? header.implicitHeight + 10 : 0)
    ColumnLayout {
        id: header
        visible: card.title.length > 0
        x: card.padding
        y: card.padding
        width: card.width - card.padding * 2
        spacing: 2
        Text { text: card.title; color: Theme.t1; font.pixelSize: Theme.fsTitle; font.bold: true; font.family: Theme.fontUi }
        Text { visible: card.subtitle.length > 0; text: card.subtitle; color: Theme.t3; font.pixelSize: Theme.fsHint; font.family: Theme.fontUi; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    ColumnLayout {
        id: inner
        x: card.padding
        y: card.padding + (header.visible ? header.implicitHeight + 10 : 0)
        width: card.width - card.padding * 2
        spacing: 12
    }
}
