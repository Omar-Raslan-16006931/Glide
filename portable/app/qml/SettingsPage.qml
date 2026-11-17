// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: settings
    signal back()
    color: Theme.bgDeep
    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ColumnLayout {
            width: Math.min(640, scroll.availableWidth - 48)
            x: (scroll.availableWidth - width) / 2
            spacing: 14
            Item { Layout.preferredHeight: 24 }
            RowLayout {
                Layout.fillWidth: true
                Text { text: qsTr("设置"); color: Theme.t1; font.pixelSize: 24; font.bold: true; font.family: Theme.fontUi; Layout.fillWidth: true }
                Button2 { text: qsTr("首页"); onClicked: settings.back() }
            }
            Card {
                Layout.fillWidth: true
                SettingRow {
                    label: qsTr("界面语言")
                    Choice {
                        objectName: "settings-language"
                        model: app.languageNames
                        currentIndex: Math.max(0, app.languageCodes.indexOf(app.language))
                        onActivated: index => app.language = app.languageCodes[index]
                    }
                }
                SettingRow {
                    label: qsTr("窗口置顶")
                    Toggle { checked: app.alwaysOnTop; onToggled: on => app.alwaysOnTop = on }
                }
                SettingRow {
                    label: qsTr("截图保存位置")
                    Button2 { text: qsTr("打开文件夹"); onClicked: app.openScreenshotFolder() }
                }
                SettingRow {
                    label: qsTr("便携模式")
                    hint: app.portable ? qsTr("已开启：设置和截图保存在程序旁边的文件夹里")
                                       : qsTr("在程序旁边放一个名为 portable.txt 的文件，设置和截图就会保存在那里，删除文件夹即可彻底移除")
                    Text { text: app.portable ? qsTr("开") : qsTr("关"); color: app.portable ? Theme.accent : Theme.t3; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi }
                }
            }
            Card {
                Layout.fillWidth: true
                title: qsTr("关于")
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.t2; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi
                    text: "Glide " + app.version + " · " + app.platformName
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.t2; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi
                    text: qsTr("Linux 和 macOS 预览版：补帧、帧节奏修复和实时 HUD 锁定都是 Glide 自己的技术，不需要 NVIDIA、AMD 或 Intel 的组件。")
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.t2; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi
                    text: qsTr("基于开源项目 Veyra（GPL-3.0）打造，感谢原作者 Likely7")
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Theme.t3; font.pixelSize: Theme.fsHint; font.family: Theme.fontUi
                    text: qsTr("Glide 以 GPL-3.0 许可发布。使用 Qt 6（LGPL-3.0），视频解码由 Qt 自带的 FFmpeg（LGPL）完成。")
                }
                Text {
                    color: Theme.accent; font.pixelSize: Theme.fsBody; font.family: Theme.fontUi
                    text: "github.com/DosukaSOL/PushFPS"
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Qt.openUrlExternally("https://github.com/DosukaSOL/PushFPS") }
                }
            }
        }
    }
}
