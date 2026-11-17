// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide video playback mode (WatchPage) against the fixture bridge: picking a window, a screen or a
// file, starting the capture without game mode, and the four picture switches.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 1300; height: 820
    Item { id: fixture; anchors.fill: parent }
    WatchPage { id: watch; pageId: "watch"; parent: fixture; width: fixture.width; height: fixture.height; visible: true; enabled: true }
    TestCase {
        name: "PushWatch"
        when: windowShown
        function init() {
            veyra.preferences = ({exportStopsPlayback: false})
            veyra.hasSource = false
            veyra.screenStarts = 0; veyra.fileDialogs = 0
            veyra.fgEnabled = false; veyra.fgMultiplier = 1; veyra.contentRate = 0
            veyra.srEnabled = false; veyra.videoHdr = false
            veyra.screenOptions = ({kind: 0, passthrough: true, method: 0, fps: 0})
            watch.kind = 0
        }
        function control(name) { const c = findChild(watch, name); verify(c !== null, name); return c }
        function test_window_capture_starts_without_game_mode() {
            watch.pick(0)
            control("watch-start").clicked()
            compare(veyra.screenStarts, 1)
            compare(veyra.screenOptions.passthrough, false, "a plain copy of the picture, no input passthrough")
            verify(veyra.hasSource)
            control("watch-stop").clicked()
            verify(!veyra.hasSource)
        }
        function test_screen_and_file() {
            watch.pick(1)
            compare(veyra.screenOptions.kind, 1)
            watch.pick(2)
            control("watch-start").clicked()
            compare(veyra.fileDialogs, 1)
            compare(veyra.screenStarts, 0)
        }
        function test_smoother_sets_frame_generation_and_duplicate_removal() {
            control("watch-smooth").toggled(true)
            verify(veyra.fgEnabled)
            compare(veyra.fgMultiplier, 2)
            compare(veyra.contentRate, 1, "videos repeat frames: duplicate removal finds the real ones")
            control("watch-smooth").toggled(false)
            verify(!veyra.fgEnabled)
        }
        function test_sharper_enhance_hdr() {
            control("watch-sharp").toggled(true)
            verify(veyra.srEnabled)
            control("watch-hdr").toggled(true)
            verify(veyra.videoHdr)
            watch.setEnhancement(true)
            verify(veyra.nrAnyEnabled)
            watch.setEnhancement(false)
        }
        function test_viewport_beside_the_controls() {
            verify(watch.videoViewport.x > watch.panelWidth)
            verify(watch.videoViewport.width > 300)
        }
    }
}
