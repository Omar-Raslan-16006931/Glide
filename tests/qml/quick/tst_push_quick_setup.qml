// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide quick setup: game frame rate, target frame rate, console and game
// profiles. UI logic only, against the fixture bridge in QuickSmokeTests.cpp.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 640; height: 640
    PushQuickSetup { id: quick; x: 10; y: 10; width: 560 }
    TestCase {
        name: "PushQuickSetup"
        when: windowShown
        function init() {
            veyra.preferences = ({exportStopsPlayback: false})
            veyra.fgMultiplier = 1
            veyra.fgEnabled = false
            veyra.outputRateMode = 0
            veyra.outputCustomFps = 0
            veyra.nrAnyEnabled = false
            veyra.presets = []
            veyra.currentPresetName = "自定义"
            veyra.lastAppliedPreset = -1
            veyra.fgAdaptive = false
            veyra.pacingRepair = false
            veyra.hudLock = false
            veyra.presentationOwned = false
            veyra.contentRate = 0
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"},{id:"3"},{id:"4"}]
            veyra.liveStats = ({running: false})
        }
        function cleanup() { init() }
        function control(name) { const c = findChild(quick, name); verify(c !== null, name); return c }

        function test_target_sets_multiplier_and_cap() {
            compare(quick.baseFps, 30)
            quick.applyTarget("120")
            compare(veyra.fgMultiplier, 4)
            verify(veyra.fgEnabled)
            compare(veyra.outputRateMode, 2)
            compare(veyra.outputCustomFps, 120)
            compare(quick.currentTarget, "120")
            quick.applyTarget("60")
            compare(veyra.fgMultiplier, 2)
            compare(veyra.outputCustomFps, 60)
            quick.applyTarget("monitor")
            compare(veyra.outputRateMode, 1)
            compare(veyra.fgMultiplier, 4, "240 Hz monitor: the largest offered multiplier")
            quick.applyTarget("off")
            verify(!veyra.fgEnabled)
            compare(quick.currentTarget, "off")
        }
        function test_target_removes_duplicate_frames() {
            compare(veyra.contentRate, 0)
            quick.applyTarget("120")
            compare(veyra.contentRate, 1, "a 30 FPS game in a 60 Hz capture: duplicates removed before generating")
            veyra.contentRate = 5
            quick.applyTarget("60")
            compare(veyra.contentRate, 5, "60->30 stays as chosen")
            veyra.contentRate = 0
            quick.applyTarget("off")
            compare(veyra.contentRate, 0, "nothing generated: nothing changed")
        }
        function test_game_fps_recomputes_the_multiplier() {
            quick.applyTarget("120")
            compare(veyra.fgMultiplier, 4)
            quick.setGameFps("60")
            compare(quick.baseFps, 60)
            compare(veyra.fgMultiplier, 2, "60 FPS game to 120: 2x")
            compare(quick.currentTarget, "120")
            quick.setGameFps("40")
            compare(veyra.fgMultiplier, 3, "40 FPS game to 120: 3x")
        }
        function test_target_the_game_already_reaches_turns_generation_off() {
            quick.applyTarget("60")
            verify(veyra.fgEnabled)
            quick.setGameFps("60")
            verify(!veyra.fgEnabled, "nothing to generate for a 60 FPS game at 60")
            const ids = control("pushfps-target").options.map(o => o.id)
            verify(ids.indexOf("60") < 0, "60 FPS is not offered for a 60 FPS game")
            quick.applyTarget("60")
            verify(!veyra.fgEnabled)
        }
        function test_console_profiles_keep_their_own_choices() {
            compare(quick.consoleId, "ps5")
            quick.applyTarget("120")
            quick.setEnhancement(true)
            quick.switchConsole("xbox")
            compare(quick.consoleId, "xbox")
            quick.applyTarget("off")
            quick.setEnhancement(false)
            quick.switchConsole("ps5")
            compare(quick.currentTarget, "120", "PS5 profile restored")
            verify(veyra.nrAnyEnabled)
            quick.switchConsole("xbox")
            compare(quick.currentTarget, "off", "Xbox profile restored")
            verify(!veyra.nrAnyEnabled)
            verify(quick.setupTip.indexOf("Xbox") === 0)
        }
        function test_game_profiles_save_apply_delete() {
            quick.setGameFps("30")
            quick.applyTarget("120")
            verify(quick.saveGame("  GTA VI  "))
            compare(quick.games.length, 1)
            compare(quick.activeGame, "GTA VI")
            compare(quick.games[0].console, "ps5")
            compare(quick.games[0].preset, "", "no saved preset matched")
            quick.applyTarget("off")
            quick.setGameFps("60")
            quick.applyGame("GTA VI")
            compare(quick.baseFps, 30)
            compare(quick.currentTarget, "120")
            compare(veyra.fgMultiplier, 4)
            // Saving the same name replaces it.
            verify(quick.saveGame("GTA VI"))
            compare(quick.games.length, 1)
            quick.deleteGame("GTA VI")
            compare(quick.games.length, 0)
            compare(quick.activeGame, "")
        }
        function test_game_profile_applies_its_preset() {
            veyra.presets = [{index: 0, name: "Default"}, {index: 3, name: "Sharp"}]
            veyra.currentPresetName = "Sharp"
            verify(quick.saveGame("Forza"))
            compare(quick.games[0].preset, "Sharp")
            quick.applyGame("Forza")
            compare(veyra.lastAppliedPreset, 3)
            // A preset that was deleted since is skipped, the rest still applies.
            veyra.presets = [{index: 0, name: "Default"}]
            veyra.lastAppliedPreset = -1
            quick.applyGame("Forza")
            compare(veyra.lastAppliedPreset, -1)
            compare(quick.activeGame, "Forza")
        }
        function test_bad_input_is_harmless() {
            verify(!quick.saveGame("   "))
            verify(quick.saveGame("x".repeat(80)))
            compare(quick.games[0].name.length, 40)
            veyra.preferences = ({pushProfiles: "{not json", pushConsole: "gamecube", pushGameFps: 999})
            compare(quick.games.length, 0)
            compare(quick.consoleId, "ps5")
            compare(quick.baseFps, 30)
            quick.applyGame("missing")
            compare(quick.activeGame, "")
        }
        // --- adaptive multiplier -------------------------------------------------
        function test_adaptive_makes_the_multiplier_a_ceiling() {
            quick.applyTarget("60")
            compare(veyra.fgMultiplier, 2)
            control("pushfps-adaptive").toggled(true)
            verify(veyra.fgAdaptive)
            compare(veyra.contentRate, 1, "duplicate removal on: the game's frame rate is measured")
            compare(veyra.fgMultiplier, 3, "60 FPS target: room for a dip to 20 FPS")
            quick.applyTarget("120")
            compare(veyra.fgMultiplier, 4, "the largest offered (4X) for 120 FPS")
            compare(veyra.outputCustomFps, 120)
            verify(quick.targetHint.length > 0)
            control("pushfps-adaptive").toggled(false)
            verify(!veyra.fgAdaptive)
            compare(veyra.fgMultiplier, 4, "back to the fixed choice: 30 FPS x 4 = 120")
            quick.applyTarget("60")
            compare(veyra.fgMultiplier, 2)
        }
        function test_adaptive_keeps_a_chosen_duplicate_mode() {
            veyra.contentRate = 5
            quick.applyTarget("120")
            quick.setAdaptive(true)
            compare(veyra.contentRate, 5, "60->30 stays as chosen")
        }
        function test_adaptive_needs_more_than_2x() {
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"}]
            quick.applyTarget("60")
            verify(!quick.adaptivePossible)
            verify(!control("pushfps-adaptive").enabled)
            veyra.fgAdaptive = true
            verify(!quick.adaptive, "a stored on means nothing without a choice above 2X")
        }
        function test_adaptive_live_hint() {
            quick.applyTarget("120")
            quick.setAdaptive(true)
            veyra.liveStats = ({running: true, fgMultiplierNow: 3, contentFps: 38})
            quick.liveStats = veyra.pushLiveStats()
            const row = control("pushfps-adaptive-row")
            compare(row.hint, qsTr("现在 %1×：游戏 %2 FPS → 约 %3 FPS").arg(3).arg(38).arg(114))
        }
        function test_profiles_remember_adaptive() {
            quick.applyTarget("120")
            quick.setAdaptive(true)
            verify(quick.saveGame("Elden"))
            quick.setAdaptive(false)
            quick.applyGame("Elden")
            verify(veyra.fgAdaptive, "the game profile brings adaptive back")
        }
        // --- Frame Pacing Repair and Live HUD Lock ----------------------------------
        function test_pacing_repair_turns_on_duplicate_removal() {
            quick.applyTarget("120")
            control("pushfps-pacing").toggled(true)
            verify(veyra.pacingRepair)
            compare(veyra.contentRate, 1, "the repair re-times what duplicate removal finds")
            control("pushfps-pacing").toggled(false)
            verify(!veyra.pacingRepair)
        }
        function test_pacing_live_hint() {
            quick.applyTarget("120")
            quick.setPacingRepair(true)
            const row = control("pushfps-pacing-row")
            quick.lastPacing = { pacingFrames: 100, pacingRepaired: 50 }
            veyra.liveStats = ({ running: true, pacingFrames: 130, pacingRepaired: 77 })
            // One timer tick, as the real one does.
            const st = veyra.pushLiveStats(), p = quick.lastPacing
            quick.pacingShare = (st.pacingRepaired - p.pacingRepaired) / (st.pacingFrames - p.pacingFrames)
            compare(row.hint, qsTr("正在修复：%1% 的画面重新对齐").arg(90))
            quick.pacingShare = 0
            compare(row.hint, qsTr("这款游戏的节奏已经均匀，无需修复"))
        }
        function test_hud_lock_switch() {
            quick.applyTarget("120")
            control("pushfps-hudlock").toggled(true)
            verify(veyra.hudLock)
            verify(control("pushfps-hudlock").checked)
            control("pushfps-hudlock").toggled(false)
            verify(!veyra.hudLock)
        }
        function test_hud_lock_reports_what_runs() {
            quick.applyTarget("120")
            veyra.hudLock = true
            quick.liveStats = ({ hudLockState: 2 })
            compare(control("pushfps-hudlock-row").hint, qsTr("实时 HUD 锁定没能在这台电脑上启动，补帧照常运行（原因见日志）"))
            quick.liveStats = ({ hudLockState: 1 })
            compare(control("pushfps-hudlock-row").hint, qsTr("正在运行：文字和准星在补出来的帧里保持清晰"))
            veyra.hudLock = false
            quick.liveStats = ({})
        }
        function test_own_tech_is_off_with_xess() {
            quick.applyTarget("120")
            veyra.presentationOwned = true
            verify(!control("pushfps-pacing").enabled)
            verify(!control("pushfps-hudlock").enabled)
            compare(control("pushfps-pacing-row").hint, qsTr("XeSS 自己控制节奏，不能同时使用"))
            veyra.presentationOwned = false
        }
        function test_profiles_remember_pacing_and_hud_lock() {
            quick.applyTarget("120")
            quick.setPacingRepair(true)
            veyra.hudLock = true
            verify(quick.saveGame("Bloodborne"))
            quick.setPacingRepair(false)
            veyra.hudLock = false
            quick.applyGame("Bloodborne")
            verify(veyra.pacingRepair && veyra.hudLock)
        }
        function test_rows_are_shown() {
            for (const n of ["pushfps-console-row", "pushfps-game-row", "pushfps-game-fps-row",
                             "pushfps-target-row", "pushfps-adaptive-row", "pushfps-pacing-row", "pushfps-hudlock-row", "pushfps-enhance-row", "pushfps-smooth-row", "pushfps-low-latency-row", "pushfps-lowest-latency-row"])
                verify(control(n).visible, n)
            verify(quick.height > 0)
        }
        function test_smooth_frame_pacing() {
            veyra.displaySync = 2; veyra.variableRefresh = false; veyra.outputRateMode = 0
            const sw = findChild(quick, "pushfps-smooth")
            verify(sw, "smooth frame pacing switch exists")
            verify(!sw.checked, "off while display sync is Automatic")
            sw.toggled(true)
            compare(veyra.displaySync, 1, "on: V-Sync")
            compare(veyra.variableRefresh, true, "on: variable refresh requested")
            compare(veyra.outputRateMode, 1, "on: output capped at the display")
            compare(veyra.preferences.pushSmoothPacing, true)
            verify(sw.checked)
            sw.toggled(false)
            compare(veyra.displaySync, 2, "off: back to Automatic")
            compare(veyra.variableRefresh, false)
            compare(veyra.outputRateMode, 0, "off: no output cap")
            compare(veyra.preferences.pushSmoothPacing, false)
        }
        function test_lowest_latency_one_tap_keeps_quality_and_restores() {
            // A G-SYNC/FreeSync screen on an NVIDIA card, display sync Automatic before.
            veyra.nvidiaLowLatencyAvailable = true; veyra.nvidiaLowLatency = 0; veyra.vrrState = 4
            veyra.displaySync = 2; veyra.variableRefresh = false; veyra.outputRateMode = 0
            veyra.fgMultiplier = 3; veyra.nrAnyEnabled = true; veyra.hudLock = true; veyra.pacingRepair = true
            const sw = control("pushfps-lowest-latency")
            verify(!sw.checked)
            sw.toggled(true)
            compare(veyra.nvidiaLowLatency, 1, "NVIDIA low latency: one-frame queue")
            compare(veyra.preferences.pushAutoFullscreen, true, "fullscreen on connect")
            compare(veyra.displaySync, 1); compare(veyra.variableRefresh, true); compare(veyra.outputRateMode, 1)
            verify(quick.lowestLatency && sw.checked)
            // Picture and frame generation untouched.
            compare(veyra.fgMultiplier, 3); verify(veyra.nrAnyEnabled); verify(veyra.hudLock); verify(veyra.pacingRepair)
            sw.toggled(false)
            compare(veyra.nvidiaLowLatency, 0, "restored")
            compare(veyra.preferences.pushAutoFullscreen, false)
            compare(veyra.displaySync, 2); compare(veyra.variableRefresh, false); compare(veyra.outputRateMode, 0)
            verify(!quick.lowestLatency)
            // Another GPU, no variable refresh: the low queue, display sync left alone (no tearing).
            veyra.nvidiaLowLatencyAvailable = false; veyra.vrrState = 0; veyra.fgLowQueue = false
            veyra.displaySync = 2
            sw.toggled(true)
            compare(veyra.fgLowQueue, true)
            compare(veyra.displaySync, 2, "no VRR: sync unchanged")
            verify(quick.lowestLatency)
            sw.toggled(false)
            compare(veyra.fgLowQueue, false)
            veyra.vrrState = 0
        }
    }
}
