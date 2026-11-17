// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// The Sunset City intro (PushIntro.qml): its clock, the beats the logo parts land on, the glide into
// the home page logo and skipping. The clock is set by hand, as a probe would.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 1280; height: 800
    PushIntro { id: intro; anchors.fill: parent; target: Qt.rect(450, 120, 380, 156) }
    property int finishedCount: 0
    property int revealCount: 0
    Connections { target: intro; function onFinished() { root.finishedCount++ } function onRevealHome() { root.revealCount++ } }

    TestCase {
        name: "PushIntro"
        when: windowShown
        function init() {
            intro.running = false; intro.dismissing = false; intro.speed = 1; intro.opacity = 1
            intro.ms = 0; root.finishedCount = 0; root.revealCount = 0
        }
        function test_parts_land_on_their_beats() {
            compare(intro.travel(intro.hit1, 360, intro.hit1), 0, "first chevron in place on 0.64 s")
            compare(intro.travel(intro.hit2, 500, intro.hit2), 0, "second chevron on 0.76 s")
            compare(intro.travel(intro.hit3, 860, intro.hit3), 0, "the word on 1.12 s")
            verify(intro.travel(500, 360, intro.hit1) > 0.5, "still flying before its beat")
        }
        function test_scene_warms_after_the_big_hit() {
            intro.ms = 600
            compare(intro.warm, 0)
            compare(intro.horizonOn, 0)
            intro.ms = 2400
            verify(intro.warm > 0.99)
            verify(intro.sunUp > 0.5)
            verify(intro.cityUp > 0.99)
        }
        function test_glide_ends_on_the_home_logo() {
            intro.ms = intro.handoffAt
            verify(Math.abs(intro.logoW - intro.heroW * intro.camera) < 0.5)
            intro.ms = intro.total
            compare(intro.glide, 1)
            verify(Math.abs(intro.logoW - intro.artW) < 0.5)
            verify(Math.abs(intro.logoX - intro.artX) < 0.5)
            verify(Math.abs(intro.logoY - intro.artY) < 0.5)
            compare(intro.scene, 0)
        }
        function test_hero_fits_small_and_large_windows() {
            verify(intro.heroW <= root.width * 0.64 + 1)
            verify(intro.heroW <= 1000)
            verify(intro.heroH > 100)
        }
        function test_skip_before_landing_dismisses() {
            intro.ms = 500
            intro.skip()
            verify(intro.dismissing)
            compare(root.revealCount, 1)
            tryCompare(root, "finishedCount", 1, 2000)
        }
        function test_skip_after_landing_speeds_up_the_glide() {
            intro.ms = 1800
            intro.skip()
            verify(!intro.dismissing)
            compare(intro.ms, intro.handoffAt)
            compare(intro.speed, 2.5)
        }
        function test_runs_to_the_end() {
            intro.speed = 4
            intro.start()
            tryCompare(root, "finishedCount", 1, 5000)
            compare(intro.ms, intro.total)
            verify(root.revealCount >= 1)
        }
    }
}
