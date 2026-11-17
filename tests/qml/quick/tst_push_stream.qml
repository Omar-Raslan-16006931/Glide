// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Going live (PushStreamDialog and the quick setup tile) against the fixture bridge:
// outputs are kept in streamTargets, keys only go to pushSetStreamKey, several outputs
// go live together, and nothing can be changed while live.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 900; height: 1400
    Item { id: host; anchors.fill: parent; property string dialog: ""; property real motionScale: 1; function close() { dialog = "" } }
    PushStreamDialog { id: live; dialogHost: host }
    PushQuickSetup { id: quick; width: 560; y: 900 }

    TestCase {
        name: "PushStream"
        when: windowShown
        function init() {
            host.dialog = ""
            veyra.hasSource = true
            veyra.streamKeys = ({})
            veyra.streamStarts = 0; veyra.streamStops = 0
            veyra.streamState = ({ active: false, live: false, outputs: [] })
            veyra.setPreference("streamTargets", "[]")
            veyra.setPreference("streamBitrateMbps", 6)
            host.dialog = "stream"
            verify(live.shown)
            live.status = ({})
        }
        function stored() { return JSON.parse(String(veyra.preferences.streamTargets || "[]")) }

        function test_adds_the_main_platforms_first_and_stops_at_eight() {
            compare(live.targets.length, 0)
            live.add(); live.add(); live.add(); live.add()
            compare(stored().map(t => t.platform).join(","), "twitch,youtube,kick,tiktok")
            for (let i = 0; i < 6; ++i) live.add()
            compare(stored().length, 8, "at most eight outputs")
            compare(stored()[4].platform, "custom")
            const ids = stored().map(t => t.id)
            compare(new Set(ids).size, ids.length, "every output has its own id")
        }
        function test_keys_are_never_stored_in_the_preferences() {
            live.add()
            const id = live.targets[0].id
            verify(!veyra.pushHasStreamKey(id))
            verify(veyra.pushSetStreamKey(id, "live_123_secret"))
            verify(veyra.pushHasStreamKey(id))
            verify(String(veyra.preferences.streamTargets).indexOf("live_123_secret") < 0)
            live.remove(0)
            verify(!veyra.pushHasStreamKey(id), "deleting an output forgets its key")
            compare(stored().length, 0)
        }
        function test_multistream_goes_live_on_every_enabled_output() {
            live.add(); live.add(); live.add()
            for (const t of live.targets) veyra.pushSetStreamKey(t.id, "k-" + t.id)
            live.update(2, { enabled: false })
            compare(live.enabledCount, 2)
            verify(live.uploadNote.indexOf("12.4") >= 0, live.uploadNote)
            compare(findChild(live, "pushfps-stream-upload-note").wrapMode, Text.WordWrap)   // (6 + 0.2) Mbps for each of two outputs
            live.actionTriggered(live.actions[1].label)
            compare(veyra.streamStarts, 1)
            verify(live.streaming)
            compare(live.status.outputs.length, 2, "the switched-off output stays off")
            compare(live.actions.length, 2)
            const stop = live.actions[1].label
            verify(stop !== "", "a stop button while live")
            const add = findChild(live, "pushfps-stream-add")
            verify(add && !add.enabled, "outputs cannot change while live")
            live.actionTriggered(stop)
            compare(veyra.streamStops, 1)
            verify(!live.streaming)
        }
        function test_without_a_key_nothing_starts() {
            live.add()
            live.actionTriggered(live.actions[1].label)
            compare(veyra.streamStarts, 0)
            verify(!live.streaming)
        }
        function test_damaged_targets_load_as_empty() {
            host.dialog = ""
            const copy = Object.assign({}, veyra.preferences); copy.streamTargets = "{not json"; veyra.preferences = copy
            host.dialog = "stream"
            compare(live.targets.length, 0)
            live.add()
            compare(stored().length, 1)
        }
        function test_platform_change_clears_the_server() {
            live.add()
            live.update(0, { server: "rtmp://live.twitch.tv/app" })
            live.update(0, { platform: "custom", server: "" })
            compare(stored()[0].platform, "custom")
            compare(stored()[0].server, "")
            compare(live.platformOf("custom").servers.length, 0)
            compare(live.platformOf("nope").id, "custom", "an unknown platform falls back to custom")
        }
        function test_quick_setup_tile_opens_the_dialog() {
            const tile = findChild(quick, "pushfps-stream")
            verify(tile)
            quick.liveStream = ({ active: true, live: true, outputs: [{ state: "live" }, { state: "live" }] })
            verify(tile.sub.indexOf("2") >= 0, tile.sub)
            quick.liveStream = ({ active: false })
        }
        function test_duration_format() {
            compare(live.duration(5), "0:05")
            compare(live.duration(125), "2:05")
            compare(live.duration(3725), "1:02:05")
        }
    }
}
