// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide simple home: open the app, choose a source, Start. In a Vice City sunset style (Glide's
// own Sunset City logo; no third-party game art or marks).
//   - Sources: the capture card (found and its best format picked automatically: Capture.cpp,
//     PushAutoCaptureFormat.h), PS5 / Xbox remote play, PC streaming and screen capture (Veyra's
//     own dialogs). Pref pushSimpleSource.
//   - Start opens the play page (PushPlayPage.qml), where the method, FPS target, delay, extras and
//     the before/after split live.
// Everything else stays one click away behind "All settings" (HomePage.qml).
import QtQuick
import QtQuick.Layouts
import "EffectSupport.js" as Support
import "PushGamePresets.js" as Presets

Item {
    id: simple
    objectName: "push-simple-home"
    signal requestPage(string page)
    signal showAllSettings()
    property real topInset: 0

    // Sunset palette.
    QtObject {
        id: c
        readonly property color skyTop: "#160A2E"
        readonly property color skyMid: "#4A1463"
        readonly property color skyLow: "#B7287A"
        readonly property color horizon: "#FF7A45"
        readonly property color pink: "#FF3D8B"
        readonly property color orange: "#FF9F43"
        readonly property color gold: "#FFD166"
        readonly property color panel: Qt.rgba(0.08, 0.03, 0.14, 0.95)
        readonly property color edge: Qt.rgba(1, 1, 1, 0.10)
        readonly property color text1: "#FFFFFF"
        readonly property color text2: Qt.rgba(1, 1, 1, 0.78)
        readonly property color text3: Qt.rgba(1, 1, 1, 0.60)
        readonly property color ledOk: "#5CF2A6"
        readonly property color ledWait: "#FFC24B"
        readonly property color warn: "#FFC24B"
        readonly property string display: Qt.fontFamilies().indexOf("Bahnschrift") >= 0 ? "Bahnschrift" : Theme.fontUi
    }

    // The quick setup's logic, not shown (multiplier choice, adaptive, pacing, console tip).
    PushQuickSetup { id: logic; visible: false; compact: true; width: 0; height: 0 }

    property bool pending: false
    property string problem: ""
    property bool showTip: false
    readonly property bool hasCard: (veyra.captureDevices || []).length > 0 && String(veyra.captureDeviceId || "").length > 0
    readonly property bool ready: simple.hasCard && String(veyra.captureFormatKey || "").length > 0 && !veyra.captureQueryBusy

    // Where the picture comes from (pref pushSimpleSource): the capture card, or one of Veyra's
    // streaming sources / screen capture, each through its own existing dialog.
    readonly property string source: ["card", "ps5", "pc", "xbox", "screen"].indexOf(String(veyra.preferences.pushSimpleSource)) >= 0
        ? String(veyra.preferences.pushSimpleSource) : "card"
    readonly property var sources: [
        { id: "card",   icon: "video",    label: qsTr("采集卡"),  hint: qsTr("主机接采集卡，插上 USB 就能用") },
        { id: "ps5",    icon: "gamepad",  label: qsTr("PS5"),     hint: qsTr("PS5 远程游玩，不需要采集卡。第一次会让你配对") },
        { id: "pc",     icon: "cast",     label: qsTr("PC 串流"), hint: qsTr("从另一台装了 Sunshine 的电脑串流") },
        { id: "xbox",   icon: "gamepad",  label: qsTr("Xbox"),    hint: qsTr("Xbox 远程游玩，不需要采集卡") },
        { id: "screen", icon: "monitor",  label: qsTr("屏幕"),    hint: qsTr("这台电脑上的游戏窗口或整个屏幕") }
    ]
    function pickSource(id) {
        simple.problem = ""
        veyra.setPreference("pushSimpleSource", id)
        if (id === "card" && !veyra.captureQueryBusy) veyra.refreshCaptureDevices()
    }

    Component.onCompleted: if (!veyra.hasSource && !veyra.captureQueryBusy) veyra.refreshCaptureDevices()

    function start() {
        simple.problem = ""
        veyra.logUi("push-simple", "start source=" + simple.source + " ready=" + simple.ready)
        // Already playing: back to the picture.
        if (veyra.hasSource) { simple.requestPage("play"); return }
        if (simple.source === "ps5") { veyra.openPs5Dialog(); return }
        if (simple.source === "pc") { veyra.openMoonlightDialog(); return }
        if (simple.source === "xbox") { veyra.openXboxDialog(); return }
        if (simple.source === "screen") { veyra.openScreenCaptureDialog(); return }
        if (logic.ownTechPossible && veyra.pacingRepair !== true) logic.setPacingRepair(true)
        if (simple.ready) { simple.connect(); return }
        simple.pending = true
        if (!veyra.captureQueryBusy) veyra.refreshCaptureDevices()
    }
    function connect() {
        simple.pending = false
        if (veyra.startCaptureSession()) simple.requestPage("play")
        else simple.problem = veyra.captureStatus || qsTr("没能连接采集卡，请点“所有设置”查看详情")
    }
    Connections {
        target: veyra
        function onCaptureChanged() {
            if (!simple.pending || veyra.captureQueryBusy) return
            if ((veyra.captureDevices || []).length === 0) {
                simple.pending = false
                simple.problem = qsTr("没有找到采集卡。插上 USB 线后再按开始")
            } else if (simple.ready) {
                simple.connect()
            } else if (String(veyra.captureDeviceId || "").length > 0) {
                simple.pending = false
                simple.problem = veyra.captureStatus || qsTr("这张采集卡需要手动选择格式，请点“所有设置”")
            }
        }
    }

    // ---- The scene: dusk sky, a striped sun that slowly rises and sets, a skyline. ----
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: c.skyTop }
            GradientStop { position: 0.45; color: c.skyMid }
            GradientStop { position: 0.78; color: c.skyLow }
            GradientStop { position: 1.0; color: c.horizon }
        }
    }
    Canvas {
        id: sun
        width: Math.min(simple.width * 0.55, 440); height: width
        anchors.horizontalCenter: parent.horizontalCenter
        y: simple.height - height * 0.42 - skyline.height * 0.3 + drift
        opacity: 0.9
        property real drift
        SequentialAnimation on drift {
            running: simple.visible && !Theme.reduced
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 14; duration: 6000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 14; to: 0; duration: 6000; easing.type: Easing.InOutSine }
        }
        onPaint: {
            const ctx = getContext("2d"), r = width / 2
            ctx.reset()
            const g = ctx.createLinearGradient(0, 0, 0, height)
            g.addColorStop(0, "#FFE27A"); g.addColorStop(0.5, "#FF8A4C"); g.addColorStop(1, "#FF3D8B")
            ctx.fillStyle = g
            ctx.beginPath(); ctx.arc(r, r, r, 0, Math.PI * 2); ctx.fill()
            // Retro stripes, thicker toward the bottom.
            ctx.globalCompositeOperation = "destination-out"
            for (let i = 0; i < 7; ++i) {
                const y = r * 1.05 + i * r * 0.14
                ctx.fillRect(0, y, width, 2 + i * 2.2)
            }
        }
        onWidthChanged: requestPaint()
    }
    Canvas {
        id: skyline
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: Math.min(96, simple.height * 0.14)
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const fill = ctx.createLinearGradient(0, 0, 0, height)
            fill.addColorStop(0, "#2A0E3D"); fill.addColorStop(1, "#12051C")
            ctx.fillStyle = fill
            // A fixed, hand-tuned row of towers so the skyline never changes between runs.
            const towers = [[0, .35], [.04, .55], [.07, .42], [.11, .78], [.15, .5], [.19, .62], [.24, .38], [.29, .9], [.33, .58],
                            [.37, .44], [.42, .7], [.47, .36], [.53, .66], [.58, .95], [.62, .52], [.67, .4], [.71, .74], [.76, .48],
                            [.81, .6], [.85, .86], [.89, .45], [.93, .64], [.97, .4]]
            for (let i = 0; i < towers.length; ++i) {
                const x = towers[i][0] * width, w = width * 0.045, h = towers[i][1] * height
                ctx.fillRect(x, height - h, w, h)
            }
            ctx.fillRect(0, height - 12, width, 12)
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // ---- The panel. ----
    Item {
        id: stage
        anchors.fill: parent
        anchors.topMargin: simple.topInset
        // One entrance: the panel rises in.
        opacity: 0
        transform: Translate { id: rise; y: 18 }
        Component.onCompleted: entrance.start()
        ParallelAnimation {
            id: entrance
            NumberAnimation { target: stage; property: "opacity"; to: 1; duration: Theme.reduced ? 0 : 450; easing.type: Easing.OutCubic }
            NumberAnimation { target: rise; property: "y"; to: 0; duration: Theme.reduced ? 0 : 600; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            id: column
            width: Math.min(560, stage.width - 48)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(300, column.width * 0.6)
                Layout.preferredHeight: Layout.preferredWidth * 550 / 1340
                Layout.bottomMargin: 8
                source: "sunset-logo.png"
                sourceSize.width: 600
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Rectangle {
                id: panel
                Layout.fillWidth: true
                implicitHeight: body.implicitHeight + 56
                radius: 28
                color: c.panel
                border.width: 1
                border.color: c.edge

                ColumnLayout {
                    id: body
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 28
                    spacing: 0

                    // Where the picture comes from.
                    Text {
                        text: qsTr("选择画面来源")
                        color: c.text1
                        font.family: Theme.fontUi
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    RowLayout {
                        objectName: "push-simple-sources"
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        spacing: 8
                        Repeater {
                            model: simple.sources
                            delegate: Rectangle {
                                id: tile
                                required property var modelData
                                readonly property bool on: simple.source === modelData.id
                                objectName: "push-simple-source-" + modelData.id
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                implicitHeight: 82
                                radius: 18
                                color: tileHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.06)
                                border.width: on ? 0 : 1
                                border.color: c.edge
                                scale: tileTap.pressed ? 0.96 : 1
                                Behavior on scale { NumberAnimation { duration: 120 } }
                                Behavior on color { ColorAnimation { duration: 150 } }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: parent.radius
                                    opacity: tile.on ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: 160 } }
                                    gradient: Gradient {
                                        GradientStop { position: 0; color: c.pink }
                                        GradientStop { position: 1; color: c.orange }
                                    }
                                }
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    VIcon {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        name: tile.modelData.icon
                                        size: 22
                                        color: tile.on ? "#FFFFFF" : c.text2
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: tile.modelData.label
                                        color: tile.on ? "#FFFFFF" : c.text2
                                        font.family: Theme.fontUi
                                        font.pixelSize: 13
                                        font.weight: tile.on ? Font.DemiBold : Font.Normal
                                    }
                                }
                                activeFocusOnTab: true
                                Keys.onReturnPressed: simple.pickSource(tile.modelData.id)
                                Keys.onSpacePressed: simple.pickSource(tile.modelData.id)
                                Rectangle { anchors.fill: parent; radius: 18; color: "transparent"; border.width: tile.activeFocus ? 2 : 0; border.color: c.gold }
                                HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { id: tileTap; onTapped: simple.pickSource(tile.modelData.id) }
                            }
                        }
                    }
                    Text {
                        objectName: "push-simple-source-hint"
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        visible: simple.source !== "card"
                        text: (simple.sources.find(o => o.id === simple.source) || {}).hint || ""
                        color: c.text2
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    // The card.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 14
                        visible: simple.source === "card"
                        implicitHeight: 56
                        radius: 18
                        color: Qt.rgba(1, 1, 1, 0.06)
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16; anchors.rightMargin: 16
                            spacing: 12
                            Rectangle {
                                id: led
                                implicitWidth: 10; implicitHeight: 10; radius: 5
                                color: veyra.captureQueryBusy ? c.text3 : simple.ready ? c.ledOk : c.ledWait
                                Behavior on color { ColorAnimation { duration: 300 } }
                                // Breathes only while it is actually looking.
                                SequentialAnimation on opacity {
                                    running: veyra.captureQueryBusy && !Theme.reduced
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 500 }
                                    NumberAnimation { to: 1; duration: 500 }
                                    onRunningChanged: if (!running) led.opacity = 1
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    objectName: "push-simple-card"
                                    Layout.fillWidth: true
                                    text: veyra.captureQueryBusy ? qsTr("正在查找采集卡…")
                                        : simple.hasCard ? veyra.captureDeviceLabel
                                        : qsTr("没有找到采集卡，请插上 USB 线")
                                    color: simple.hasCard || veyra.captureQueryBusy ? c.text1 : c.warn
                                    font.family: Theme.fontUi
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: simple.hasCard && !veyra.captureQueryBusy && veyra.captureFormatLabel.length > 0
                                    text: veyra.captureFormatLabel
                                    color: c.text3
                                    font.family: Theme.fontUi
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            VSelect {
                                objectName: "push-simple-device"
                                visible: (veyra.captureDevices || []).length > 1
                                implicitWidth: 140
                                options: veyra.captureDevices || []
                                value: veyra.captureDeviceLabel
                                onPicked: id => veyra.captureDeviceId = id
                            }
                            PillButton {
                                objectName: "push-simple-refresh"
                                text: qsTr("重新查找")
                                enabled: !veyra.captureQueryBusy
                                onClicked: veyra.refreshCaptureDevices()
                            }
                        }
                    }

                    Rectangle {
                        id: startButton
                        objectName: "push-simple-start"
                        Layout.fillWidth: true
                        Layout.topMargin: 22
                        implicitWidth: Math.max(160, startText.implicitWidth + 64)
                        implicitHeight: 52
                        radius: 18
                        clip: true
                        opacity: simple.pending ? 0.75 : 1
                        scale: startTap.pressed ? 0.96 : startHover.hovered ? 1.03 : 1
                        Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: c.pink }
                            GradientStop { position: 1; color: c.orange }
                        }
                        // A light sweep crosses the button every few seconds, and keeps going while it connects.
                        Rectangle {
                            id: sweep
                            width: 60; height: parent.height * 2
                            y: -parent.height / 2
                            rotation: 20
                            opacity: 0.35
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0; color: "transparent" }
                                GradientStop { position: 0.5; color: "#FFFFFF" }
                                GradientStop { position: 1; color: "transparent" }
                            }
                            SequentialAnimation on x {
                                running: simple.visible && !Theme.reduced
                                loops: Animation.Infinite
                                NumberAnimation { from: -80; to: startButton.width + 20; duration: simple.pending ? 700 : 900; easing.type: Easing.InOutQuad }
                                PauseAnimation { duration: simple.pending ? 100 : 2600 }
                            }
                        }
                        Text {
                            id: startText
                            anchors.centerIn: parent
                            text: simple.pending ? qsTr("正在连接…") : veyra.hasSource ? qsTr("回到画面") : qsTr("开始")
                            color: "#FFFFFF"
                            font.family: c.display
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            font.italic: true
                        }
                        activeFocusOnTab: true
                        Keys.onReturnPressed: if (!simple.pending) simple.start()
                        Keys.onSpacePressed: if (!simple.pending) simple.start()
                        Rectangle { anchors.fill: parent; radius: 18; color: "transparent"; border.width: startButton.activeFocus ? 2 : 0; border.color: "#FFFFFF" }
                        HoverHandler { id: startHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { id: startTap; enabled: !simple.pending; onTapped: simple.start() }
                    }

                    Text {
                        objectName: "push-simple-problem"
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        visible: simple.problem.length > 0
                        text: simple.problem
                        color: c.warn
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }
                    Text {
                        objectName: "push-simple-tip"
                        Layout.fillWidth: true
                        Layout.topMargin: 12
                        visible: simple.showTip
                        text: logic.consoleTip
                        color: c.text2
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        lineHeight: 1.2
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Row {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 16
                spacing: 10
                PillButton { objectName: "push-simple-help"; text: qsTr("黑屏？"); onClicked: simple.showTip = !simple.showTip }
                PillButton { objectName: "push-simple-all-settings"; text: qsTr("所有设置"); onClicked: simple.showAllSettings() }
            }
        }
    }

    // A small rounded glass button.
    component PillButton: Rectangle {
        id: pb
        property string text: ""
        signal clicked()
        implicitWidth: pbText.implicitWidth + 28
        implicitHeight: 32
        radius: 16
        color: pbHover.hovered && enabled ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(1, 1, 1, 0.08)
        border.width: 1
        border.color: c.edge
        opacity: enabled ? 1 : 0.5
        scale: pbTap.pressed ? 0.95 : 1
        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on scale { NumberAnimation { duration: 120 } }
        Text {
            id: pbText
            anchors.centerIn: parent
            text: pb.text
            color: c.text1
            font.family: Theme.fontUi
            font.pixelSize: 13
            font.weight: Font.Medium
        }
        HoverHandler { id: pbHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { id: pbTap; enabled: pb.enabled; onTapped: pb.clicked() }
    }
}
