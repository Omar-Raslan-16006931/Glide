// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Going live: the Glide picture (frame generation included) and sound straight to Twitch, YouTube,
// Kick, TikTok, Facebook, Trovo or any RTMP server, several at once from one encode
// (engine/LiveStream.h). Outputs are kept in the pref streamTargets; stream keys go to
// pushSetStreamKey (encrypted with Windows DPAPI) and are never shown again. Server lists and
// recommended limits follow OBS Studio's rtmp-services.
import QtQuick
import QtQuick.Layouts

DialogLayer {
    id: live
    objectName: "pushfps-stream-dialog"
    key: "stream"
    glyph: "cast"
    title: qsTr("直播")
    sub: qsTr("把 Glide 的画面（含补帧）和声音直接推到 Twitch、YouTube、Kick、TikTok 等平台，可以同时推多个平台")
    dialogWidth: 720

    readonly property var platforms: veyra.pushStreamPlatforms() || []
    property var targets: []          // [{id, platform, name, server, enabled}]
    property var status: ({})
    property int keyRevision: 0       // bumps after a key is saved or forgotten
    readonly property bool streaming: status.active === true
    readonly property int bitrate: Number(veyra.preferences.streamBitrateMbps || 6)
    readonly property int enabledCount: targets.filter(t => t.enabled !== false).length

    function platformOf(id) { return platforms.find(p => p.id === id) || platforms[platforms.length - 1] || ({ servers: [] }) }
    function load() {
        let list = []
        try { list = JSON.parse(String(veyra.preferences.streamTargets || "[]")) } catch (e) { list = [] }
        live.targets = Array.isArray(list) ? list : []
    }
    function save(list) {
        if (veyra.setPreference("streamTargets", JSON.stringify(list))) live.targets = list
        else live.load()
    }
    function newId() { return "o" + Date.now().toString(36) + Math.floor(Math.random() * 1e6).toString(36) }
    function add() {
        if (live.targets.length >= 8) return
        const used = live.targets.map(t => t.platform)
        const pick = ["twitch", "youtube", "kick", "tiktok"].find(p => used.indexOf(p) < 0) || "custom"
        const list = live.targets.slice()
        list.push({ id: live.newId(), platform: pick, name: "", server: "", enabled: true })
        live.save(list)
    }
    function update(index, changes) {
        const list = live.targets.map((t, i) => i === index ? Object.assign({}, t, changes) : t)
        live.save(list)
    }
    function remove(index) {
        veyra.pushForgetStreamKey(live.targets[index].id)
        live.keyRevision++
        live.save(live.targets.filter((t, i) => i !== index))
    }
    function stateLabel(s) {
        return s === "live" ? qsTr("直播中") : s === "connecting" ? qsTr("正在连接") : s === "reconnecting" ? qsTr("重新连接中")
             : s === "failed" ? qsTr("连接失败") : s === "stopped" ? qsTr("已停止") : qsTr("未开始")
    }
    function duration(seconds) {
        const s = Math.max(0, Math.floor(seconds || 0))
        const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, r = s % 60
        return (h > 0 ? h + ":" + String(m).padStart(2, "0") : String(m)) + ":" + String(r).padStart(2, "0")
    }

    onShownChanged: if (shown) { load(); status = veyra.pushStreamStatus() || ({}) }
    Timer {
        interval: 1000; repeat: true
        running: live.shown || live.streaming
        onTriggered: live.status = veyra.pushStreamStatus() || ({})
    }

    // The upload note sits in the body, where it wraps; the footer row cut it off at "about 12.4 ...".
    readonly property string uploadNote: qsTr("服务器列表和推荐设置来自 OBS Studio。每个平台各自上传一份，需要的上传带宽约为 %1 Mbps").arg(Number(((live.bitrate + 0.2) * Math.max(1, live.enabledCount)).toFixed(1)))
    actions: live.streaming ? [{ label: qsTr("关闭") }, { label: qsTr("停止直播"), primary: true }]
                            : [{ label: qsTr("关闭") }, { label: qsTr("开播"), primary: true }]
    onActionTriggered: label => {
        if (label === qsTr("开播")) { veyra.pushStartStream(); live.status = veyra.pushStreamStatus() || ({}) }
        else if (label === qsTr("停止直播")) { veyra.pushStopStream(); live.status = veyra.pushStreamStatus() || ({}) }
        else live.dialogHost.close()
    }

    // Status while live.
    Text {
        objectName: "pushfps-stream-status"
        Layout.fillWidth: true
        visible: live.streaming || String(live.status.error || "").length > 0
        text: live.streaming ? (live.status.live ? qsTr("直播中 · %1").arg(live.duration(live.status.seconds)) : qsTr("正在连接平台…"))
                             : String(live.status.error || "")
        color: live.streaming ? (live.status.live ? Theme.accent : Theme.t1) : Theme.warn
        font.family: Theme.fontUi; font.pixelSize: 14; font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }
    Repeater {
        model: live.streaming ? (live.status.outputs || []) : []
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 8
            Rectangle { width: 8; height: 8; radius: 4; color: modelData.state === "live" ? Theme.accent : modelData.state === "failed" ? Theme.err : Theme.warn }
            Text { text: modelData.name; color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12; Layout.preferredWidth: 120; elide: Text.ElideRight }
            Text {
                Layout.fillWidth: true
                text: live.stateLabel(modelData.state)
                      + (modelData.state === "live" ? " · " + Math.round(modelData.kbps) + " kbps" : "")
                      + (modelData.framesDropped > 0 ? " · " + qsTr("丢帧 %1").arg(modelData.framesDropped) : "")
                      + (modelData.reconnects > 0 ? " · " + qsTr("重连 %1 次").arg(modelData.reconnects) : "")
                      + (modelData.state !== "live" && modelData.error ? " · " + modelData.error : "")
                color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 11; elide: Text.ElideRight
            }
        }
    }

    // Outputs.
    Repeater {
        model: live.targets
        delegate: DialogGroup {
            id: row
            required property var modelData
            required property int index
            readonly property var platform: live.platformOf(modelData.platform)
            readonly property bool hasKey: live.keyRevision >= 0 && veyra.pushHasStreamKey(modelData.id)
            Layout.fillWidth: true
            VRow {
                label: qsTr("平台")
                RowLayout {
                    spacing: 8
                    VSelect {
                        objectName: "pushfps-stream-platform-" + row.index
                        implicitWidth: 170
                        options: live.platforms.map(p => ({ id: p.id, label: p.id === "custom" ? qsTr("自定义 RTMP") : p.name }))
                        value: row.platform.id === "custom" ? qsTr("自定义 RTMP") : row.platform.name
                        enabled: !live.streaming
                        onPicked: id => live.update(row.index, { platform: id, server: "" })
                    }
                    VSwitch {
                        objectName: "pushfps-stream-enabled-" + row.index
                        checked: row.modelData.enabled !== false
                        enabled: !live.streaming
                        onToggled: checked => live.update(row.index, { enabled: checked })
                    }
                    VButton {
                        ghost: true; icon: true; iconName: "trash"; tip: qsTr("删除")
                        enabled: !live.streaming
                        onClicked: live.remove(row.index)
                    }
                }
            }
            VRow {
                label: qsTr("服务器")
                hint: row.platform.servers.length === 0 ? qsTr("粘贴平台给你的服务器地址（rtmp:// 或 rtmps://）") : ""
                RowLayout {
                spacing: 0
                VSelect {
                    visible: row.platform.servers.length > 0
                    objectName: "pushfps-stream-server-" + row.index
                    implicitWidth: 260
                    options: row.platform.servers.map(s => ({ id: s.url, label: s.name }))
                    value: (row.platform.servers.find(s => s.url === row.modelData.server) || row.platform.servers[0] || {}).name || ""
                    enabled: !live.streaming
                    onPicked: id => live.update(row.index, { server: id })
                }
                DialogTextField {
                    visible: row.platform.servers.length === 0
                    objectName: "pushfps-stream-url-" + row.index
                    implicitWidth: 300
                    placeholder: "rtmps://…"
                    text: row.modelData.server || ""
                    enabled: !live.streaming
                    onEdited: value => { if (value !== (row.modelData.server || "")) live.update(row.index, { server: value.trim() }) }
                }
                }
            }
            VRow {
                label: qsTr("串流密钥")
                hint: row.hasKey ? qsTr("已加密保存在这台电脑上，不会再显示")
                                 : (row.platform.keyPage ? qsTr("在这里找到：%1").arg(row.platform.keyPage) : "")
                RowLayout {
                    spacing: 8
                    Rectangle {
                        implicitWidth: 220; implicitHeight: 30; radius: 9
                        color: Theme.ink(0.04)
                        border.width: 1; border.color: keyInput.activeFocus ? Theme.accent : Theme.stroke
                        TextInput {
                            id: keyInput
                            objectName: "pushfps-stream-key-" + row.index
                            anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                            verticalAlignment: TextInput.AlignVCenter
                            echoMode: TextInput.Password
                            color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: Theme.fsBody
                            enabled: !live.streaming
                            onAccepted: saveKey.clicked()
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10
                            visible: keyInput.text.length === 0
                            text: row.hasKey ? "••••••••" : qsTr("粘贴串流密钥")
                            color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: Theme.fsBody
                        }
                    }
                    VButton {
                        id: saveKey
                        objectName: "pushfps-stream-save-key-" + row.index
                        text: qsTr("保存")
                        enabled: keyInput.text.trim().length > 0 && !live.streaming
                        onClicked: {
                            if (veyra.pushSetStreamKey(row.modelData.id, keyInput.text)) { keyInput.text = ""; live.keyRevision++ }
                        }
                    }
                }
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        VButton {
            objectName: "pushfps-stream-add"
            text: qsTr("添加平台")
            iconName: "plus"
            enabled: live.targets.length < 8 && !live.streaming
            onClicked: live.add()
        }
        Item { Layout.fillWidth: true }
    }

    // Encoding for every output.
    DialogGroup {
        VRow {
            label: qsTr("视频码率")
            hint: qsTr("Twitch 最高 6 Mbps，Kick 8 Mbps，YouTube 更高；同时推多个平台时按最严格的平台")
            VSeg {
                objectName: "pushfps-stream-bitrate"
                options: [{ id: "3", label: "3" }, { id: "4", label: "4" }, { id: "6", label: "6" }, { id: "8", label: "8" }, { id: "12", label: "12" }]
                current: String(live.bitrate)
                enabled: !live.streaming
                onPicked: id => veyra.setPreference("streamBitrateMbps", Number(id))
            }
        }
        VRow {
            label: qsTr("帧率")
            hint: qsTr("平台最多接收 60 FPS；画面更高时按 60 FPS 推流")
            VSeg {
                objectName: "pushfps-stream-fps"
                options: [{ id: "30", label: "30" }, { id: "60", label: "60" }]
                current: String(Number(veyra.preferences.streamFps || 60) >= 60 ? 60 : 30)
                enabled: !live.streaming
                onPicked: id => veyra.setPreference("streamFps", Number(id))
            }
        }
        VRow {
            label: qsTr("游戏声音")
            VSwitch {
                checked: veyra.preferences.streamAudio !== false
                enabled: !live.streaming
                onToggled: checked => veyra.setPreference("streamAudio", checked)
            }
        }
        VRow {
            label: qsTr("麦克风")
            hint: qsTr("混进游戏声音；设备与录制设置相同")
            VSwitch {
                checked: veyra.preferences.streamMic === true
                enabled: !live.streaming
                onToggled: checked => veyra.setPreference("streamMic", checked)
            }
        }
    }
    Text {
        objectName: "pushfps-stream-upload-note"
        Layout.fillWidth: true
        text: live.uploadNote
        color: Theme.t3
        font.family: Theme.fontUi; font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
}
