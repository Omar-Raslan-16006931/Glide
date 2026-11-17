// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Plug and play rows of the quick setup (connect on start, fullscreen on connect), the 120 Hz
// console tip, and the total-latency guide (PushLatencyGuideDialog), against the fixture bridge.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 900; height: 1200
    Item { id: host; anchors.fill: parent; property string dialog: ""; function close() { dialog = "" } }
    PushQuickSetup { id: quick; width: 560 }
    PushLatencyGuideDialog { id: guide; dialogHost: host }

    property string navigated: ""
    Connections { target: veyra; function onNavigate(p) { root.navigated = p } }

    TestCase {
        name: "PushPlugAndPlay"
        when: windowShown
        function init() {
            veyra.preferences = ({})
            root.navigated = ""
            guide.phoneFps = 240
            guide.frames = 10
        }
        function control(item, name) { const c = findChild(item, name); verify(c !== null, name); return c }

        function test_connect_on_start_uses_veyra_auto_resume() {
            const sw = control(quick, "pushfps-auto-connect")
            verify(!sw.checked)
            sw.toggled(true)
            compare(veyra.preferences.autoResume, true)
            verify(control(quick, "pushfps-auto-connect").checked)
            sw.toggled(false)
            compare(veyra.preferences.autoResume, false)
        }
        function test_fullscreen_on_connect_preference() {
            const sw = control(quick, "pushfps-auto-fullscreen")
            verify(!sw.checked)
            sw.toggled(true)
            compare(veyra.preferences.pushAutoFullscreen, true)
            verify(sw.checked)
        }
        function test_plug_and_play_rows_hidden_in_the_compact_panel() {
            verify(control(quick, "pushfps-auto-connect-row").visible)
            quick.compact = true
            verify(!control(quick, "pushfps-auto-connect-row").visible)
            verify(!control(quick, "pushfps-auto-fullscreen-row").visible)
            quick.compact = false
        }
        function test_120hz_tip_for_playstation_and_xbox_only() {
            veyra.setPreference("pushConsole", "ps5")
            verify(quick.setupTip.indexOf("120 Hz") > 0, quick.setupTip)
            veyra.setPreference("pushConsole", "xbox")
            verify(quick.setupTip.indexOf("Xbox") === 0)
            verify(quick.setupTip.indexOf("120 Hz") > 0)
            veyra.setPreference("pushConsole", "switch2")
            verify(quick.setupTip.indexOf("120 Hz") < 0, "Switch 2 docked output: no 120 Hz line")
        }
        function test_latency_guide_opens_from_the_quick_setup() {
            control(quick, "pushfps-latency-guide-open").clicked()
            compare(root.navigated, "latencyguide")
        }
        function test_latency_guide_works_out_the_delay() {
            compare(Math.round(guide.totalMs), 42, "10 frames at 240 fps")
            guide.phoneFps = 120
            compare(Math.round(guide.totalMs), 83)
            guide.frames = 0
            compare(guide.totalMs, 0)
            const result = control(guide, "pushfps-latency-guide-result")
            verify(result.text.indexOf("0") >= 0)
            compare(guide.steps.length, 4)
        }
    }
}
