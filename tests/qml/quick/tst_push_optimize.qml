// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// "Optimise for my PC" (PushOptimizeDialog) against the fixture bridge: the plan,
// the pass rules, the confirmation run, applying the winner and undo. The engine's
// measurement window is replaced by a model of a PC (veyra.benchmarkModel).
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 900; height: 900
    Item { id: host; anchors.fill: parent; property string dialog: "optimize"; function close() { dialog = "" } }
    PushOptimizeDialog {
        id: opt
        dialogHost: host
        settleTimeout: 1; settleHold: 0.05; warmSeconds: 0.05; measureSeconds: 0.1; tickMs: 15
    }

    // A PC: VFG reaches 120 FPS with 18 ms, DLSS 120 with 24 ms, FSR only 60; AI
    // enhancement costs too much at 120; nothing generates under "broken".
    property string broken: ""
    property real vfgLatency: 18
    property int confirmFailures: 0
    function model() {
        const b = veyra.fgEnabled ? veyra.fgBackendName : ""
        const m = veyra.fgEnabled ? veyra.fgMultiplier : 1
        const base = { valid: true, presents: 600, generatedPresents: 0, low01Fps: 0, jitterMs: 0.4, latencyP50Ms: 0, latencyP95Ms: 0 }
        if (!b) return Object.assign(base, { fps: 60, low1Fps: 58, latencyMeanMs: 12 })
        const want = 30 * m
        let fps = want, latency = 20
        if (b === "vfg") latency = root.vfgLatency
        if (b === "dlss") latency = 24
        if (b === "fsr3") fps = Math.min(want, 60)
        if (veyra.nrAnyEnabled) fps = want * 0.8
        const generated = b === root.broken ? 0 : Math.round(600 * (m - 1) / m)
        return Object.assign(base, { fps: fps, low1Fps: fps * 0.92, latencyMeanMs: latency, generatedPresents: generated })
    }

    TestCase {
        name: "PushOptimize"
        when: windowShown
        function init() {
            veyra.preferences = ({ pushGameFps: 30 })
            veyra.fgEnabled = false; veyra.fgMultiplier = 1; veyra.fgBackendName = "dlss"
            veyra.outputRateMode = 0; veyra.outputCustomFps = 0; veyra.nrAnyEnabled = false; veyra.srEnabled = false
            veyra.displayRefreshHz = 144
            veyra.fgBackendChoices = [{ id: "dlss", label: "DLSS" }, { id: "fsr3", label: "FSR 3" }, { id: "vfg", label: "VFG" },
                                      { id: "anr", label: "ANR", danger: true }, { id: "xess", label: "XeSS" },
                                      { id: "amdfrc", label: "AMD FRC", disabled: true, note: "needs an AMD GPU" }]
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"},{id:"3"},{id:"4"}]
            root.vfgLatency = 18
            veyra.liveStats = ({ running: true, settled: true, contentFps: 30, displayRefreshHz: 144, sourceHeight: 2160,
                                 captureReceived: 1000, captureDropped: 0, backendWarning: "" })
            veyra.benchmarkModel = root.model
            root.broken = ""
            host.dialog = "optimize"
            opt.phase = "idle"
        }
        function cleanup() { if (opt.phase === "running") opt.cancel(); init() }

        function test_needs_a_picture() {
            veyra.liveStats = ({ running: false })
            opt.start()
            compare(opt.phase, "failed")
            verify(opt.failure.length > 0)
            verify(!veyra.fgEnabled, "nothing changed")
        }
        function test_picks_the_best_measured_combination() {
            opt.start()
            tryCompare(opt, "phase", "done", 15000)
            verify(opt.best !== null)
            compare(opt.best.config.backend, "vfg", "highest quality method that held 120 FPS")
            compare(opt.best.config.target, "120")
            compare(opt.best.config.enhance, false, "AI enhancement could not hold 120 here")
            compare(veyra.fgBackendName, "vfg")
            compare(veyra.fgMultiplier, 4)
            compare(veyra.outputRateMode, 2)
            compare(veyra.outputCustomFps, 120)
            const kinds = opt.steps.map(s => s.config.kind)
            compare(kinds[0], "baseline")
            verify(kinds.indexOf("enhance") > 0 && kinds[kinds.length - 1] === "confirm", kinds.join(","))
            // Every available method is measured (audit item), the unavailable one is listed with its reason.
            verify(opt.steps.some(s => s.config.backend === "anr"), "ANR measured")
            verify(opt.steps.some(s => s.config.backend === "xess"), "XeSS measured")
            verify(!opt.steps.some(s => s.config.backend === "amdfrc"), "AMD FRC not available here")
            verify(opt.skipped.some(x => x.label === "AMD FRC" && x.reason === "needs an AMD GPU"), "skip reason shown")
            // 144 Hz: no whole multiplier of 30 reaches 144, so 120 is the top target and that is said.
            verify(!opt.steps.some(s => s.config.target === "monitor"), "no unreachable 144 FPS target")
            verify(opt.skipped.some(x => x.reason.indexOf("144") >= 0), "the 144 Hz limit is explained")
            const fsr = opt.steps.find(s => s.config.backend === "fsr3")
            verify(fsr && !fsr.result.ok && fsr.result.reason.length > 0, "FSR at 60 FPS fails the 120 target")
            const record = JSON.parse(veyra.preferences.pushBenchmark)
            compare(record.steps.length, opt.steps.length)
            verify(record.best.indexOf("VFG") >= 0)
            compare(veyra.preferences.pushGameFps, 30)
        }
        function test_frames_must_really_be_generated() {
            root.broken = "vfg"
            opt.start()
            tryCompare(opt, "phase", "done", 15000)
            const vfg = opt.steps.find(s => s.config.backend === "vfg" && s.config.kind === "fg-high")
            verify(!vfg.result.ok)
            compare(vfg.result.reason, qsTr("没有真正生成帧"))
            compare(opt.best.config.backend, "dlss", "next best that really generated frames")
        }
        function test_undo_restores_the_settings() {
            veyra.fgEnabled = true; veyra.fgBackendName = "dlss"; veyra.fgMultiplier = 2
            veyra.outputRateMode = 2; veyra.outputCustomFps = 60
            opt.start()
            tryCompare(opt, "phase", "done", 15000)
            opt.undo()
            compare(veyra.fgBackendName, "dlss")
            compare(veyra.fgMultiplier, 2)
            compare(veyra.outputCustomFps, 60)
            compare(host.dialog, "")
        }
        function test_stops_when_the_picture_stops() {
            opt.start()
            wait(100)
            veyra.liveStats = ({ running: false })
            tryCompare(opt, "phase", "failed", 5000)
            verify(!veyra.fgEnabled, "settings restored")
        }
        function test_240hz_screen_is_tested_at_240() {
            veyra.displayRefreshHz = 240
            veyra.liveStats = Object.assign({}, veyra.liveStats, { displayRefreshHz: 240 })
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"},{id:"3"},{id:"4"},{id:"6"},{id:"8"}]
            opt.start()
            tryCompare(opt, "phase", "done", 20000)
            verify(opt.steps.some(s => s.config.target === "monitor" && s.config.multiplier === 8), "8X at 240 Hz measured")
            compare(opt.best.config.target, "monitor", "240 FPS held on the model PC")
            compare(veyra.fgMultiplier, 8)
        }
        function test_lowest_latency_goal() {
            root.vfgLatency = 30
            veyra.preferences = ({ pushGameFps: 30, pushOptimizeGoal: "latency" })
            opt.start()
            tryCompare(opt, "phase", "done", 20000)
            verify(opt.best.config.backend !== "vfg", "the 30 ms method loses on latency")
            verify(opt.best.result.latency <= 20.5, "lowest measured latency wins (" + opt.best.result.latency + ")")
            compare(JSON.parse(veyra.preferences.pushBenchmark).goal, "latency")
        }
        function test_balanced_goal_keeps_quality_at_equal_rate() {
            root.vfgLatency = 30
            veyra.preferences = ({ pushGameFps: 30, pushOptimizeGoal: "balanced" })
            opt.start()
            tryCompare(opt, "phase", "done", 20000)
            compare(opt.best.config.backend, "vfg", "same 120 FPS: the better picture method, latency only breaks ties")
        }
        function test_detects_a_60_fps_game() {
            veyra.liveStats = Object.assign({}, veyra.liveStats, { contentFps: 60 })
            veyra.displayRefreshHz = 144
            opt.start()
            tryVerify(() => opt.phase !== "running", 15000)
            compare(opt.gameFps, 60)
            verify(!opt.steps.some(s => s.config.target === "60" && s.config.backend), "no 60 FPS target for a 60 FPS game")
        }
    }
}
