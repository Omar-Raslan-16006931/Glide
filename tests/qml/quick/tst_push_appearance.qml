// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Appearance modes (Theme.mode: default, black, light): every text level and state
// colour must stay readable on every surface it is drawn on (WCAG contrast ratios),
// and overlays (Theme.ink) must turn dark on light surfaces.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 200; height: 200

    TestCase {
        name: "PushAppearance"
        readonly property var modes: ["default", "black", "light"]
        readonly property var surfaces: ["card", "card2", "card3", "popover", "dialog", "bgInner"]
        function cleanup() { Theme.mode = "default"; Theme.accentName = "orange" }

        function lin(c) { return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4) }
        function luminance(r, g, b) { return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b) }
        // Contrast of colour fg (alpha composited) drawn on the opaque colour bg.
        function contrast(fg, bg) {
            const a = fg.a
            const r = fg.r * a + bg.r * (1 - a), g = fg.g * a + bg.g * (1 - a), b = fg.b * a + bg.b * (1 - a)
            const l1 = luminance(r, g, b), l2 = luminance(bg.r, bg.g, bg.b)
            return (Math.max(l1, l2) + 0.05) / (Math.min(l1, l2) + 0.05)
        }

        function test_text_is_readable_in_every_mode() {
            for (const m of modes) {
                Theme.mode = m
                for (const s of surfaces) {
                    const bg = Theme[s]
                    verify(contrast(Theme.t1, bg) >= 7, m + " t1 on " + s)
                    verify(contrast(Theme.tLabel, bg) >= 7, m + " tLabel on " + s)
                    verify(contrast(Theme.t2, bg) >= 4.5, m + " t2 on " + s + ": " + contrast(Theme.t2, bg).toFixed(2))
                    verify(contrast(Theme.t3, bg) >= 4.0, m + " t3 on " + s + ": " + contrast(Theme.t3, bg).toFixed(2))
                }
            }
        }
        function test_accent_and_states_are_readable() {
            for (const m of modes) {
                Theme.mode = m
                for (const name of ["orange", "white"]) {
                    Theme.accentName = name
                    for (const s of ["card", "card2", "card3"]) {
                        verify(contrast(Theme.accent, Theme[s]) >= 3, m + "/" + name + " accent on " + s)
                    }
                    verify(contrast(Theme.accentInk, Theme.accent) >= 4.5, m + "/" + name + " text on accent")
                }
                for (const c of ["ok", "warn", "err", "exp"])
                    verify(contrast(Theme[c], Theme.card2) >= 3, m + " " + c + " on card2: " + contrast(Theme[c], Theme.card2).toFixed(2))
            }
        }
        function test_ink_follows_the_mode() {
            Theme.mode = "default"
            verify(Theme.ink(0.5).r > 0.99, "white overlay on dark surfaces")
            Theme.mode = "black"
            verify(Theme.ink(0.5).r > 0.99)
            verify(Theme.card.r < 0.05 && Theme.bgOuter.r === 0, "black mode is almost black")
            Theme.mode = "light"
            verify(Theme.ink(0.5).r < 0.1, "dark overlay on light surfaces")
            verify(Theme.light && !Theme.black)
            verify(Theme.card.r > 0.95, "light surfaces")
            compare(String(Theme.onVideo), "#ffffff", "text over the picture stays white")
        }
    }
}
