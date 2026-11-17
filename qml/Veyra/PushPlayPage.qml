// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the Glide contributors. Part of Glide, a modified version of Veyra.
//
// Glide play page (page "play", simple mode only): the game on the left, one small panel on the
// right with everything a player changes while playing:
//   - Method: frame generation off, or one of the engine's backends (fgBackendChoices).
//   - Frame target: any FPS from 30 to 360 (slider, marks double as picks) and the game's own rate.
//   - Delay: lowest / balanced / smoothest (prefs pushSimpleDelay; NVIDIA low latency or the low
//     queue, display sync, VRR).
//   - Extras: Live HUD Lock, Frame Pacing Repair, sharper picture (super resolution).
//   - Before / after: Veyra's live split (compareMode 2; frame generation pauses while it shows,
//     so it compares the picture) and a 10 s recorded comparison clip (pushStartCompareClip), which
//     does show the generated frames.
// Every control only sets existing engine settings, so "All settings" shows what was set.
import QtQuick
import QtQuick.Layouts
import "EffectSupport.js" as Support
import "PushGamePresets.js" as Presets

VPage {
    id: play
    objectName: "push-play-page"
    signal requestPage(string page)
    signal requestFullscreen()

    readonly property int margin: 12
    readonly property int panelWidth: Math.min(360, Math.max(300, width * 0.3))
    readonly property rect videoViewport: Qt.rect(margin, margin, Math.max(1, width - panelWidth - 3 * margin), Math.max(1, height - 2 * margin))

    QtObject {
        id: c
        readonly property color bgTop: "#120821"
        readonly property color bgLow: "#1D0B2E"
        readonly property color panel: Qt.rgba(1, 1, 1, 0.045)
        readonly property color card: Qt.rgba(1, 1, 1, 0.06)
        readonly property color edge: Qt.rgba(1, 1, 1, 0.09)
        readonly property color pink: "#FF3D8B"
        readonly property color orange: "#FF9F43"
        readonly property color gold: "#FFD166"
        readonly property color text1: "#FFFFFF"
        readonly property color text2: Qt.rgba(1, 1, 1, 0.78)
        readonly property color text3: Qt.rgba(1, 1, 1, 0.58)
        readonly property color ok: "#5CF2A6"
        readonly property color warn: "#FFC24B"
        readonly property string display: Qt.fontFamilies().indexOf("Bahnschrift") >= 0 ? "Bahnschrift" : Theme.fontUi
    }

    PushQuickSetup { id: logic; visible: false; compact: true; width: 0; height: 0 }

    // ---- Frame target ----
    readonly property int screenHz: veyra.displayRefreshHz > 0 ? Math.round(veyra.displayRefreshHz) : 0
    readonly property int fps: {
        const n = Number(veyra.preferences.pushSimpleFps || 60)
        return n >= 30 && n <= 500 ? Math.round(n) : 60
    }
    readonly property int gameFps: logic.baseFps
    readonly property int maxMultiplier: logic.offeredMultipliers.reduce((a, b) => Math.max(a, b), 1)
    readonly property int reach: gameFps * maxMultiplier
    readonly property bool fgOk: logic.fgPossible
    readonly property bool fgOff: veyra.preferences.pushSimpleFgOff === true
    function setFps(n) {
        n = Math.max(30, Math.min(500, Math.round(n)))
        if (n !== play.fps) veyra.setPreference("pushSimpleFps", n)
        play.applyFps(n)
    }
    function setGameFps(n) {
        logic.setGameFps(String(n))
        play.applyFps(play.fps, n)
    }
    function applyFps(n, game) {
        if (!play.fgOk) return
        game = game || play.gameFps
        if (play.fgOff || n <= game + 0.5) { veyra.fgEnabled = false; return }
        if (logic.adaptivePossible && !logic.adaptive) veyra.fgAdaptive = true
        const m = logic.adaptivePossible ? Presets.adaptiveCeiling(logic.offeredMultipliers, n) : logic.multiplierFor(n, game)
        if (m < 2) return
        if (!veyra.fgEnabled) veyra.fgEnabled = true
        veyra.fgMultiplier = m
        if (Number(veyra.contentRate) === 0) veyra.contentRate = 1
        if (!veyra.presentationOwned) {
            if (play.screenHz > 0 && n === play.screenHz) veyra.outputRateMode = 1
            else { veyra.outputCustomFps = n; veyra.outputRateMode = 2 }
        }
    }

    // ---- Method ----
    readonly property var methods: veyra.fgBackendChoices || []
    function setMethod(id) {
        veyra.logUi("push-play", "method=" + id)
        if (id === "off") { veyra.setPreference("pushSimpleFgOff", true); veyra.fgEnabled = false; return }
        veyra.setPreference("pushSimpleFgOff", false)
        if (veyra.fgBackendName !== id) veyra.fgBackendName = id
        Qt.callLater(() => play.applyFps(play.fps))
    }

    // ---- Delay ----
    readonly property string delay: ["low", "balanced", "smooth"].indexOf(String(veyra.preferences.pushSimpleDelay)) >= 0
        ? String(veyra.preferences.pushSimpleDelay) : "balanced"
    function setDelay(d) {
        veyra.setPreference("pushSimpleDelay", d)
        play.applyDelay(d)
    }
    function applyDelay(d) {
        const nvidia = veyra.nvidiaLowLatencyAvailable === true
        const fast = d !== "smooth"
        if (nvidia) veyra.nvidiaLowLatency = fast ? 1 : 0
        else veyra.fgLowQueue = fast
        veyra.displaySync = d === "low" ? 2 : 1
        veyra.variableRefresh = true
        veyra.logUi("push-play", "delay=" + d)
    }
    readonly property string delayHint: delay === "low" ? qsTr("画面一好就显示，延迟最低，可能有轻微撕裂")
        : delay === "smooth" ? qsTr("等屏幕刷新再显示，最顺滑，延迟稍高")
        : qsTr("低延迟，同时不撕裂（推荐）")

    // ---- Before / after ----
    readonly property bool split: veyra.compareMode === 2
    readonly property bool clipRecording: logic.compareRecording

    // Apply the saved choices when the page opens (a new session starts from them).
    onCurrentChanged: if (current) { play.applyDelay(play.delay); play.applyFps(play.fps) }

    function stop() {
        if (play.split) veyra.compareMode = 0
        veyra.stopPlayback()
        play.requestPage("home")
    }

    // ---- Background and the picture plate ----
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: c.bgTop }
            GradientStop { position: 1; color: c.bgLow }
        }
    }
    Rectangle {
        objectName: "push-play-stage"
        x: play.videoViewport.x; y: play.videoViewport.y
        width: play.videoViewport.width; height: play.videoViewport.height
        radius: 14
        color: "#000000"
        Text {
            anchors.centerIn: parent
            visible: !veyra.hasSource
            text: qsTr("等待画面…")
            color: c.text3
            font.family: Theme.fontUi
            font.pixelSize: 14
        }
        // Live split: drag on the picture to move the divider.
        MouseArea {
            anchors.fill: parent
            enabled: play.split
            cursorShape: play.split ? Qt.SplitHCursor : Qt.ArrowCursor
            onPressed: mouse => veyra.setCompareSplitAt(mouse.x / width)
            onPositionChanged: mouse => { if (pressed) veyra.setCompareSplitAt(Math.max(0, Math.min(1, mouse.x / width))) }
        }
    }

    // ---- The panel ----
    Rectangle {
        id: panel
        objectName: "push-play-panel"
        x: play.width - play.panelWidth - play.margin
        y: play.margin
        width: play.panelWidth
        height: play.height - 2 * play.margin
        radius: 20
        color: c.panel
        border.width: 1
        border.color: c.edge
        clip: true

        Flickable {
            id: flick
            objectName: "push-play-scroll"
            anchors.fill: parent
            anchors.bottomMargin: footer.height
            contentHeight: body.implicitHeight + 32
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: body
                x: 18; y: 18
                width: flick.width - 36
                spacing: 10

                // Status: the source and live frame rates.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        implicitWidth: 9; implicitHeight: 9; radius: 4.5
                        color: veyra.hasSource ? c.ok : c.warn
                    }
                    Text {
                        objectName: "push-play-source"
                        Layout.fillWidth: true
                        text: veyra.hasSource ? (veyra.isCapture && veyra.captureDeviceLabel.length > 0 ? veyra.captureDeviceLabel : qsTr("已连接"))
                                              : qsTr("正在连接…")
                        color: c.text2
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }
                Row {
                    objectName: "push-play-rates"
                    spacing: 10
                    readonly property int inFps: Math.round(Number(veyra.sourceFps || 0))
                    readonly property int outFps: veyra.submitFpsKnown ? Math.round(Number(veyra.submitFps || 0)) : 0
                    Text {
                        anchors.baseline: outText.baseline
                        text: parent.inFps > 0 ? parent.inFps : "–"
                        color: c.text2
                        font.family: c.display; font.pixelSize: 26; font.weight: Font.Bold; font.italic: true
                    }
                    Text {
                        anchors.baseline: outText.baseline
                        text: "→"
                        color: c.text3
                        font.family: Theme.fontUi; font.pixelSize: 20
                    }
                    Text {
                        id: outText
                        text: parent.outFps > 0 ? parent.outFps : "–"
                        color: c.gold
                        font.family: c.display; font.pixelSize: 40; font.weight: Font.Bold; font.italic: true
                    }
                    Text {
                        anchors.baseline: outText.baseline
                        text: "FPS"
                        color: c.text3
                        font.family: c.display; font.pixelSize: 15; font.weight: Font.Bold; font.italic: true
                    }
                }

                // Method.
                Section {
                    title: qsTr("补帧方法")
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Chip {
                            objectName: "push-play-method-off"
                            text: qsTr("关")
                            on: play.fgOff || !veyra.fgEnabled && !play.fgOk
                            onClicked: play.setMethod("off")
                        }
                        Repeater {
                            model: play.methods
                            delegate: Chip {
                                required property var modelData
                                objectName: "push-play-method-" + modelData.id
                                text: modelData.label || modelData.id
                                enabled: !modelData.disabled
                                on: !play.fgOff && veyra.fgBackendName === modelData.id
                                tip: modelData.disabled ? String(modelData.note || "") : ""
                                onClicked: play.setMethod(modelData.id)
                            }
                        }
                    }
                    Note {
                        visible: text.length > 0
                        text: !play.fgOk ? (Support.status(veyra, "frame-generation").reason || "")
                            : play.methods.filter(o => o.disabled).map(o => (o.label || o.id) + ": " + (String(o.note || "") || qsTr("这台电脑不能运行"))).join("\n")
                        color: play.fgOk ? c.text3 : c.warn
                    }
                }

                // Frame target.
                Section {
                    title: qsTr("目标帧率")
                    enabled: play.fgOk && !play.fgOff
                    opacity: enabled ? 1 : 0.45
                    Row {
                        spacing: 6
                        Text {
                            objectName: "push-play-fps"
                            text: play.fps
                            color: c.text1
                            font.family: c.display; font.pixelSize: 34; font.weight: Font.Bold; font.italic: true
                        }
                        Text {
                            anchors.baseline: parent.children[0].baseline
                            text: "FPS"
                            color: c.text3
                            font.family: c.display; font.pixelSize: 14; font.weight: Font.Bold; font.italic: true
                        }
                    }
                    FpsSlider { Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 2
                        spacing: 8
                        Text {
                            text: qsTr("游戏本身")
                            color: c.text3
                            font.family: Theme.fontUi; font.pixelSize: 12
                        }
                        Seg {
                            objectName: "push-play-game-fps"
                            Layout.fillWidth: true
                            options: [{ id: 30, label: "30" }, { id: 40, label: "40" }, { id: 60, label: "60" }]
                            current: play.gameFps
                            onPicked: id => play.setGameFps(id)
                        }
                    }
                    Note {
                        visible: text.length > 0
                        text: play.fps > play.reach ? qsTr("你的显卡最高约 %1 FPS，会尽量接近").arg(play.reach) : ""
                    }
                }

                // Delay.
                Section {
                    title: qsTr("延迟")
                    Seg {
                        objectName: "push-play-delay"
                        Layout.fillWidth: true
                        options: [{ id: "low", label: qsTr("最低") }, { id: "balanced", label: qsTr("平衡") }, { id: "smooth", label: qsTr("最顺滑") }]
                        current: play.delay
                        onPicked: id => play.setDelay(id)
                    }
                    Note { text: play.delayHint }
                }

                // Extras.
                Section {
                    title: qsTr("更多")
                    SwitchRow {
                        objectName: "push-play-hudlock"
                        text: qsTr("锁定 HUD")
                        hint: qsTr("准星、血条和文字不再抖动")
                        enabled: logic.ownTechPossible
                        checked: veyra.hudLock === true
                        onToggled: on => veyra.hudLock = on
                    }
                    SwitchRow {
                        objectName: "push-play-pacing"
                        text: qsTr("修复卡顿")
                        hint: qsTr("把不均匀的游戏帧排整齐")
                        enabled: logic.ownTechPossible
                        checked: logic.pacingOn
                        onToggled: on => logic.setPacingRepair(on)
                    }
                    SwitchRow {
                        objectName: "push-play-sharp"
                        text: qsTr("画面更清晰")
                        hint: Support.available(veyra, "sr") ? qsTr("放大到屏幕分辨率") : Support.reason(veyra, "sr")
                        enabled: Support.available(veyra, "sr")
                        checked: veyra.srEnabled === true
                        onToggled: on => veyra.srEnabled = on
                    }
                }

                // Before / after.
                Section {
                    title: qsTr("前后对比")
                    SwitchRow {
                        objectName: "push-play-split"
                        text: qsTr("分屏对比")
                        hint: qsTr("左边原画，右边 Glide。在画面上拖动分割线。对比时补帧暂停")
                        enabled: veyra.hasSource
                        checked: play.split
                        onToggled: on => veyra.compareMode = on ? 2 : 0
                    }
                    ActionButton {
                        objectName: "push-play-compare-clip"
                        Layout.fillWidth: true
                        text: play.clipRecording ? qsTr("正在录制对比视频…") : qsTr("录制 10 秒对比视频")
                        enabled: veyra.hasSource && !play.clipRecording
                        onClicked: veyra.pushStartCompareClip(10, 0)
                    }
                    Note { text: qsTr("视频里能看到补出来的帧，可直接分享") }
                }
            }
        }

        // A soft fade at the bottom while more is below.
        Rectangle {
            anchors.left: panel.left; anchors.right: panel.right
            anchors.bottom: footer.top
            height: 36
            visible: flick.contentY + flick.height < flick.contentHeight - 4
            gradient: Gradient {
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 1; color: "#1D0B2E" }
            }
        }
        // Footer: fullscreen and stop.
        Rectangle {
            id: footer
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 72
            color: Qt.rgba(0, 0, 0, 0.22)
            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10
                ActionButton {
                    objectName: "push-play-fullscreen"
                    Layout.fillWidth: true
                    text: qsTr("全屏")
                    primary: true
                    enabled: veyra.hasSource
                    onClicked: play.requestFullscreen()
                }
                ActionButton {
                    objectName: "push-play-stop"
                    Layout.fillWidth: true
                    text: qsTr("停止")
                    onClicked: play.stop()
                }
            }
        }
    }

    // ---- Small parts ----
    // The sunset fill of a selected control: a layer that fades in (a Rectangle's gradient
    // cannot be switched on and off by a binding reliably).
    component SunsetFill: Rectangle {
        property bool on: false
        anchors.fill: parent
        radius: parent.radius
        opacity: on ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 160 } }
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#FF3D8B" }
            GradientStop { position: 1; color: "#FF9F43" }
        }
    }
    component Section: ColumnLayout {
        property string title: ""
        Layout.fillWidth: true
        Layout.topMargin: 2
        spacing: 6
        Text {
            text: parent.title
            color: c.text1
            font.family: Theme.fontUi
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
    }

    component Note: Text {
        Layout.fillWidth: true
        color: c.text3
        font.family: Theme.fontUi
        font.pixelSize: 12
        lineHeight: 1.15
        wrapMode: Text.WordWrap
    }

    component Chip: Rectangle {
        id: chip
        property string text: ""
        property bool on: false
        property string tip: ""
        signal clicked()
        implicitWidth: chipText.implicitWidth + 28
        implicitHeight: 34
        radius: 17
        color: chipHover.hovered && enabled ? Qt.rgba(1, 1, 1, 0.12) : c.card
        border.width: on ? 0 : 1
        border.color: c.edge
        opacity: enabled ? 1 : 0.4
        SunsetFill { on: chip.on }
        scale: chipTap.pressed ? 0.95 : 1
        Behavior on scale { NumberAnimation { duration: 120 } }
        Text {
            id: chipText
            anchors.centerIn: parent
            text: chip.text
            color: c.text1
            font.family: Theme.fontUi
            font.pixelSize: 13
            font.weight: chip.on ? Font.DemiBold : Font.Normal
        }
        HoverHandler { id: chipHover; cursorShape: chip.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { id: chipTap; enabled: chip.enabled; onTapped: chip.clicked() }
    }

    component Seg: Rectangle {
        id: seg
        property var options: []
        property var current
        signal picked(var id)
        implicitHeight: 36
        radius: 18
        color: c.card
        border.width: 1
        border.color: c.edge
        Row {
            anchors.fill: parent
            anchors.margins: 3
            Repeater {
                model: seg.options
                delegate: Rectangle {
                    id: segItem
                    required property var modelData
                    readonly property bool on: seg.current === modelData.id
                    width: (seg.width - 6) / seg.options.length
                    height: parent.height
                    radius: height / 2
                    color: segHover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                    SunsetFill { on: segItem.on }
                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: on ? c.text1 : c.text2
                        font.family: Theme.fontUi
                        font.pixelSize: 13
                        font.weight: on ? Font.DemiBold : Font.Normal
                    }
                    HoverHandler { id: segHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: seg.picked(modelData.id) }
                }
            }
        }
    }

    component SwitchRow: RowLayout {
        id: sw
        property string text: ""
        property string hint: ""
        property bool checked: false
        signal toggled(bool on)
        Layout.fillWidth: true
        spacing: 12
        opacity: enabled ? 1 : 0.45
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                text: sw.text
                color: c.text1
                font.family: Theme.fontUi
                font.pixelSize: 13
            }
            Text {
                Layout.fillWidth: true
                visible: sw.hint.length > 0
                text: sw.hint
                color: c.text3
                font.family: Theme.fontUi
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
        Rectangle {
            implicitWidth: 44; implicitHeight: 26; radius: 13
            color: sw.checked ? c.pink : Qt.rgba(1, 1, 1, 0.16)
            Behavior on color { ColorAnimation { duration: 180 } }
            Rectangle {
                width: 20; height: 20; radius: 10
                anchors.verticalCenter: parent.verticalCenter
                x: sw.checked ? parent.width - width - 3 : 3
                color: "#FFFFFF"
                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
            }
            HoverHandler { cursorShape: sw.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
            TapHandler { enabled: sw.enabled; onTapped: sw.toggled(!sw.checked) }
        }
    }

    component ActionButton: Rectangle {
        id: ab
        property string text: ""
        property bool primary: false
        signal clicked()
        implicitHeight: 42
        implicitWidth: abText.implicitWidth + 32
        radius: 14
        color: abHover.hovered && enabled ? Qt.rgba(1, 1, 1, 0.14) : c.card
        SunsetFill { on: ab.primary }
        border.width: primary ? 0 : 1
        border.color: c.edge
        opacity: enabled ? 1 : 0.45
        scale: abTap.pressed ? 0.97 : 1
        Behavior on scale { NumberAnimation { duration: 120 } }
        Text {
            id: abText
            anchors.centerIn: parent
            text: ab.text
            color: c.text1
            font.family: Theme.fontUi
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        HoverHandler { id: abHover; cursorShape: ab.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { id: abTap; enabled: ab.enabled; onTapped: ab.clicked() }
    }

    // 30 to 360 in steps of 5; marks at the common targets and the screen's rate double as picks.
    component FpsSlider: Item {
        id: slider
        objectName: "push-play-slider"
        implicitHeight: 50
        readonly property int lo: 30
        readonly property int hi: 360
        property real knobX: (Math.min(play.fps, hi) - lo) / (hi - lo) * track.width
        Behavior on knobX { enabled: !drag.active; NumberAnimation { duration: Theme.reduced ? 0 : 200; easing.type: Easing.OutCubic } }
        readonly property bool screenMark: play.screenHz >= lo && play.screenHz <= hi
        readonly property var marks: [60, 120, 240].filter(m => !screenMark || Math.abs(m - play.screenHz) >= 40)
            .concat(screenMark ? [play.screenHz] : []).sort((a, b) => a - b)
        function valueAt(x) { return Math.round((lo + Math.max(0, Math.min(1, x / track.width)) * (hi - lo)) / 5) * 5 }
        function xOf(v) { return (v - lo) / (hi - lo) * track.width }
        Item {
            id: track
            x: 11; width: parent.width - 22
            y: 10; height: 6
            Rectangle { anchors.fill: parent; radius: 3; color: Qt.rgba(1, 1, 1, 0.12) }
            Rectangle { width: Math.max(6, slider.knobX); height: parent.height; radius: 3; gradient: sunsetH }
            Rectangle {
                x: slider.knobX - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 22; height: 22; radius: 11
                color: "#FFFFFF"
                scale: drag.active || tap.pressed ? 1.15 : 1
                Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutBack } }
                Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: c.pink }
            }
        }
        Repeater {
            model: slider.marks
            delegate: Text {
                id: mark
                required property int modelData
                readonly property bool on: play.fps === modelData
                x: track.x + slider.xOf(modelData) - width / 2
                y: 28
                text: modelData === play.screenHz ? modelData + " Hz" : modelData
                color: on ? c.gold : markHover.hovered ? c.text1 : c.text3
                font.family: Theme.fontUi
                font.pixelSize: 11
                font.weight: on ? Font.DemiBold : Font.Normal
                HoverHandler { id: markHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: play.setFps(mark.modelData) }
            }
        }
        Item {
            anchors.left: parent.left; anchors.right: parent.right
            height: 28
            activeFocusOnTab: true
            Keys.onLeftPressed: play.setFps(play.fps - 5)
            Keys.onRightPressed: play.setFps(play.fps + 5)
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { id: tap; onTapped: ev => play.setFps(slider.valueAt(ev.position.x - track.x)) }
            DragHandler {
                id: drag
                target: null
                xAxis.enabled: true; yAxis.enabled: false
                onCentroidChanged: if (active) play.setFps(slider.valueAt(centroid.position.x - track.x))
            }
        }
    }

    Gradient {
        id: sunsetH
        orientation: Gradient.Horizontal
        GradientStop { position: 0; color: c.pink }
        GradientStop { position: 1; color: c.orange }
    }
}
