// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Glide for Linux and macOS: home (sources and quick setup), player, settings.
import QtQuick
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win
    width: 1180
    height: 800
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    color: Theme.bgDeep
    title: "Glide"
    flags: app.alwaysOnTop ? (Qt.Window | Qt.WindowStaysOnTopHint) : Qt.Window

    property string page: app.startPage === "player" || app.startPage === "settings" ? app.startPage : "home"
    function go(p) { page = p; if (p !== "player" && visibility === Window.FullScreen) showNormal() }
    function toggleFullscreen() { if (visibility === Window.FullScreen) showNormal(); else showFullScreen() }

    Connections {
        target: app
        function onRunningChanged() { if (app.running) win.go("player") }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: win.page === "player" ? 1 : (win.page === "settings" ? 2 : 0)
        HomePage { onOpenSettings: win.go("settings"); onOpenPlayer: win.go("player") }
        PlayerPage {
            active: win.page === "player"
            fullscreen: win.visibility === Window.FullScreen
            onBack: { app.stop(); win.go("home") }
            onToggleFullscreen: win.toggleFullscreen()
        }
        SettingsPage { onBack: win.go("home") }
    }

    Shortcut { sequences: ["F11", "Ctrl+F"]; onActivated: win.toggleFullscreen() }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (win.visibility === Window.FullScreen) win.showNormal()
            else if (win.page === "settings") win.go("home")
        }
    }
    Shortcut { sequence: "Tab"; enabled: win.page === "player"; onActivated: app.showStats = !app.showStats }
}
