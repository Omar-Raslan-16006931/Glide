// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: home
    signal openSettings()
    signal openPlayer()
    color: Theme.bgDeep
    gradient: Gradient {
        GradientStop { position: 0; color: Theme.bg }
        GradientStop { position: 1; color: Theme.bgDeep }
    }

    FileDialog {
        id: fileDialog
        title: qsTr("打开视频")
        nameFilters: [qsTr("视频文件") + " (*.mp4 *.mkv *.mov *.webm *.avi *.ts)"]
        onAccepted: app.startFile(selectedFile)
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            id: column
            width: Math.min(640, scroll.availableWidth - 48)
            x: (scroll.availableWidth - width) / 2
            spacing: 14

            Item { Layout.preferredHeight: 28 }
            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "logo.png"
                sourceSize.height: 72
                fillMode: Image.PreserveAspectFit
            }
            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 0
                Text { text: "Push"; color: Theme.t1; font.pixelSize: 36; font.italic: true; font.bold: true; font.family: Theme.fontBrand }
                Text { text: "FPS"; color: Theme.accent; font.pixelSize: 36; font.italic: true; font.bold: true; font.family: Theme.fontBrand }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                Text { text: qsTr("主机画面经采集卡接入，由电脑补帧到 60 或 120 FPS"); color: Theme.t2; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi }
                Rectangle {
                    radius: 6; color: Theme.accentSoft; implicitWidth: badge.implicitWidth + 12; implicitHeight: badge.implicitHeight + 4
                    Text { id: badge; anchors.centerIn: parent; text: qsTr("预览版"); color: Theme.accent; font.pixelSize: Theme.fsHint; font.bold: true; font.family: Theme.fontUi }
                }
            }

            // Errors stay until the next action.
            Rectangle {
                visible: app.errorText.length > 0
                Layout.fillWidth: true
                radius: 10
                color: Theme.errSoft
                border.color: Theme.err
                implicitHeight: errorText.implicitHeight + 20
                Text {
                    id: errorText
                    x: 12; y: 10; width: parent.width - 48
                    text: app.errorText; color: Theme.t1; wrapMode: Text.WordWrap; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi
                }
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
                    text: "✕"; color: Theme.t2
                    MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: app.clearError() }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("采集卡")
                subtitle: qsTr("HDMI 采集设备")
                SettingRow {
                    label: qsTr("设备")
                    Choice {
                        objectName: "home-device"
                        model: app.videoDevices.length > 0 ? app.videoDevices : [qsTr("未检测到采集设备")]
                        enabled: app.videoDevices.length > 0
                        currentIndex: app.videoDevices.length > 0 ? app.videoDeviceIndex : 0
                        onActivated: index => app.videoDeviceIndex = index
                    }
                }
                SettingRow {
                    label: qsTr("格式")
                    visible: app.formats.length > 0
                    Choice {
                        model: app.formats
                        currentIndex: app.formatIndex
                        onActivated: index => app.formatIndex = index
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button2 { text: qsTr("刷新"); onClicked: app.refreshDevices() }
                    Button2 { objectName: "home-connect"; text: qsTr("连接"); primary: true; enabled2: app.videoDevices.length > 0; onClicked: app.startCamera() }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Card {
                    Layout.fillWidth: true
                    title: qsTr("打开视频")
                    subtitle: "MP4 · MKV · MOV"
                    Button2 { text: qsTr("选择文件"); onClicked: fileDialog.open() }
                }
                Card {
                    Layout.fillWidth: true
                    title: qsTr("测试画面")
                    subtitle: qsTr("没有采集卡也能看看补帧效果")
                    RowLayout {
                        spacing: 8
                        Button2 { objectName: "home-test"; text: qsTr("开始"); onClicked: app.startTestPattern(false) }
                        Button2 { text: qsTr("不均匀出帧"); onClicked: app.startTestPattern(true) }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("快速设置")
                SettingRow {
                    label: qsTr("目标帧率")
                    hint: app.targetMode === 0 ? qsTr("补帧关闭：每一帧到达后立刻显示，延迟最低")
                                               : qsTr("补出来的帧夹在两张真实画面之间，画面会晚一张真实画面显示")
                    Segmented {
                        objectName: "home-target"
                        options: [qsTr("关"), "60 FPS", "120 FPS", qsTr("匹配显示器")]
                        currentIndex: app.targetMode
                        onPicked: index => app.targetMode = index
                    }
                }
                SettingRow {
                    label: qsTr("帧节奏修复")
                    hint: qsTr("把出帧不均匀的锁帧游戏重新对齐到均匀的时间，每帧最多晚一次刷新。需要开启自动去重")
                    Toggle { objectName: "home-pacing"; checked: app.pacingRepair; enabled2: app.targetMode !== 0; onToggled: on => app.pacingRepair = on }
                }
                SettingRow {
                    label: qsTr("实时 HUD 锁定")
                    hint: qsTr("每一帧自动找出 HUD，文字和准星在补出来的帧里保持清晰")
                    Toggle { objectName: "home-hud"; checked: app.hudLock; enabled2: app.targetMode !== 0; onToggled: on => app.hudLock = on }
                }
                SettingRow {
                    label: qsTr("去除重复帧")
                    hint: qsTr("30 FPS 的游戏在 60 Hz 信号里每张画面会发两次；补帧只看真正的新画面")
                    Toggle { checked: app.removeDuplicates; onToggled: on => app.removeDuplicates = on }
                }
                SettingRow {
                    label: qsTr("运动估计")
                    hint: qsTr("精细：按 16 像素找运动，更准；快速：按 32 像素，占用更少")
                    Segmented { options: [qsTr("快速"), qsTr("精细")]; currentIndex: app.quality; onPicked: index => app.quality = index }
                }
                SettingRow {
                    label: qsTr("在画面上显示延迟")
                    hint: qsTr("游戏帧率、输出帧率和延迟（按 Tab 切换）")
                    Toggle { checked: app.showStats; onToggled: on => app.showStats = on }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("声音")
                subtitle: qsTr("采集卡的声音与画面同步延迟后播放")
                SettingRow {
                    label: qsTr("播放采集卡声音")
                    Toggle { checked: app.audioPassthrough; onToggled: on => app.audioPassthrough = on }
                }
                SettingRow {
                    label: qsTr("音量")
                    Slider2 { from: 0; to: 1; value: app.volume; neutral: 1; decimals: 2; onMoved: v => app.volume = v }
                }
                SettingRow {
                    label: qsTr("输入")
                    Choice { model: app.audioInputs; currentIndex: app.audioInputIndex; onActivated: index => app.audioInputIndex = index }
                }
                SettingRow {
                    label: qsTr("输出")
                    Choice { model: app.audioOutputs; currentIndex: app.audioOutputIndex; onActivated: index => app.audioOutputIndex = index }
                }
            }

            Card {
                Layout.fillWidth: true
                title: qsTr("画面")
                SettingRow { label: qsTr("亮度"); Slider2 { from: -0.5; to: 0.5; value: app.brightness; neutral: 0; onMoved: v => app.brightness = v } }
                SettingRow { label: qsTr("对比度"); Slider2 { from: 0.5; to: 1.5; value: app.contrast; neutral: 1; onMoved: v => app.contrast = v } }
                SettingRow { label: qsTr("饱和度"); Slider2 { from: 0; to: 2; value: app.saturation; neutral: 1; onMoved: v => app.saturation = v } }
                SettingRow { label: qsTr("伽马"); Slider2 { from: 0.5; to: 2; value: app.gamma; neutral: 1; onMoved: v => app.gamma = v } }
                RowLayout { Item { Layout.fillWidth: true } Button2 { text: qsTr("重置"); onClicked: app.resetPicture() } }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("基于开源项目 Veyra（GPL-3.0）打造，感谢原作者 Likely7")
                    color: Theme.t3; font.pixelSize: Theme.fsHint; font.family: Theme.fontUi; wrapMode: Text.WordWrap
                }
                Button2 { text: qsTr("设置"); onClicked: home.openSettings() }
            }
            Item { Layout.preferredHeight: 24 }
        }
    }
}
