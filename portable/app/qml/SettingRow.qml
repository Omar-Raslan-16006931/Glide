// SPDX-License-Identifier: GPL-3.0-or-later
// A label (with an optional hint under it) and its control on the right; the control moves under the
// label when the row is too narrow.
import QtQuick
import QtQuick.Layouts

Item {
    id: row
    default property alias control: slot.data
    property string label: ""
    property string hint: ""
    property bool wide: false          // the control takes the full width under the label
    Layout.fillWidth: true
    implicitHeight: grid.implicitHeight
    GridLayout {
        id: grid
        width: row.width
        columns: row.wide || row.width < 520 ? 1 : 2
        columnSpacing: 16
        rowSpacing: 8
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text { text: row.label; color: Theme.t1; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { visible: row.hint.length > 0; text: row.hint; color: Theme.t3; font.pixelSize: Theme.fsHint; font.family: Theme.fontUi; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        Item {
            id: slot
            Layout.alignment: grid.columns === 1 ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignVCenter)
            Layout.fillWidth: grid.columns === 1
            implicitWidth: children.length > 0 ? children[0].implicitWidth : 0
            implicitHeight: children.length > 0 ? children[0].implicitHeight : 0
        }
    }
}
