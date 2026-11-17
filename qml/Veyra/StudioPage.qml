// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide streaming mode (page "live"; dock button 直播模式), laid out like OBS Studio: the live
// preview in the middle, Glide's own features on the left (the quick setup: frame generation,
// adaptive multiplier, pacing repair, HUD lock, AI enhancement, lowest latency), the controls on the
// right (go live, record, instant replay, comparison clip, screenshot, outputs), the audio mixer
// across the bottom and a status bar under it.
//
// Audio mixer (engine/PushAudioMix.h, bridge PushStudio.cpp): game sound and microphone faders,
// mutes and level meters for what recordings and streams take, plus the monitor volume (what you
// hear yourself, Veyra's own volume). The microphone's gear opens its filters (PushMicFiltersDialog:
// rumble filter, EQ, noise gate, compressor). While this page is on screen the meters keep the
// game sound tap open, and the microphone too when recordings or streams use it.
// Geometry: StudioLayout.js, also used by Main.qml to place the native video window.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "StudioLayout.js" as StudioGeometry

VPage {
    id: studio
    signal requestPage(string page)
    readonly property var geometry: StudioGeometry.calculate(width, height)
    readonly property bool compact: geometry.compact
    readonly property rect videoViewport: Qt.rect(geometry.video.x, geometry.video.y, geometry.video.width, geometry.video.height)

    component FieldLabel: Text { color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11 }
    component StatusText: Text {
        color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 11
    }

    // --- live state, polled while the page is on screen ------------------------------------
    property var stream: ({})
    property var levels: ({})
    property var stats: ({})
    readonly property bool live: stream.active === true
    readonly property bool recordingNow: veyra.recording === true
    readonly property bool micUsed: veyra.recordingMicrophone === true || veyra.preferences.streamMic === true
    // Shown meter levels with OBS-like fall-off (StudioLayout.fall).
    property real gameRms: -100
    property real gamePeak: -100
    property real micRms: -100
    property real micPeak: -100
    property double lastTick: 0

    function poll() {
        const now = Date.now()
        const dt = studio.lastTick > 0 ? Math.min(0.5, (now - studio.lastTick) / 1000) : 0.05
        studio.lastTick = now
        const l = veyra.pushStudioLevels() || ({})
        studio.levels = l
        studio.gameRms = StudioGeometry.fall(studio.gameRms, Number(l.gameRmsDb ?? -100), dt, 24)
        studio.gamePeak = StudioGeometry.fall(studio.gamePeak, Number(l.gamePeakDb ?? -100), dt, 24)
        studio.micRms = StudioGeometry.fall(studio.micRms, Number(l.micRmsDb ?? -100), dt, 24)
        studio.micPeak = StudioGeometry.fall(studio.micPeak, Number(l.micPeakDb ?? -100), dt, 24)
    }
    function pollSlow() {
        studio.stream = veyra.pushStreamStatus() || ({})
        studio.stats = veyra.pushLiveStats() || ({})
    }
    function meters(on) {
        veyra.pushSetStudioMeters(on)
        if (!on) { studio.gameRms = studio.gamePeak = studio.micRms = studio.micPeak = -100 }
    }
    onCurrentChanged: {
        if (current) { veyra.refreshMicrophones(); studio.pollSlow(); studio.lastTick = 0 }
        studio.meters(current)
    }
    // The microphone meter follows whether recordings or streams use it, and the chosen device.
    onMicUsedChanged: if (current) studio.meters(true)
    Connections {
        target: veyra
        function onPreferencesChanged() {
            const id = veyra.recordingMicrophoneId
            if (studio.current && studio.micUsed && id !== studio.meterDevice) { studio.meterDevice = id; studio.meters(false); studio.meters(true) }
        }
    }
    property string meterDevice: veyra.recordingMicrophoneId
    Timer { interval: 50; repeat: true; running: studio.current; onTriggered: studio.poll() }
    Timer { interval: 1000; repeat: true; running: studio.current; onTriggered: studio.pollSlow() }

    function toggleMicRecording() { veyra.setPreference("recordMic", !veyra.recordingMicrophone); veyra.refreshMicrophones() }
    function toggleMicStreaming() { veyra.setPreference("streamMic", !(veyra.preferences.streamMic === true)) }
    // --- scenes (PushStreamOverlay.h): what recordings and streams carry ------------------------
    // pref studioScenes: [{id, name, type: "game"|"card", title, subtitle, image, overlayImage,
    // overlayText, corner, muteGame}]; the first game scene cannot be removed. Ctrl+1..9 switch.
    readonly property var defaultScenes: [
        { id: "game", name: qsTr("游戏"), type: "game", overlayImage: "", overlayText: "", corner: "tr" },
        { id: "soon", name: qsTr("即将开始"), type: "card", title: qsTr("即将开始"), subtitle: "", image: "", muteGame: true },
        { id: "brb", name: qsTr("马上回来"), type: "card", title: qsTr("马上回来"), subtitle: "", image: "", muteGame: true },
        { id: "end", name: qsTr("感谢观看"), type: "card", title: qsTr("感谢观看"), subtitle: "", image: "", muteGame: true }
    ]
    readonly property var scenes: {
        let list = null
        try { list = JSON.parse(String(veyra.preferences.studioScenes || "")) } catch (e) { list = null }
        return Array.isArray(list) && list.length > 0 && list.some(x => x.type === "game") ? list : studio.defaultScenes
    }
    property string activeScene: "game"
    property bool muteBeforeCard: false
    property string editingScene: ""
    function sceneById(id) { return studio.scenes.find(x => x.id === id) || null }
    function saveScenes(list) { veyra.setPreference("studioScenes", JSON.stringify(list)) }
    function activate(id) {
        const scene = studio.sceneById(id)
        if (!scene) return
        const wasCard = (studio.sceneById(studio.activeScene) || {}).type === "card"
        if (scene.type === "card" && !wasCard) studio.muteBeforeCard = veyra.preferences.mixGameMuted === true
        veyra.pushSetStreamScene(scene)
        if (scene.type === "card") veyra.setPreference("mixGameMuted", scene.muteGame !== false ? true : studio.muteBeforeCard)
        else if (wasCard) veyra.setPreference("mixGameMuted", studio.muteBeforeCard)
        studio.activeScene = id
        veyra.logUi("push-studio", "scene " + id + " (" + scene.type + ")")
    }
    function addScene(type) {
        const list = studio.scenes.slice()
        if (list.length >= 12) return
        const id = "s" + Date.now().toString(36)
        list.push(type === "card" ? { id: id, name: qsTr("新场景"), type: "card", title: qsTr("新场景"), subtitle: "", image: "", muteGame: true }
                                  : { id: id, name: qsTr("新场景"), type: "game", overlayImage: "", overlayText: "", corner: "tr" })
        studio.saveScenes(list)
        studio.editingScene = id
    }
    function updateScene(id, changes) {
        const list = studio.scenes.map(x => x.id === id ? Object.assign({}, x, changes) : x)
        studio.saveScenes(list)
        if (id === studio.activeScene) Qt.callLater(() => studio.activate(id))
    }
    function removeScene(id) {
        const games = studio.scenes.filter(x => x.type === "game")
        const target = studio.sceneById(id)
        if (!target || (target.type === "game" && games.length <= 1)) return false
        if (id === studio.activeScene) studio.activate(games[0].id)
        studio.saveScenes(studio.scenes.filter(x => x.id !== id))
        return true
    }
    // Starts on the game scene: a card left on from a previous session never surprises a new stream.
    Component.onCompleted: Qt.callLater(() => { const g = studio.scenes.find(x => x.type === "game"); if (g) veyra.pushSetStreamScene(g) })
    Repeater {
        model: 9
        delegate: Item {
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                enabled: studio.current && index < studio.scenes.length
                onActivated: studio.activate(studio.scenes[index].id)
            }
        }
    }

    function goLive() {
        if (studio.live) { veyra.pushStopStream(); studio.pollSlow(); return }
        let targets = []
        try { targets = JSON.parse(String(veyra.preferences.streamTargets || "[]")) } catch (e) { targets = [] }
        if (!Array.isArray(targets) || targets.filter(t => t.enabled !== false).length === 0) { veyra.navigate("stream"); return }
        veyra.pushStartStream()
        studio.pollSlow()
    }
    readonly property real streamKbps: {
        const outs = studio.stream.outputs || []
        let sum = 0
        for (const o of outs) sum += Number(o.kbps || 0)
        return sum
    }
    readonly property string sourceText: veyra.hasSource ? (veyra.sourceTitle.length > 0 ? veyra.sourceTitle : qsTr("片源")) : qsTr("没有画面")

    // --- header ----------------------------------------------------------------------------
    RowLayout {
        id: head
        x: 12; y: 12; width: parent.width - 24; height: 32
        spacing: 12
        VH2 { text: qsTr("直播模式") }
        Text {
            visible: !studio.compact
            text: qsTr("直播和录制 Glide 的画面，混音、开播、录像都在这一页")
            color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        // On-air and recording lights, as OBS shows them.
        Rectangle {
            objectName: "studio-live-badge"
            visible: studio.live
            implicitWidth: liveText.implicitWidth + 26; implicitHeight: 24; radius: 12
            color: Theme.errGlow; border.width: 1; border.color: Theme.err
            Rectangle { x: 9; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: Theme.err }
            Text {
                id: liveText
                x: 20; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("直播中 %1").arg(StudioGeometry.duration(studio.stream.seconds))
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
            }
        }
        Rectangle {
            objectName: "studio-rec-badge"
            visible: studio.recordingNow
            implicitWidth: recText.implicitWidth + 26; implicitHeight: 24; radius: 12
            color: Theme.ink(0.06); border.width: 1; border.color: Theme.err
            Rectangle { x: 9; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: Theme.err }
            Text {
                id: recText
                x: 20; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("录制 %1").arg(StudioGeometry.duration(veyra.recordingState.seconds))
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
            }
        }
    }

    // --- left: Glide features --------------------------------------------------------------
    Rectangle {
        objectName: "studio-features"
        visible: !studio.compact
        x: studio.geometry.left.x; y: studio.geometry.left.y
        width: studio.geometry.left.width; height: studio.geometry.left.height
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        clip: true
        Flickable {
            id: featureScroll
            anchors.fill: parent
            anchors.margins: 10
            contentWidth: width
            contentHeight: featureColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: featureColumn
                width: featureScroll.width
                spacing: 10
                PushQuickSetup {
                    Layout.fillWidth: true
                    compact: true
                    compactPro: true
                }
            }
        }
    }

    // --- middle: the preview ----------------------------------------------------------------
    Rectangle {
        objectName: "studio-preview"
        x: studio.geometry.preview.x; y: studio.geometry.preview.y
        width: studio.geometry.preview.width; height: studio.geometry.preview.height
        radius: Theme.rCard; color: Theme.videoBlack; border.color: Theme.stroke
        RowLayout {
            x: 12; width: parent.width - 24; height: studio.geometry.headerHeight
            spacing: 8
            VIcon {
                name: veyra.sourceKind === "ps5" || veyra.sourceKind === "xbox" ? "gamepad" : veyra.sourceKind === "moonlight" ? "cast" : "video"
                size: 14; color: Theme.t2
            }
            Text {
                Layout.fillWidth: true
                text: studio.sourceText
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                elide: Text.ElideRight
            }
            Text {
                visible: veyra.hasSource
                text: {
                    const out = Number(studio.stats.submitFps || 0), game = Number(studio.stats.contentFps || 0)
                    return (game > 0 ? qsTr("游戏 %1 FPS").arg(Math.round(game)) + " → " : "") + (out > 0 ? qsTr("%1 FPS").arg(Math.round(out)) : "")
                }
                color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 11
            }
        }
        // Nothing playing: where to start. The native video window covers this area once a picture runs.
        ColumnLayout {
            visible: !veyra.hasSource
            anchors.centerIn: parent
            anchors.verticalCenterOffset: studio.geometry.headerHeight / 2
            spacing: 12
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("先连接画面，再开始直播或录制")
                color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 13
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                VButton { objectName: "studio-open-capture"; primary: true; iconName: "video"; text: qsTr("采集卡"); onClicked: veyra.navigate("capture") }
                VButton { iconName: "gamepad"; text: "PS5"; onClicked: veyra.navigate("ps5") }
                VButton { iconName: "gamepad"; text: "Xbox"; onClicked: veyra.navigate("xbox") }
                VButton { iconName: "cast"; text: "Moonlight"; onClicked: veyra.navigate("moonlight") }
            }
        }
    }

    // --- right: controls ------------------------------------------------------------------
    Rectangle {
        objectName: "studio-controls"
        x: studio.geometry.right.x; y: studio.geometry.right.y
        width: studio.geometry.right.width; height: studio.geometry.right.height
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        clip: true
        Flickable {
            id: controlScroll
            anchors.fill: parent
            anchors.margins: 12
            contentWidth: width
            contentHeight: controls.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: controls
                width: controlScroll.width
                spacing: 8
                Text {
                    text: qsTr("场景")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                Repeater {
                    model: studio.scenes
                    delegate: Rectangle {
                        id: sceneRow
                        required property var modelData
                        required property int index
                        objectName: "studio-scene-" + modelData.id
                        readonly property bool active: studio.activeScene === modelData.id
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: 8
                        color: active ? Theme.errGlow : sceneHover.hovered ? Theme.ink(0.07) : Theme.ink(0.03)
                        border.width: 1
                        border.color: active ? Theme.err : Theme.stroke
                        HoverHandler { id: sceneHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: studio.activate(sceneRow.modelData.id) }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 4
                            spacing: 8
                            VIcon { name: sceneRow.modelData.type === "card" ? "image" : "gamepad"; size: 13; color: sceneRow.active ? Theme.err : Theme.t2 }
                            Text {
                                Layout.fillWidth: true
                                text: sceneRow.modelData.name
                                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                visible: sceneRow.index < 9
                                text: "Ctrl+" + (sceneRow.index + 1)
                                color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 10
                            }
                            VButton {
                                icon: true; ghost: true; iconName: "settings"
                                tip: qsTr("编辑场景")
                                onClicked: studio.editingScene = studio.editingScene === sceneRow.modelData.id ? "" : sceneRow.modelData.id
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    VButton { objectName: "studio-add-game-scene"; Layout.fillWidth: true; ghost: true; iconName: "plus"; text: qsTr("画面场景"); onClicked: studio.addScene("game") }
                    VButton { objectName: "studio-add-card-scene"; Layout.fillWidth: true; ghost: true; iconName: "plus"; text: qsTr("图卡场景"); onClicked: studio.addScene("card") }
                }
                // The scene editor, under the list while a scene is being edited.
                Rectangle {
                    id: editor
                    objectName: "studio-scene-editor"
                    readonly property var scene: studio.sceneById(studio.editingScene)
                    visible: scene !== null
                    Layout.fillWidth: true
                    implicitHeight: editorColumn.implicitHeight + 20
                    radius: 8
                    color: Theme.ink(0.04)
                    border.width: 1
                    border.color: Theme.stroke2
                    ColumnLayout {
                        id: editorColumn
                        x: 10; y: 10; width: parent.width - 20
                        spacing: 6
                        FieldLabel { text: qsTr("名称") }
                        DialogTextField {
                            objectName: "scene-name"
                            Layout.fillWidth: true
                            text: editor.scene ? editor.scene.name : ""
                            onEdited: t => { if (editor.scene && t.trim().length) studio.updateScene(editor.scene.id, { name: t.trim().slice(0, 40) }) }
                        }
                        FieldLabel { visible: editor.scene && editor.scene.type === "card"; text: qsTr("标题") }
                        DialogTextField {
                            objectName: "scene-title"
                            visible: editor.scene && editor.scene.type === "card"
                            Layout.fillWidth: true
                            text: editor.scene ? (editor.scene.title || "") : ""
                            onEdited: t => { if (editor.scene) studio.updateScene(editor.scene.id, { title: t.slice(0, 60) }) }
                        }
                        FieldLabel { visible: editor.scene && editor.scene.type === "card"; text: qsTr("副标题") }
                        DialogTextField {
                            objectName: "scene-subtitle"
                            visible: editor.scene && editor.scene.type === "card"
                            Layout.fillWidth: true
                            text: editor.scene ? (editor.scene.subtitle || "") : ""
                            onEdited: t => { if (editor.scene) studio.updateScene(editor.scene.id, { subtitle: t.slice(0, 120) }) }
                        }
                        FieldLabel { visible: editor.scene && editor.scene.type === "game"; text: qsTr("角标文字（例如频道名）") }
                        DialogTextField {
                            objectName: "scene-overlay-text"
                            visible: editor.scene && editor.scene.type === "game"
                            Layout.fillWidth: true
                            text: editor.scene ? (editor.scene.overlayText || "") : ""
                            onEdited: t => { if (editor.scene) studio.updateScene(editor.scene.id, { overlayText: t.slice(0, 80) }) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            VButton {
                                objectName: "scene-pick-image"
                                Layout.fillWidth: true
                                ghost: true
                                iconName: "image"
                                readonly property string current: editor.scene ? String(editor.scene.type === "card" ? (editor.scene.image || "") : (editor.scene.overlayImage || "")) : ""
                                text: current.length ? current.split(/[\\/]/).pop() : (editor.scene && editor.scene.type === "card" ? qsTr("背景图片") : qsTr("角标图片"))
                                maxTextWidth: 150
                                onClicked: {
                                    const path = veyra.pushPickImage()
                                    if (path && editor.scene) studio.updateScene(editor.scene.id, editor.scene.type === "card" ? { image: path } : { overlayImage: path })
                                }
                            }
                            VButton {
                                icon: true; ghost: true; iconName: "x"
                                tip: qsTr("移除图片")
                                onClicked: if (editor.scene) studio.updateScene(editor.scene.id, editor.scene.type === "card" ? { image: "" } : { overlayImage: "" })
                            }
                        }
                        VSeg {
                            objectName: "scene-corner"
                            visible: editor.scene && editor.scene.type === "game"
                            options: [{ id: "tl", label: qsTr("左上") }, { id: "tr", label: qsTr("右上") }, { id: "bl", label: qsTr("左下") }, { id: "br", label: qsTr("右下") }]
                            current: editor.scene ? (editor.scene.corner || "tr") : "tr"
                            onPicked: id => { if (editor.scene) studio.updateScene(editor.scene.id, { corner: id }) }
                        }
                        RowLayout {
                            visible: editor.scene && editor.scene.type === "card"
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: qsTr("切换到此场景时静音游戏声音"); color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            VSwitch {
                                objectName: "scene-mute"
                                checked: editor.scene ? editor.scene.muteGame !== false : true
                                onToggled: v => { if (editor.scene) studio.updateScene(editor.scene.id, { muteGame: v }) }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            VButton { objectName: "scene-delete"; ghost: true; text: qsTr("删除"); onClicked: { if (editor.scene && studio.removeScene(editor.scene.id)) studio.editingScene = "" } }
                            Item { Layout.fillWidth: true }
                            VButton { objectName: "scene-done"; primary: true; text: qsTr("完成"); onClicked: studio.editingScene = "" }
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("场景只改变录制和直播的画面；你自己看到的仍是游戏")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.topMargin: 6
                    text: qsTr("控制")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                VButton {
                    objectName: "studio-go-live"
                    Layout.fillWidth: true
                    primary: !studio.live
                    iconName: "cast"
                    text: studio.live ? qsTr("停止直播") : qsTr("开始直播")
                    enabled: studio.live || veyra.hasSource
                    onClicked: studio.goLive()
                }
                VButton {
                    objectName: "studio-record"
                    Layout.fillWidth: true
                    iconName: "video"
                    text: studio.recordingNow ? qsTr("停止录制") : qsTr("开始录制")
                    enabled: studio.recordingNow || veyra.hasSource
                    onClicked: veyra.toggleRecording("studio")
                }
                VButton {
                    objectName: "studio-replay"
                    Layout.fillWidth: true
                    iconName: "clock"
                    text: veyra.replayEnabled ? qsTr("保存即时回放（%1 秒）").arg(veyra.replaySeconds) : qsTr("开启即时回放")
                    onClicked: veyra.replayEnabled ? veyra.saveReplay("studio") : veyra.setPreference("replayEnabled", true)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    VButton { objectName: "studio-compare"; Layout.fillWidth: true; iconName: "split"; text: qsTr("对比片段"); onClicked: veyra.navigate("compare") }
                    VButton { objectName: "studio-screenshot"; icon: true; iconName: "camera"; tip: qsTr("截图"); enabled: veyra.hasSource; onClicked: veyra.takeScreenshot() }
                }
                // One per row: side by side their labels were cut off in the narrow column.
                VButton { objectName: "studio-outputs"; Layout.fillWidth: true; ghost: true; iconName: "settings"; text: qsTr("直播平台"); onClicked: veyra.navigate("stream") }
                VButton { objectName: "studio-folder"; Layout.fillWidth: true; ghost: true; iconName: "folder"; text: qsTr("录制文件夹"); onClicked: veyra.openRecordingDirectory() }

                // Outputs and their health (one line per platform while live).
                Text {
                    Layout.topMargin: 6
                    text: qsTr("直播输出")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                Text {
                    visible: (studio.stream.outputs || []).length === 0
                    Layout.fillWidth: true
                    text: studio.live ? qsTr("正在连接…") : qsTr("开播后这里显示每个平台的状态和码率")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: studio.stream.outputs || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: modelData.state === "live" ? Theme.ok : modelData.state === "failed" ? Theme.err : Theme.warn
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            text: Math.round(Number(modelData.kbps || 0)) + " kbps"
                            color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 11
                        }
                    }
                }
                Text {
                    visible: studio.stream.failed === true && String(studio.stream.error || "").length > 0
                    Layout.fillWidth: true
                    text: String(studio.stream.error || "")
                    color: Theme.err; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    // --- bottom: the audio mixer ----------------------------------------------------------
    Rectangle {
        objectName: "studio-mixer"
        x: studio.geometry.mixer.x; y: studio.geometry.mixer.y
        width: studio.geometry.mixer.width; height: studio.geometry.mixer.height
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("音频混音器")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                Text {
                    Layout.fillWidth: true
                    visible: !studio.compact
                    text: qsTr("游戏声音和麦克风的推子只影响录制和直播；监听只改变你听到的声音")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
            PushMixerStrip {
                objectName: "mixer-game"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("游戏声音")
                subtitle: qsTr("采集卡或串流")
                iconName: "gamepad"
                from: -60; to: 12
                value: Number(veyra.preferences.mixGameDb ?? 0)
                muted: veyra.preferences.mixGameMuted === true
                rmsDb: studio.gameRms; peakDb: studio.gamePeak
                meterActive: veyra.hasSource
                gear: true
                gearTip: qsTr("音频设置")
                onMoved: v => veyra.setPreference("mixGameDb", Math.round(v))
                onMuteToggled: veyra.setPreference("mixGameMuted", !(veyra.preferences.mixGameMuted === true))
                onGearClicked: veyra.navigate("audio")
            }
            PushMixerStrip {
                objectName: "mixer-mic"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("麦克风")
                subtitle: !studio.micUsed ? qsTr("未用于录制或直播")
                        : studio.levels.micError ? String(studio.levels.micError)
                        : String(studio.levels.micDevice || qsTr("系统默认麦克风"))
                iconName: "activity"
                from: -60; to: 20
                value: Number(veyra.preferences.mixMicDb ?? 0)
                muted: veyra.preferences.mixMicMuted === true
                rmsDb: studio.micRms; peakDb: studio.micPeak
                meterActive: studio.micUsed && studio.levels.micOpen === true
                gear: true
                gearTip: qsTr("麦克风滤镜（均衡器、噪声门、压缩）")
                onMoved: v => veyra.setPreference("mixMicDb", Math.round(v))
                onMuteToggled: veyra.setPreference("mixMicMuted", !(veyra.preferences.mixMicMuted === true))
                onGearClicked: veyra.navigate("micfilters")
                // Where the microphone goes: recordings (recordMic) and streams (streamMic).
                VTag {
                    objectName: "mixer-mic-record"
                    text: qsTr("录制")
                    kind: veyra.recordingMicrophone ? "ok" : ""
                    opacity: veyra.recordingMicrophone ? 1 : 0.6
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: studio.toggleMicRecording() }
                }
                VTag {
                    objectName: "mixer-mic-stream"
                    text: qsTr("直播")
                    kind: veyra.preferences.streamMic === true ? "ok" : ""
                    opacity: veyra.preferences.streamMic === true ? 1 : 0.6
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: studio.toggleMicStreaming() }
                }
            }
            PushMixerStrip {
                objectName: "mixer-monitor"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("监听")
                subtitle: qsTr("只改变你听到的声音")
                iconName: "vol"
                decibels: false
                from: 0; to: 1
                value: Number(veyra.volume ?? 1)
                muted: veyra.muted === true
                meterActive: false
                onMoved: v => veyra.volume = v
                onMuteToggled: veyra.muted = !veyra.muted
            }
        }
    }

    // --- status bar -----------------------------------------------------------------------
    RowLayout {
        objectName: "studio-status"
        x: studio.geometry.status.x; y: studio.geometry.status.y
        width: studio.geometry.status.width; height: studio.geometry.status.height
        spacing: 16
        StatusText {
            objectName: "studio-status-live"
            color: studio.live ? Theme.t1 : Theme.t3
            text: studio.live ? qsTr("直播 %1 · %2 Mbps · 丢帧 %3").arg(StudioGeometry.duration(studio.stream.seconds))
                                    .arg((studio.streamKbps / 1000).toFixed(1)).arg(Number(studio.stream.dropped || 0))
                              : qsTr("直播 未开始")
        }
        StatusText {
            color: studio.recordingNow ? Theme.t1 : Theme.t3
            text: studio.recordingNow ? qsTr("录制 %1 · 丢帧 %2").arg(StudioGeometry.duration(veyra.recordingState.seconds)).arg(Number(veyra.recordingState.dropped || 0))
                                      : qsTr("录制 未开始")
        }
        StatusText {
            visible: Number(studio.stats.latencyMs || 0) > 0
            text: qsTr("处理延迟 %1 ms").arg(Number(studio.stats.latencyMs || 0).toFixed(0))
        }
        Item { Layout.fillWidth: true }
        StatusText {
            visible: String(studio.stream.encoder || veyra.recordingState.encoder || "").length > 0
            text: String(studio.stream.encoder || veyra.recordingState.encoder || "")
        }
    }
}
