// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide simple home: one screen, plug and play, in a Vice City sunset style (Glide's own
// Sunset City logo; no third-party game art or marks).
//   - The capture card is found and its best format picked automatically (Capture.cpp,
//     PushAutoCaptureFormat.h); nobody chooses a resolution or a pixel format.
//   - Any FPS target: a slider (30-360, marks double as picks) or typing (pref pushSimpleFps, 30-500).
//   - The frame strip shows what will happen: white bars are the console's frames, sunset bars
//     are the frames this PC makes, in the real ratio for the chosen target.
//   - One switch (low lag), one button (Start).
// Everything else stays one click away behind "All settings" (HomePage.qml). Every choice only sets
// existing settings, so Professional mode shows exactly what was set.
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

    // The FPS target: any number from 30 to 500.
    readonly property int minFps: 30
    readonly property int maxFps: 500
    readonly property int screenHz: veyra.displayRefreshHz > 0 ? Math.round(veyra.displayRefreshHz) : 0
    readonly property int fps: {
        const n = Number(veyra.preferences.pushSimpleFps || 60)
        return n >= simple.minFps && n <= simple.maxFps ? Math.round(n) : 60
    }
    function setFps(n) {
        n = Math.max(simple.minFps, Math.min(simple.maxFps, Math.round(n)))
        if (n === simple.fps && veyra.preferences.pushSimpleFps !== undefined) return
        veyra.setPreference("pushSimpleFps", n)
        simple.applyFps(n)
    }
    // Friendly steps: 5 up to 60, 10 up to 240, 20 above.
    function stepFps(dir) {
        const n = simple.fps
        const step = dir > 0 ? (n < 60 ? 5 : n < 240 ? 10 : 20) : (n <= 60 ? 5 : n <= 240 ? 10 : 20)
        simple.setFps(Math.round((n + dir * step) / step) * step)
    }
    // The game's frame rate (30 / 40 / 60, the quick setup's pushGameFps) and what this PC reaches.
    readonly property int gameFps: logic.baseFps
    readonly property int maxMultiplier: logic.offeredMultipliers.reduce((a, b) => Math.max(a, b), 1)
    readonly property int reach: simple.gameFps * simple.maxMultiplier
    readonly property int shownFps: Math.max(simple.gameFps, Math.min(simple.fps, simple.reach))
    function cycleGameFps() {
        const next = simple.gameFps === 30 ? 40 : simple.gameFps === 40 ? 60 : 30
        logic.setGameFps(String(next))
        simple.applyFps(simple.fps)
    }

    // Frame generation set for the target: the multiplier (or the adaptive ceiling) and an output
    // cap at exactly that FPS. A target the game already reaches turns frame generation off.
    function applyFps(n) {
        if (!logic.fgPossible) return
        const game = simple.gameFps
        if (n <= game + 0.5) { veyra.fgEnabled = false; return }
        if (logic.adaptivePossible && !logic.adaptive) veyra.fgAdaptive = true
        const m = logic.adaptivePossible ? Presets.adaptiveCeiling(logic.offeredMultipliers, n) : logic.multiplierFor(n, game)
        if (m < 2) return
        if (!veyra.fgEnabled) veyra.fgEnabled = true
        veyra.fgMultiplier = m
        if (Number(veyra.contentRate) === 0) veyra.contentRate = 1
        if (!veyra.presentationOwned) {
            if (simple.screenHz > 0 && n === simple.screenHz) veyra.outputRateMode = 1
            else { veyra.outputCustomFps = n; veyra.outputRateMode = 2 }
        }
    }

    // Low lag: each frame shows as soon as it is ready. Off: wait for the screen's refresh, so every
    // frame is shown once. Both keep the one-frame GPU queue and fullscreen on connect.
    readonly property bool lowLag: veyra.preferences.pushSimpleLowLag === true
    function setLowLag(on) {
        veyra.logUi("push-simple", "lowLag=" + on)
        veyra.setPreference("pushSimpleLowLag", on)
        if (veyra.nvidiaLowLatencyAvailable === true) veyra.nvidiaLowLatency = 1
        else veyra.fgLowQueue = true
        veyra.displaySync = on ? 2 : 1
        veyra.variableRefresh = true
        veyra.setPreference("pushAutoFullscreen", true)
    }

    Component.onCompleted: if (!veyra.hasSource && !veyra.captureQueryBusy) veyra.refreshCaptureDevices()

    function start() {
        simple.problem = ""
        if (logic.ownTechPossible && veyra.pacingRepair !== true) logic.setPacingRepair(true)
        simple.applyFps(simple.fps)
        simple.setLowLag(simple.lowLag)
        veyra.logUi("push-simple", "start fps=" + simple.fps + " ready=" + simple.ready)
        if (simple.ready) { simple.connect(); return }
        simple.pending = true
        if (!veyra.captureQueryBusy) veyra.refreshCaptureDevices()
    }
    function connect() {
        simple.pending = false
        if (veyra.startCaptureSession()) simple.requestPage("min")
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

                    // The card.
                    Rectangle {
                        Layout.fillWidth: true
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

                    // The number: sunset-filled, it rolls to the new value. Tap it to type.
                    Item {
                        Layout.fillWidth: true
                        Layout.topMargin: 22
                        implicitHeight: fpsNumber.height
                        property real shown: simple.fps
                        Behavior on shown { NumberAnimation { duration: Theme.reduced ? 0 : 260; easing.type: Easing.OutCubic } }
                        // The figure, drawn with a sunset fill.
                        TextMetrics { id: fpsMetrics; font: fpsNumber.font; text: fpsNumber.text }
                        Canvas {
                            id: fpsNumber
                            objectName: "push-simple-fps"
                            visible: !fpsField.visible
                            readonly property string text: String(Math.round(parent.shown))
                            readonly property font font: Qt.font({ family: c.display, pixelSize: 92, weight: Font.Bold, italic: true })
                            readonly property real textBase: 92 * 0.88
                            baselineOffset: textBase
                            width: Math.ceil(fpsMetrics.advanceWidth + 16)
                            height: 104
                            opacity: logic.fgPossible ? 1 : 0.5
                            onTextChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.reset()
                                const g = ctx.createLinearGradient(0, 8, 0, height - 8)
                                g.addColorStop(0, String(c.gold)); g.addColorStop(0.55, String(c.orange)); g.addColorStop(1, String(c.pink))
                                ctx.fillStyle = g
                                ctx.font = "italic bold 92px " + c.display
                                ctx.textBaseline = "alphabetic"
                                ctx.fillText(text, 2, textBase)
                            }
                        }
                        TapHandler {
                            enabled: logic.fgPossible
                            onTapped: { fpsField.text = String(simple.fps); fpsField.visible = true; fpsField.forceActiveFocus(); fpsField.selectAll() }
                        }
                        HoverHandler { enabled: logic.fgPossible; cursorShape: Qt.IBeamCursor }
                        TextInput {
                            id: fpsField
                            objectName: "push-simple-fps-input"
                            visible: false
                            anchors.left: parent.left
                            anchors.leftMargin: 2
                            anchors.baseline: fpsNumber.baseline
                            width: Math.max(fpsNumber.width, 160)
                            color: c.gold
                            selectionColor: Qt.rgba(1, 0.24, 0.55, 0.45)
                            selectedTextColor: "#FFFFFF"
                            font: fpsNumber.font
                            validator: IntValidator { bottom: 1; top: 999 }
                            onAccepted: { simple.setFps(parseInt(text) || simple.fps); visible = false }
                            onActiveFocusChanged: if (!activeFocus) visible = false
                        }
                        Text {
                            x: (fpsField.visible ? fpsField.x + fpsField.contentWidth + 12 : fpsMetrics.advanceWidth + 14)
                            anchors.baseline: fpsNumber.baseline
                            text: "FPS"
                            color: c.text2
                            font.family: c.display
                            font.pixelSize: 22
                            font.weight: Font.Bold
                            font.italic: true
                        }
                    }

                    // The slider: 30 to 360 in steps of 5, marks at the common targets and the screen's rate.
                    Item {
                        id: slider
                        objectName: "push-simple-slider"
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        implicitHeight: 54
                        opacity: logic.fgPossible ? 1 : 0.45
                        readonly property int lo: 30
                        readonly property int hi: 360
                        property real knobX: (Math.min(simple.fps, hi) - lo) / (hi - lo) * track.width
                        Behavior on knobX { enabled: !drag.active; NumberAnimation { duration: Theme.reduced ? 0 : 220; easing.type: Easing.OutCubic } }
                        // The screen's rate replaces a common mark that would sit too close to it.
                        readonly property bool screenMark: simple.screenHz >= lo && simple.screenHz <= hi
                        readonly property var marks: [60, 120, 240].filter(m => !screenMark || Math.abs(m - simple.screenHz) >= 40)
                            .concat(screenMark ? [simple.screenHz] : []).sort((a, b) => a - b)
                        function valueAt(x) { return Math.round((lo + Math.max(0, Math.min(1, x / track.width)) * (hi - lo)) / 5) * 5 }
                        function xOf(v) { return (v - lo) / (hi - lo) * track.width }
                        Item {
                            id: track
                            x: 12; width: parent.width - 24
                            y: 12; height: 8
                            Rectangle { anchors.fill: parent; radius: 4; color: Qt.rgba(1, 1, 1, 0.12) }
                            Rectangle {
                                width: Math.max(8, slider.knobX); height: parent.height; radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0; color: c.pink }
                                    GradientStop { position: 1; color: c.orange }
                                }
                            }
                            Rectangle {
                                id: knob
                                objectName: "push-simple-slider-knob"
                                x: slider.knobX - width / 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 26; height: 26; radius: 13
                                color: "#FFFFFF"
                                scale: drag.active || sliderTap.pressed ? 1.18 : knobHover.hovered ? 1.08 : 1
                                Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }
                                Rectangle { anchors.centerIn: parent; width: 10; height: 10; radius: 5; color: c.pink }
                                HoverHandler { id: knobHover }
                            }
                        }
                        // The marks double as quick picks.
                        Repeater {
                            model: slider.marks
                            delegate: Text {
                                id: mark
                                required property int modelData
                                objectName: "push-simple-pick-" + modelData
                                readonly property bool on: simple.fps === modelData
                                x: track.x + slider.xOf(modelData) - width / 2
                                y: 32
                                text: modelData === simple.screenHz ? qsTr("%1 · 屏幕").arg(modelData) : modelData
                                color: on ? c.gold : markHover.hovered ? c.text1 : c.text3
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                                font.weight: on ? Font.DemiBold : Font.Normal
                                Behavior on color { ColorAnimation { duration: 150 } }
                                HoverHandler { id: markHover; enabled: logic.fgPossible; cursorShape: Qt.PointingHandCursor }
                                TapHandler { enabled: logic.fgPossible; onTapped: simple.setFps(mark.modelData) }
                            }
                        }
                        // Click anywhere on the track, or drag; arrow keys move it in steps of 5.
                        Item {
                            anchors.left: parent.left; anchors.right: parent.right
                            y: 0; height: 32
                            activeFocusOnTab: logic.fgPossible
                            Keys.onLeftPressed: simple.setFps(simple.fps - 5)
                            Keys.onRightPressed: simple.setFps(simple.fps + 5)
                            Rectangle { anchors.fill: parent; radius: 12; color: "transparent"; border.width: parent.activeFocus ? 2 : 0; border.color: c.gold }
                            HoverHandler { enabled: logic.fgPossible; cursorShape: Qt.PointingHandCursor }
                            TapHandler { id: sliderTap; enabled: logic.fgPossible; onTapped: ev => simple.setFps(slider.valueAt(ev.position.x - track.x)) }
                            DragHandler {
                                id: drag
                                enabled: logic.fgPossible
                                target: null
                                xAxis.enabled: true; yAxis.enabled: false
                                onCentroidChanged: if (active) simple.setFps(slider.valueAt(centroid.position.x - track.x))
                            }
                        }
                    }

                    // The frame strip: white bars come from the console, sunset bars are made by this PC.
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 14
                        implicitHeight: 76
                        radius: 18
                        color: Qt.rgba(0, 0, 0, 0.22)
                        clip: true
                        Item {
                            id: strip
                            objectName: "push-simple-strip"
                            anchors.fill: parent
                            anchors.margins: 14
                            clip: true
                            readonly property int slot: 12
                            readonly property real ratio: simple.shownFps / Math.max(1, simple.gameFps)
                            // How many bars before the pattern repeats (5.5X repeats every 11).
                            readonly property int period: {
                                for (let k = 1; k <= 40; ++k) if (Math.abs(k / ratio - Math.round(k / ratio)) < 0.001) return k
                                return 40
                            }
                            readonly property int count: Math.ceil(width / slot) + period + 1
                            function real(n) { return Math.floor(n / ratio) !== Math.floor((n - 1) / ratio) }
                            Row {
                                id: bars
                                spacing: strip.slot - 6
                                // Glides left by one pattern and starts over; it lines up exactly, so it never jumps.
                                NumberAnimation on x {
                                    running: simple.visible && !Theme.reduced && logic.fgPossible
                                    loops: Animation.Infinite
                                    from: 0; to: -strip.period * strip.slot
                                    duration: strip.period * 260
                                }
                                Repeater {
                                    model: strip.count
                                    delegate: Rectangle {
                                        required property int index
                                        readonly property bool fromConsole: !logic.fgPossible || strip.real(index)
                                        width: 6; height: strip.height
                                        radius: 3
                                        color: fromConsole ? "#FFFFFF" : c.pink
                                        gradient: fromConsole ? null : sunsetBar
                                    }
                                }
                            }
                            Gradient {
                                id: sunsetBar
                                GradientStop { position: 0; color: c.orange }
                                GradientStop { position: 1; color: c.pink }
                            }
                        }
                    }

                    // What the strip means. Tap the console figure to switch 30 / 40 / 60.
                    Row {
                        Layout.topMargin: 10
                        spacing: 18
                        Row {
                            spacing: 7
                            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6; height: 14; radius: 3; color: "#FFFFFF" }
                            Text {
                                objectName: "push-simple-game-fps"
                                text: qsTr("主机 %1").arg(simple.gameFps)
                                color: gameHover.hovered ? c.text1 : c.text2
                                font.family: Theme.fontUi
                                font.pixelSize: 13
                                font.underline: gameHover.hovered
                                HoverHandler { id: gameHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: simple.cycleGameFps() }
                            }
                        }
                        Row {
                            visible: logic.fgPossible && simple.shownFps > simple.gameFps
                            spacing: 7
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter; width: 6; height: 14; radius: 3
                                gradient: Gradient { GradientStop { position: 0; color: c.orange } GradientStop { position: 1; color: c.pink } }
                            }
                            Text {
                                text: qsTr("电脑生成 %1").arg(simple.shownFps - simple.gameFps)
                                color: c.text2
                                font.family: Theme.fontUi
                                font.pixelSize: 13
                            }
                        }
                    }
                    Text {
                        objectName: "push-simple-fps-note"
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        wrapMode: Text.WordWrap
                        visible: text.length > 0
                        text: !logic.fgPossible ? (Support.status(veyra, "frame-generation").reason || "")
                            : simple.fps > simple.reach ? qsTr("你的显卡最高约 %1 FPS，会尽量接近").arg(simple.reach)
                            : ""
                        color: logic.fgPossible ? c.text3 : c.warn
                        font.family: Theme.fontUi
                        font.pixelSize: 12
                    }

                    // Low lag and Start.
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 22
                        spacing: 12
                        Rectangle {
                            objectName: "push-simple-lowlag"
                            implicitWidth: 46; implicitHeight: 28; radius: 14
                            color: simple.lowLag ? c.pink : Qt.rgba(1, 1, 1, 0.16)
                            Behavior on color { ColorAnimation { duration: 200 } }
                            Rectangle {
                                width: 22; height: 22; radius: 11
                                anchors.verticalCenter: parent.verticalCenter
                                x: simple.lowLag ? parent.width - width - 3 : 3
                                color: "#FFFFFF"
                                Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }
                            }
                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: simple.setLowLag(!simple.lowLag) }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("低延迟模式")
                            color: c.text1
                            font.family: Theme.fontUi
                            font.pixelSize: 14
                            TapHandler { onTapped: simple.setLowLag(!simple.lowLag) }
                        }
                        Rectangle {
                            id: startButton
                            objectName: "push-simple-start"
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
                                text: simple.pending ? qsTr("正在连接…") : qsTr("开始")
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
