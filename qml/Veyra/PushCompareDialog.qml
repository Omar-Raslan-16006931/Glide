// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Before/after comparison clip: choose the layout and the length, then record
// (pushStartCompareClip, engine/RecordingCompare.h). Split: one picture cut down
// the middle. Side by side: the whole picture twice, each at half size, with the
// FPS labels in the black bar above each. Vertical: a 9:16 clip for TikTok, Shorts
// and Reels, the console on top and Glide below, labels next to the divider. Every
// layout labels each side with its frame rate. Dialog key "compare"; opened from the
// quick setup's comparison tile. Preferences pushCompareLayout (0 split, 1 side by
// side, 2 vertical) and pushCompareSeconds.
import QtQuick
import QtQuick.Layouts

DialogLayer {
    id: cmp
    objectName: "pushfps-compare"
    key: "compare"
    glyph: "split"
    title: qsTr("对比片段")
    sub: qsTr("录制一段前后对比视频：一边是主机原本的帧率，一边是 Glide")
    dialogWidth: 560

    readonly property var lengths: [10, 15, 30, 60]
    readonly property int layoutId: {
        const n = Number(veyra.preferences.pushCompareLayout || 0)
        return n === 1 || n === 2 ? n : 0
    }
    readonly property int seconds: {
        const n = Number(veyra.preferences.pushCompareSeconds || 15)
        return cmp.lengths.indexOf(n) >= 0 ? n : 15
    }
    // For the preview only: the recording measures the real rates when it starts.
    readonly property int gameFps: {
        const n = Number(veyra.preferences.pushGameFps || 30)
        return n === 40 || n === 60 ? n : 30
    }
    readonly property int shownFps: cmp.gameFps * Math.max(2, Number(veyra.fgMultiplier || 2))
    readonly property string leftTag: qsTr("主机原画 · %1 FPS").arg(cmp.gameFps)
    readonly property string rightTag: "Glide · " + cmp.shownFps + " FPS"

    // The preview drawn: portrait for the vertical layout, landscape for the others.
    readonly property bool verticalPreview: cmp.layoutId === 2
    function layoutName(id) { return id === 2 ? qsTr("竖屏") : id === 1 ? qsTr("并排") : qsTr("分屏") }
    function layoutHint(id) {
        return id === 2 ? qsTr("竖屏 9:16，适合 TikTok、Shorts 和 Reels：上面是原帧率，下面是 Glide，各占半屏（取画面中间部分）")
             : id === 1 ? qsTr("完整画面并排两份：左边是原帧率，右边是 Glide")
             : qsTr("同一个画面从中间分开：左半边是原帧率，右半边是 Glide")
    }
    // Vertical preview (portrait, 108 x 192): where each label goes, centred just above (index 0)
    // and just below (index 1) the divider in the middle, as the clip puts them.
    function verticalTagY(index, tagHeight, previewHeight) {
        return index === 0 ? previewHeight / 2 - 4 - tagHeight : previewHeight / 2 + 4
    }
    // Where the preview puts each label (index 0 left, 1 right), as the clip does: split at
    // the top corners of the picture, side by side centred in the black bar above each picture.
    function tagX(index, tagWidth, previewWidth, sideBySide) {
        if (sideBySide) return (index === 0 ? 0 : previewWidth / 2) + (previewWidth / 2 - tagWidth) / 2
        return index === 0 ? 6 : previewWidth - tagWidth - 6
    }
    function tagY(tagHeight, previewHeight, sideBySide) {
        return sideBySide ? (previewHeight / 4 - tagHeight) / 2 : 6
    }

    // The layout drawn small, with the labels where the clip puts them.
    Rectangle {
        id: preview
        objectName: "pushfps-compare-preview"
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 12
        implicitWidth: 320
        implicitHeight: 180
        radius: 6
        color: "#000000"
        border.width: 1
        border.color: Theme.stroke
        clip: true
        visible: !cmp.verticalPreview
        readonly property bool sideBySide: cmp.layoutId === 1
        readonly property real picW: sideBySide ? width / 2 : width / 2
        readonly property real picH: sideBySide ? height / 2 : height
        readonly property real picTop: sideBySide ? height / 4 : 0
        Repeater {
            model: 2
            delegate: Item {
                required property int index
                x: index === 0 ? 0 : preview.width / 2
                y: preview.picTop
                width: preview.picW
                height: preview.picH
                // A stand-in picture: a sky and a road, dimmer on the console side.
                Rectangle {
                    // Split: both halves are parts of one picture; side by side: each is the whole picture.
                    x: preview.sideBySide ? 0 : -index * preview.picW
                    width: preview.sideBySide ? parent.width : preview.width
                    height: parent.height
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#3d6fa8" }
                        GradientStop { position: 0.55; color: "#9cc3e6" }
                        GradientStop { position: 0.56; color: "#4b5a3a" }
                        GradientStop { position: 1.0; color: "#2c3324" }
                    }
                }
                clip: true
            }
        }
        Rectangle {
            x: preview.width / 2 - 1
            y: preview.picTop
            width: 2
            height: preview.picH
            color: "#ffffff"
        }
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                objectName: index === 0 ? "pushfps-compare-tag-left" : "pushfps-compare-tag-right"
                // The clip's badges in small: dark pills, the Glide one outlined in logo green.
                width: tagText.implicitWidth + 16
                height: tagText.implicitHeight + 6
                radius: height / 2
                border.width: 1
                border.color: index === 1 ? "#8df237" : "#4dffffff"
                // Split: top corners of the picture. Side by side: centred in the bar above each picture.
                x: cmp.tagX(index, width, preview.width, preview.sideBySide)
                y: cmp.tagY(height, preview.height, preview.sideBySide)
                color: "#d60e0f12"
                Text {
                    id: tagText
                    anchors.centerIn: parent
                    text: index === 0 ? cmp.leftTag : cmp.rightTag
                    color: "#ffffff"
                    font.family: Theme.fontUi
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }
        }
    }

    // Vertical: a portrait stand-in, the console picture on top and Glide below.
    Rectangle {
        id: portrait
        objectName: "pushfps-compare-preview-vertical"
        visible: cmp.verticalPreview
        Layout.alignment: Qt.AlignHCenter
        Layout.bottomMargin: 12
        implicitWidth: 108
        implicitHeight: 192
        radius: 6
        color: "#000000"
        border.width: 1
        border.color: Theme.stroke
        clip: true
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                y: index * portrait.height / 2
                width: portrait.width
                height: portrait.height / 2
                opacity: index === 0 ? 0.8 : 1
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#3d6fa8" }
                    GradientStop { position: 0.55; color: "#9cc3e6" }
                    GradientStop { position: 0.56; color: "#4b5a3a" }
                    GradientStop { position: 1.0; color: "#2c3324" }
                }
            }
        }
        Rectangle { y: portrait.height / 2 - 1; width: portrait.width; height: 2; color: "#ffffff" }
        Repeater {
            model: 2
            delegate: Rectangle {
                required property int index
                objectName: index === 0 ? "pushfps-compare-vtag-top" : "pushfps-compare-vtag-bottom"
                width: Math.min(portrait.width - 8, vtagText.implicitWidth + 10)
                height: vtagText.implicitHeight + 4
                radius: height / 2
                border.width: 1
                border.color: index === 1 ? "#8df237" : "#4dffffff"
                x: (portrait.width - width) / 2
                y: cmp.verticalTagY(index, height, portrait.height)
                color: "#d60e0f12"
                Text {
                    id: vtagText
                    anchors.centerIn: parent
                    text: index === 0 ? cmp.leftTag : cmp.rightTag
                    color: "#ffffff"
                    font.family: Theme.fontUi
                    font.pixelSize: 7
                    font.weight: Font.Bold
                }
            }
        }
    }

    DialogGroup {
        VRow {
            label: qsTr("布局")
            hint: cmp.layoutHint(cmp.layoutId)
            hintBelow: true
            VSeg {
                objectName: "pushfps-compare-layout"
                options: [0, 1, 2].map(n => ({ id: String(n), label: cmp.layoutName(n) }))
                current: String(cmp.layoutId)
                onPicked: id => veyra.setPreference("pushCompareLayout", Number(id))
            }
        }
        VRow {
            label: qsTr("时长")
            VSeg {
                objectName: "pushfps-compare-length"
                options: cmp.lengths.map(n => ({ id: String(n), label: qsTr("%1 秒").arg(n) }))
                current: String(cmp.seconds)
                onPicked: id => veyra.setPreference("pushCompareSeconds", Number(id))
            }
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.topMargin: 10
        text: !veyra.hasSource ? qsTr("需要正在播放的画面")
            : cmp.layoutId === 2 ? qsTr("两半都标有各自的帧率。保存为 1080×1920 视频，可直接上传到手机应用")
            : qsTr("两边上方都标有各自的帧率。视频保存在录制文件夹里，可直接导入剪辑软件")
        color: veyra.hasSource ? Theme.t3 : Theme.warn
        font.family: Theme.fontUi
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    actions: [{ label: qsTr("取消") }, { label: qsTr("开始录制"), primary: true }]
    onActionTriggered: label => {
        if (label === qsTr("开始录制")) {
            if (veyra.pushStartCompareClip(cmp.seconds, cmp.layoutId)) cmp.dialogHost.close()
        } else cmp.dialogHost.close()
    }
}
