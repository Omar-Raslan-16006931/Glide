// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Glide video playback mode (page "watch"; dock button 视频播放模式): Glide's tools on any video
// on the PC. Pick a window (a browser playing YouTube, a video app), a whole screen or a video file,
// then switch on smoother motion (frame generation), a sharper picture (super resolution), AI picture
// enhancement and HDR from SDR video. Every switch is an existing engine setting (the same ones as
// Professional mode); the window and screen capture are Veyra's (ScreenSourceDialog, WGC/DXGI).
// The picture shows on the right through Main.qml's native video window (videoViewport).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "EffectSupport.js" as Support

VPage {
    id: watch
    signal requestPage(string page)
    readonly property bool compact: width < 980
    readonly property int margin: 12
    readonly property int bodyTop: 56
    readonly property int panelWidth: compact ? Math.max(300, width - 2 * margin) : 400
    // Narrow windows: the controls only; the picture is a fullscreen away (F11).
    readonly property rect videoViewport: compact ? Qt.rect(0, 0, 0, 0)
        : Qt.rect(margin + panelWidth + 10 + 1, bodyTop + 36, Math.max(1, width - 2 * margin - panelWidth - 10 - 2), Math.max(1, height - bodyTop - margin - 36 - 1))

    // What to play: 0 a window, 1 a whole screen, 2 a video file.
    property int kind: Number(veyra.screenOptions.kind || 0) === 1 ? 1 : 0
    readonly property var o: veyra.screenOptions
    readonly property bool fgOk: Support.available(veyra, "frame-generation")
    readonly property bool srOk: Support.available(veyra, "sr")
    readonly property bool nrOk: Support.available(veyra, "nr")
    readonly property bool hdrOk: Support.available(veyra, "video-hdr")
    readonly property var multipliers: (veyra.fgMultiplierChoices || []).map(c => parseInt(c.id)).filter(m => m >= 2 && m <= 4)

    function pick(k) {
        watch.kind = k
        if (k < 2) veyra.setScreenOption("kind", k)
    }
    function start() {
        if (watch.kind === 2) { veyra.openFileDialog(); return }
        // A plain copy of the picture: no game mode, the whole window, fitted.
        if (watch.o.passthrough === true) veyra.setScreenOption("passthrough", false)
        if (veyra.startScreenCapture()) veyra.logUi("push-watch", "video playback started kind=" + watch.kind)
    }
    function setSmooth(on) {
        if (on) {
            const m = watch.multipliers.length ? watch.multipliers.reduce((a, b) => Math.min(a, b)) : 2
            if (Number(veyra.fgMultiplier) < 2) veyra.fgMultiplier = m
            veyra.fgEnabled = true
            // Videos repeat frames inside the screen's refresh: duplicate removal finds the real ones.
            if (Number(veyra.contentRate) === 0) veyra.contentRate = 1
        } else veyra.fgEnabled = false
    }
    function setEnhancement(on) {
        if (on) {
            if ((veyra.nrLayers || []).length === 0) veyra.addEffect("nr")
            else veyra.setAllNrEnabled(true)
        } else if ((veyra.nrLayers || []).length > 0) veyra.setAllNrEnabled(false)
    }
    onCurrentChanged: if (current) veyra.refreshCaptureTargets()

    RowLayout {
        x: watch.margin; y: 12; width: parent.width - 2 * watch.margin; height: 32
        spacing: 12
        VH2 { text: qsTr("视频播放模式") }
        Text {
            visible: !watch.compact
            text: qsTr("让电脑上的任何视频更流畅、更清晰：浏览器里的 YouTube、视频软件或本地文件")
            color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
    }

    Rectangle {
        objectName: "watch-panel"
        x: watch.margin; y: watch.bodyTop
        width: watch.panelWidth; height: watch.height - watch.bodyTop - watch.margin
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        clip: true
        Flickable {
            id: scroll
            anchors.fill: parent
            anchors.margins: 12
            contentWidth: width
            contentHeight: column.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: column
                width: scroll.width
                spacing: 10

                Text {
                    text: qsTr("1. 选择要播放的内容")
                    color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 14
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                VSeg {
                    objectName: "watch-kind"
                    Layout.fillWidth: true
                    options: [{ id: "0", label: qsTr("窗口") }, { id: "1", label: qsTr("整个屏幕") }, { id: "2", label: qsTr("视频文件") }]
                    current: String(watch.kind)
                    onPicked: id => watch.pick(Number(id))
                }
                RowLayout {
                    visible: watch.kind < 2
                    Layout.fillWidth: true
                    spacing: 6
                    VSelect {
                        objectName: "watch-target"
                        Layout.fillWidth: true
                        value: veyra.screenTargetLabel
                        options: veyra.screenTargets
                        onPicked: id => veyra.screenTargetId = id
                    }
                    VButton { text: qsTr("刷新"); ghost: true; onClicked: veyra.refreshCaptureTargets() }
                }
                Text {
                    Layout.fillWidth: true
                    text: watch.kind === 0 ? qsTr("YouTube 或网页视频：先在浏览器里播放，再在这里选浏览器窗口。浏览器窗口变黑时，请在浏览器设置里关闭硬件加速（受版权保护的视频可能仍然是黑的）")
                        : watch.kind === 1 ? qsTr("整块显示器的画面，适合全屏播放的视频。Glide 请放在另一块屏幕上")
                        : qsTr("打开电脑上的视频文件，补帧和画面增强同样生效")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    VButton {
                        objectName: "watch-start"
                        Layout.fillWidth: true
                        primary: true
                        iconName: watch.kind === 2 ? "folder" : "play"
                        text: watch.kind === 2 ? qsTr("打开视频文件") : qsTr("开始播放")
                        enabled: watch.kind === 2 || veyra.screenTargets.length > 0
                        onClicked: watch.start()
                    }
                    VButton {
                        objectName: "watch-stop"
                        visible: veyra.hasSource
                        ghost: true
                        text: qsTr("停止")
                        onClicked: veyra.stopPlayback()
                    }
                }

                Text {
                    Layout.topMargin: 8
                    text: qsTr("2. 让画面更好")
                    color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 14
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                VGroup {
                    VRow {
                        objectName: "watch-smooth-row"
                        label: qsTr("更流畅")
                        help: qsTr("补帧：在视频的帧之间生成新帧，24 或 30 FPS 的视频看起来像 60 或 120 FPS")
                        hint: !watch.fgOk ? Support.reason(veyra, "frame-generation") : veyra.fgEnabled ? qsTr("%1× 补帧").arg(veyra.fgMultiplier) : qsTr("补帧让动作更顺滑")
                        VSwitch {
                            objectName: "watch-smooth"
                            enabled: watch.fgOk
                            opacity: enabled ? 1 : 0.4
                            checked: veyra.fgEnabled === true
                            onToggled: checked => watch.setSmooth(checked)
                        }
                    }
                    VRow {
                        objectName: "watch-multiplier-row"
                        visible: veyra.fgEnabled === true && watch.multipliers.length > 1
                        label: qsTr("倍数")
                        VSeg {
                            objectName: "watch-multiplier"
                            options: watch.multipliers.map(m => ({ id: String(m), label: m + "×" }))
                            current: String(veyra.fgMultiplier)
                            onPicked: id => veyra.fgMultiplier = Number(id)
                        }
                    }
                    VRow {
                        objectName: "watch-sharp-row"
                        label: qsTr("更清晰")
                        help: qsTr("超分辨率：把低分辨率视频放大到屏幕分辨率，边缘和文字更锐利")
                        hint: !watch.srOk ? Support.reason(veyra, "sr") : qsTr("放大到屏幕分辨率")
                        VSwitch {
                            objectName: "watch-sharp"
                            enabled: watch.srOk
                            opacity: enabled ? 1 : 0.4
                            checked: veyra.srEnabled === true
                            onToggled: checked => veyra.srEnabled = checked
                        }
                    }
                    VRow {
                        objectName: "watch-enhance-row"
                        label: qsTr("AI 画面增强")
                        help: qsTr("用 AI 去掉压缩造成的色块和噪点，适合网络视频。更耗 GPU")
                        hint: !watch.nrOk ? Support.reason(veyra, "nr") : qsTr("去掉网络视频的色块和噪点")
                        VSwitch {
                            objectName: "watch-enhance"
                            enabled: watch.nrOk
                            opacity: enabled ? 1 : 0.4
                            checked: veyra.nrAnyEnabled === true
                            onToggled: checked => watch.setEnhancement(checked)
                        }
                    }
                    VRow {
                        objectName: "watch-hdr-row"
                        label: qsTr("SDR 转 HDR")
                        help: qsTr("把普通视频转换成 HDR，在 HDR 显示器上更明亮、颜色更丰富。需要 Windows HDR 开启")
                        hint: !watch.hdrOk ? Support.reason(veyra, "video-hdr") : qsTr("需要 HDR 显示器并开启 Windows HDR")
                        VSwitch {
                            objectName: "watch-hdr"
                            enabled: watch.hdrOk
                            opacity: enabled ? 1 : 0.4
                            checked: veyra.videoHdr === true
                            onToggled: checked => veyra.videoHdr = checked
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    text: qsTr("按 F11 全屏观看。更多设置在专业模式里")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    VButton { objectName: "watch-pro"; Layout.fillWidth: true; ghost: true; iconName: "sliders"; text: qsTr("专业模式"); onClicked: watch.requestPage("pro") }
                    VButton { objectName: "watch-record"; Layout.fillWidth: true; ghost: true; iconName: "video"; text: veyra.recording ? qsTr("停止录制") : qsTr("录制"); enabled: veyra.recording || veyra.hasSource; onClicked: veyra.toggleRecording("watch") }
                }
            }
        }
    }

    // The picture (native video window), with a hint while nothing plays.
    Rectangle {
        objectName: "watch-preview"
        visible: !watch.compact
        x: watch.margin + watch.panelWidth + 10; y: watch.bodyTop
        width: watch.width - 2 * watch.margin - watch.panelWidth - 10; height: watch.height - watch.bodyTop - watch.margin
        radius: Theme.rCard; color: Theme.videoBlack; border.color: Theme.stroke
        Text {
            x: 12; height: 36
            verticalAlignment: Text.AlignVCenter
            width: parent.width - 24
            text: veyra.hasSource ? (veyra.sourceTitle.length > 0 ? veyra.sourceTitle : qsTr("片源")) : qsTr("没有画面")
            color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
            elide: Text.ElideRight
        }
        Text {
            visible: !veyra.hasSource
            anchors.centerIn: parent
            width: parent.width - 60
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("在左边选择一个窗口、屏幕或视频文件，然后开始播放")
            color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
    }
}
