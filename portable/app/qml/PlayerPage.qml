// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
import QtQuick
import QtQuick.Layouts
import Glide

Rectangle {
    id: player
    property bool active: false
    property bool fullscreen: false
    signal back()
    signal toggleFullscreen()
    color: "black"

    FrameView {
        id: view
        objectName: "player-view"
        anchors.fill: parent
        running: player.active
        brightness: app.brightness
        contrast: app.contrast
        saturation: app.saturation
        gamma: app.gamma
    }

    readonly property var st: app.stats
    readonly property bool waiting: !st.framesRead || st.framesRead === 0

    Text {
        anchors.centerIn: parent
        visible: player.waiting && app.running
        text: qsTr("正在等待画面…")
        color: Theme.t2; font.pixelSize: 16; font.family: Theme.fontUi
    }

    // Statistics (Tab).
    Rectangle {
        visible: app.showStats && !player.waiting
        x: 16; y: 16
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.62)
        implicitWidth: stats.implicitWidth + 20
        implicitHeight: stats.implicitHeight + 14
        Column {
            id: stats
            x: 10; y: 7
            spacing: 2
            Text {
                color: "white"; font.pixelSize: 13; font.family: Theme.fontUi; font.bold: true
                text: qsTr("游戏 %1 FPS → 输出 %2 FPS").arg(Math.round(player.st.contentFps || 0)).arg(Math.round(player.st.outputFps || 0))
            }
            Text {
                color: Qt.rgba(1, 1, 1, 0.75); font.pixelSize: 12; font.family: Theme.fontUi
                text: qsTr("信号 %1 Hz · 延迟 %2 ms · 运动估计 %3 ms").arg(Math.round(player.st.transportFps || 0))
                          .arg(Math.round(player.st.latencyMs || 0)).arg((player.st.estimateMs || 0).toFixed(1))
            }
            Text {
                visible: app.pacingRepair
                color: Qt.rgba(1, 1, 1, 0.75); font.pixelSize: 12; font.family: Theme.fontUi
                text: qsTr("正在修复：%1% 的画面重新对齐").arg(Math.round((player.st.pacingShare || 0) * 100))
            }
        }
    }

    // Controls: show on mouse movement, hide after 2.5 s.
    property bool controlsShown: true
    Timer { id: hide; interval: 2500; onTriggered: player.controlsShown = false }
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onPositionChanged: { player.controlsShown = true; hide.restart() }
        onDoubleClicked: player.toggleFullscreen()
        cursorShape: player.controlsShown ? Qt.ArrowCursor : Qt.BlankCursor
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: player.controlsShown ? 14 : -height - 10
        Behavior on y { NumberAnimation { duration: 160 } }
        radius: 12
        color: Qt.rgba(0.07, 0.07, 0.09, 0.88)
        border.color: Theme.stroke2
        implicitWidth: bar.implicitWidth + 16
        implicitHeight: bar.implicitHeight + 12
        RowLayout {
            id: bar
            x: 8; y: 6
            spacing: 6
            Button2 { text: qsTr("断开"); onClicked: player.back() }
            Button2 { text: player.fullscreen ? qsTr("退出全屏") : qsTr("全屏"); onClicked: player.toggleFullscreen() }
            Button2 {
                text: qsTr("截图")
                onClicked: {
                    const path = app.newScreenshotPath()
                    view.grabToImage(function(result) { if (result.saveToFile(path)) savedNote.show() })
                }
            }
            Button2 { text: app.showStats ? qsTr("隐藏统计") : qsTr("显示统计"); onClicked: app.showStats = !app.showStats }
        }
        Component.onCompleted: hide.restart()
    }
    Rectangle {
        id: savedNote
        function show() { opacity = 1; noteTimer.restart() }
        opacity: 0
        Behavior on opacity { NumberAnimation { duration: 200 } }
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        radius: 8; color: Qt.rgba(0, 0, 0, 0.7)
        implicitWidth: note.implicitWidth + 24; implicitHeight: note.implicitHeight + 12
        Text {
            id: note; anchors.centerIn: parent; color: "white"; font.pixelSize: 13; font.family: Theme.fontUi
            text: qsTr("截图已保存")
            MouseArea { anchors.fill: parent; onClicked: app.openScreenshotFolder() }
        }
        Timer { id: noteTimer; interval: 2500; onTriggered: savedNote.opacity = 0 }
    }
}
