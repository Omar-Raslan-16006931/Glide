// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide streaming mode (StudioPage, PushMixerStrip, PushMicFiltersDialog) against the fixture
// bridge: the OBS-style layout, the mixer's faders and mutes, the microphone's destinations and
// filters, go live / record controls and the meters' ballistics.
import QtQuick
import QtQuick.Window
import QtTest
import Veyra
import "../qml/Veyra/StudioLayout.js" as StudioGeometry
Item {
    id: root
    width: 1400; height: 900
    Item { id: fixture; anchors.fill: parent }
    Item { id: host; anchors.fill: parent; property string dialog: ""; property real motionScale: 1; function close() { dialog = "" } }
    // VPage takes its parent from Main.qml's page stack; the fixture places it directly.
    StudioPage { id: studio; pageId: "live"; parent: fixture; width: fixture.width; height: fixture.height; visible: true; enabled: true }
    PushMicFiltersDialog { id: filters; dialogHost: host }

    property var navigated: []
    Connections { target: veyra; function onNavigate(p) { root.navigated.push(p) } }

    TestCase {
        name: "PushStudio"
        when: windowShown
        function init() {
            veyra.preferences = ({exportStopsPlayback: false})
            veyra.hasSource = true
            veyra.recording = false
            veyra.volume = 1; veyra.muted = false
            veyra.streamState = ({active: false, live: false, outputs: []})
            veyra.streamStarts = 0
            studio.activeScene = "game"
            root.navigated = []
            host.dialog = ""
        }
        function control(item, name) { const c = findChild(item, name); verify(c !== null, name); return c }

        function test_layout_like_obs() {
            const g = StudioGeometry.calculate(1400, 900)
            verify(!g.compact)
            verify(g.left.x < g.preview.x && g.preview.x + g.preview.width < g.right.x, "features | preview | controls")
            verify(g.mixer.y >= g.preview.y + g.preview.height && g.status.y >= g.mixer.y + g.mixer.height, "mixer and status bar below")
            compare(g.mixer.width, 1400 - 24, "mixer spans the window")
            verify(g.video.y > g.preview.y && g.video.height < g.preview.height, "the picture sits under the preview header")
            const small = StudioGeometry.calculate(900, 600)
            verify(small.compact && small.left.width === 0, "narrow window: no feature column")
            verify(small.preview.width > 300)
            compare(studio.videoViewport.width, g.video.width)
        }
        function test_meter_scale_and_fall() {
            compare(StudioGeometry.meterPosition(0), 1)
            compare(StudioGeometry.meterPosition(-60), 0)
            compare(StudioGeometry.meterPosition(-30), 0.5)
            compare(StudioGeometry.meterPosition(-200), 0)
            compare(StudioGeometry.fall(-10, -40, 0.5, 24), -22, "falls 24 dB/s")
            compare(StudioGeometry.fall(-30, -6, 0.05, 24), -6, "rises at once")
            compare(StudioGeometry.duration(3725), "1:02:05")
            compare(StudioGeometry.duration(65), "1:05")
        }
        function test_game_fader_and_mute() {
            const game = control(studio, "mixer-game")
            game.moved(-6.4)
            compare(veyra.preferences.mixGameDb, -6)
            compare(game.readout, "-6.0 dB")
            game.muteToggled()
            compare(veyra.preferences.mixGameMuted, true)
            compare(game.readout, qsTr("静音"))
            game.muteToggled()
            compare(veyra.preferences.mixGameMuted, false)
            game.moved(-60)
            compare(game.readout, "-∞ dB")
        }
        function test_microphone_destinations_and_filters() {
            const mic = control(studio, "mixer-mic")
            verify(!studio.micUsed)
            compare(mic.subtitle, qsTr("未用于录制或直播"))
            verify(control(studio, "mixer-mic-record") !== null)
            studio.toggleMicRecording()
            compare(veyra.preferences.recordMic, true)
            verify(studio.micUsed)
            mic.moved(4)
            compare(veyra.preferences.mixMicDb, 4)
            mic.gearClicked()
            compare(root.navigated[root.navigated.length - 1], "micfilters")
        }
        function test_monitor_is_the_player_volume() {
            const monitor = control(studio, "mixer-monitor")
            monitor.moved(0.4)
            compare(veyra.volume, 0.4)
            compare(monitor.readout, "40%")
            monitor.muteToggled()
            verify(veyra.muted)
        }
        function test_go_live_needs_an_output() {
            control(studio, "studio-go-live").clicked()
            compare(root.navigated[root.navigated.length - 1], "stream", "no platform yet: the outputs dialog opens")
            veyra.setPreference("streamTargets", JSON.stringify([{id: "a", platform: "twitch", name: "Twitch", enabled: true}]))
            veyra.pushSetStreamKey("a", "k")
            control(studio, "studio-go-live").clicked()
            compare(veyra.streamStarts, 1)
            verify(studio.live)
            verify(control(studio, "studio-live-badge").visible)
            compare(studio.streamKbps, 6100)
            control(studio, "studio-go-live").clicked()
            verify(!studio.live)
        }
        function test_record_replay_compare_screenshot() {
            control(studio, "studio-record").clicked()
            verify(veyra.recording)
            verify(control(studio, "studio-rec-badge").visible)
            control(studio, "studio-replay").clicked()
            compare(veyra.preferences.replayEnabled, true, "first tap turns the instant replay on")
            control(studio, "studio-replay").clicked()
            compare(veyra.replaySaves, 1)
            control(studio, "studio-compare").clicked()
            compare(root.navigated[root.navigated.length - 1], "compare")
            const before = veyra.screenshots
            control(studio, "studio-screenshot").clicked()
            compare(veyra.screenshots, before + 1)
            veyra.recording = false
        }
        function test_meters_follow_the_levels() {
            veyra.studioLevels = ({gamePeakDb: -3, gameRmsDb: -12, micPeakDb: -100, micRmsDb: -100, micOpen: false})
            studio.lastTick = 0
            studio.poll()
            compare(studio.gamePeak, -3)
            compare(studio.gameRms, -12)
            veyra.studioLevels = ({gamePeakDb: -100, gameRmsDb: -100, micPeakDb: -100, micRmsDb: -100, micOpen: false})
            studio.lastTick = Date.now() - 500
            studio.poll()
            verify(studio.gamePeak < -3 && studio.gamePeak > -100, "falls off instead of dropping to silence")
        }
        function test_scenes_switch_the_output_and_mute_the_game_on_cards() {
            compare(studio.scenes.length, 4, "game, starting soon, be right back, thanks for watching")
            veyra.setPreference("mixGameMuted", false)
            studio.activate("brb")
            compare(veyra.lastScene.type, "card")
            compare(studio.activeScene, "brb")
            compare(veyra.preferences.mixGameMuted, true, "a card mutes the game")
            verify(findChild(studio, "studio-scene-brb").active)
            studio.activate("soon")
            compare(veyra.preferences.mixGameMuted, true)
            studio.activate("game")
            compare(veyra.lastScene.type, "game")
            compare(veyra.preferences.mixGameMuted, false, "back to the game: the mute from before")
        }
        function test_scenes_add_edit_remove() {
            studio.addScene("card")
            compare(studio.scenes.length, 5)
            const id = studio.editingScene
            verify(id.length > 0, "the new scene opens in the editor")
            verify(findChild(studio, "studio-scene-editor").visible)
            studio.updateScene(id, { title: "Back in 5", subtitle: "Grab a drink" })
            compare(studio.sceneById(id).title, "Back in 5")
            studio.activate(id)
            const calls = veyra.sceneCalls
            studio.updateScene(id, { subtitle: "Soon" })
            wait(20)
            verify(veyra.sceneCalls > calls, "editing the live scene redraws it")
            verify(studio.removeScene(id))
            compare(studio.activeScene, "game", "removing the live scene goes back to the game")
            verify(!studio.removeScene("game"), "the only game scene stays")
            studio.addScene("game")
            const g = studio.editingScene
            veyra.pickedImage = "C:/logo.png"
            studio.updateScene(g, { overlayImage: veyra.pickedImage, overlayText: "@me", corner: "bl" })
            studio.activate(g)
            compare(veyra.lastScene.overlayText, "@me")
            compare(veyra.lastScene.corner, "bl")
            verify(studio.removeScene(g))
            studio.editingScene = ""
        }
        function test_mic_filters_dialog() {
            host.dialog = "micfilters"
            verify(filters.shown)
            control(filters, "mic-highpass").toggled(true)
            compare(veyra.preferences.micHighPass, true)
            verify(filters.responseDb(30) < -10, "rumble filter in the curve")
            control(filters, "mic-gate").toggled(true)
            compare(veyra.preferences.micGate, true)
            filters.applyPreset("clear")
            compare(veyra.preferences.micEq, "-2,-3,0,4,2")
            compare(veyra.preferences.micEqOn, true)
            compare(filters.presetOf(filters.eq), qsTr("人声清晰"))
            verify(Math.abs(filters.responseDb(3500) - 4) < 1.5, "EQ boost shows in the curve")
            filters.setBand(0, 5)
            compare(filters.presetOf(filters.eq), qsTr("自定义"))
            filters.actionTriggered(qsTr("全部关闭"))
            compare(veyra.preferences.micHighPass, false)
            compare(veyra.preferences.micGate, false)
            compare(veyra.preferences.micEqOn, false)
            filters.actionTriggered(qsTr("完成"))
            compare(host.dialog, "")
        }
    }
}
