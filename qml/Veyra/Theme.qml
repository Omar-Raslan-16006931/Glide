// Design tokens, transcribed from the approved prototype's app.css.
//
// Every classic value here is copied from the prototype rather than chosen. That
// is the point of this file: the interface has to match the approved design, so
// the numbers live in one place that can be diffed against app.css.
//
// 设置 → 外观 → 界面配色 adds a second palette, 日落 (dusk, the default since
// 2026-11-14): the same depths tinted indigo-violet, a sunset accent that fills
// switches, sliders, primary buttons and seek bars as a peach -> pink gradient
// (VAccentFill.qml), and a twilight backdrop (tools/qt_probe/make-backdrop-dusk.py).
// Its values are our own, chosen after sunset skies. "classic" keeps every value
// of the prototype exactly.
pragma Singleton
import QtQuick

QtObject {
    // Glide appearance (设置 → 外观, preference pushTheme; Main.qml binds it):
    // "default" the design's dark grey, "black" near-black surfaces (OLED), "light"
    // light surfaces with dark text. Every surface, text and accent below follows it;
    // ink(a) replaces the design's white-at-alpha overlays so tracks, fills and
    // hover states stay visible on light surfaces too. Contrast: t1/t2/t3 on card2
    // are at least 4.5:1 in every mode.
    property string mode: "default"
    readonly property bool light: mode === "light"
    readonly property bool black: mode === "black"
    function ink(a) { return light ? Qt.rgba(0.07, 0.08, 0.06, Math.min(1, a * 1.15)) : Qt.rgba(1, 1, 1, a) }
    // Text and icons drawn over the video picture or a dark chip stay white in every mode.
    readonly property color onVideo: "#FFFFFF"

    // --- surfaces (app.css .vy) -------------------------------------------
    readonly property color card: light ? "#FFFFFF" : black ? "#08080A" : "#131317"
    readonly property color card2: light ? "#F3F5F0" : black ? "#0D0D10" : "#19191E"
    readonly property color card3: light ? "#E4E7E0" : black ? "#17171B" : "#24242A"
    readonly property color popover: light ? "#FFFFFF" : black ? "#101013" : "#1B1B20"
    readonly property color dialog: light ? "#FFFFFF" : black ? "#0A0A0C" : "#141418"
    readonly property color stroke: light ? Qt.rgba(0, 0, 0, 0.10) : Qt.rgba(1, 1, 1, black ? 0.08 : 0.07)
    readonly property color stroke2: light ? Qt.rgba(0, 0, 0, 0.18) : Qt.rgba(1, 1, 1, black ? 0.14 : 0.13)
    // The app background: a radial gradient in the prototype, approximated here
    // with the three stops it interpolates between.
    readonly property color bgOuter: light ? "#DFE3DA" : black ? "#000000" : "#070709"
    readonly property color bgMid: light ? "#E9ECE5" : black ? "#010102" : "#0C0C10"
    readonly property color bgInner: light ? "#F4F6F1" : black ? "#050506" : "#17171C"
    // Veyra 2.1.9 palette API, kept so upstream components work unchanged. Glide has
    // its own appearance modes (above) instead of Veyra's dusk palette, so it is always
    // "classic" here: shade() returns the colour as written, no accent gradient.
    readonly property string paletteName: "classic"
    readonly property bool dusk: false
    readonly property color videoBlack: "#000000"
    // Dark neutrals some components still spell out (the cinema pill, the node
    // canvas, the dock): exactly that colour in the classic palette, the same
    // depth tinted indigo-violet in the dusk one.
    function shade(r, g, b, a) {
        const alpha = a === undefined ? 1 : a
        return dusk ? Qt.rgba(Math.min(255, r + 2) / 255, Math.max(0, g - 1) / 255, Math.min(255, b + 14) / 255, alpha)
                    : Qt.rgba(r / 255, g / 255, b / 255, alpha)
    }

    // --- text -------------------------------------------------------------
    readonly property color t1: light ? "#15171A" : "#F3F3F5"
    readonly property color t2: light ? Qt.rgba(0.08, 0.09, 0.10, 0.74) : Qt.rgba(0.953, 0.953, 0.961, 0.62)
    // Glide: secondary text a little brighter than the design's .38, so hints
    // stay readable on the dark cards (contrast about 4.5:1 on card2).
    readonly property color t3: light ? Qt.rgba(0.08, 0.09, 0.10, 0.62) : Qt.rgba(0.953, 0.953, 0.961, 0.48)
    // Row labels: between t1 and t2, so a label reads before its hint.
    readonly property color tLabel: light ? Qt.rgba(0.08, 0.09, 0.10, 0.90) : Qt.rgba(0.953, 0.953, 0.961, 0.88)
    readonly property real fsHint: 11.5

    // --- accent and state -------------------------------------------------
    // 设置 → 强调色: Glide green (default) or the design's .accent-white variant.
    // The saved preference value stays "orange" (Veyra's name for the default) so
    // existing settings files keep working; Glide draws it in its brand green.
    property string accentName: "orange"
    readonly property bool accentWhite: accentName === "white"
    // Light mode: a deeper green (and near-black for the white variant), since the
    // brand green is too light for text and icons on white.
    readonly property color accent: accentWhite ? (light ? "#15171A" : "#F3F3F5") : (light ? "#2F7A06" : "#89F52A")
    // The logo's green, also with the white accent (the Glide wordmark's "FPS").
    readonly property color brandGreen: light ? "#2F7A06" : "#89F52A"
    readonly property color accentInk: accentWhite ? (light ? "#FFFFFF" : "#0B0B0E") : (light ? "#FFFFFF" : "#0D1A04")
    readonly property color accentSoft: accentWhite ? ink(0.1) : (light ? Qt.rgba(0.184, 0.478, 0.024, 0.12) : Qt.rgba(0.537, 0.961, 0.165, 0.14))
    readonly property color accentGlow: accentWhite ? ink(0.35) : (light ? Qt.rgba(0.184, 0.478, 0.024, 0.30) : Qt.rgba(0.537, 0.961, 0.165, 0.35))
    // 设置 → 背景渐变强度: 0 none, 1 light (the design default), 2 medium.
    property int backdropLevel: 1
    readonly property color ok: light ? "#17824A" : "#3DDC84"
    readonly property color okGlow: Qt.rgba(0.239, 0.863, 0.518, 0.16)
    readonly property color warn: light ? "#9A6200" : "#F5C84B"
    // Veyra 2.1.9 accent-gradient API (VAccentFill.qml): a plain fill in Glide.
    readonly property bool accentGradient: false
    readonly property color accentFrom: accent
    readonly property color accentTo: accent
    readonly property color accentFar: accent
    readonly property string backdropImage: "backdrop" + (backdropLevel === 2 ? "-strong" : "") + ".png"
    // Glide logo (the chevrons, scripts/brand/make_icon.py) in every mode.
    readonly property string logoImage: "logo.png"
    readonly property color warnGlow: Qt.rgba(0.961, 0.784, 0.294, 0.16)
    readonly property color err: light ? "#C62828" : "#FF5D5D"
    readonly property color errGlow: Qt.rgba(1, 0.365, 0.365, 0.16)
    readonly property color exp: light ? "#5B3FC4" : "#B79BFF"
    readonly property color expSoft: Qt.rgba(0.718, 0.608, 1.0, 0.14)

    // --- geometry (--r-win / --r-card / --r-ctl) --------------------------
    // The window radius is 8px, small, because Windows' own default is small.
    // The video surface is NEVER rounded: .video in the prototype carries
    // border-radius:0!important because rounding crops the picture.
    readonly property int rWindow: 8
    readonly property int rCard: 14
    readonly property int rCtl: 10

    // --- motion (--spring / --spring-soft / --out) ------------------------
    // easing.bezierCurve takes whole cubic segments, six numbers each
    // [c1x, c1y, c2x, c2y, endX, endY], the last one ending at (1, 1). Any other
    // length is rejected and the animation silently runs linear (G0.5/G0.6 caught
    // the old four-number curves doing exactly that).
    // --spring and --spring-soft are CSS linear() functions: straight lines between
    // stops. A cubic with its control points at 1/3 and 2/3 of a line is that line,
    // so each stop becomes one segment and the curve matches the design exactly,
    // overshoot included. Stops without a position are spread evenly, as in CSS.
    // At most 10 segments: Qt 6.8.3 corrupts the heap when a BezierSpline curve of
    // 11+ segments is destroyed (reproduced in C++ with no QML, G1.1). --spring has
    // 12, so its 1.035@28.4% and .998@38.5% stops are dropped; the remaining line
    // stays within 0.007 of the design curve (veyra_qml_easing_tests checks 0.01).
    // Reduced motion (the design's .vy.reduced: every transition and animation at
    // 0s). Main.qml binds it to the saved setting or the --reduced-motion switch;
    // every duration in the app goes through d(), so one flag covers all of them.
    // Endless animations also stop on it: a zero-length loop would spin.
    property bool reduced: false
    function d(ms) { return reduced ? 0 : ms }

    readonly property int durFast: 150
    readonly property int durNormal: 240
    readonly property int durSlow: 450
    function linearEasing(stops) {
        var out = []
        for (var i = 1; i < stops.length; ++i) {
            var a = stops[i - 1], b = stops[i]
            out.push(a[0] + (b[0] - a[0]) / 3, a[1] + (b[1] - a[1]) / 3,
                     a[0] + (b[0] - a[0]) * 2 / 3, a[1] + (b[1] - a[1]) * 2 / 3,
                     b[0], b[1])
        }
        return out
    }
    // linear(0,.009,.035 2.1%,.141 4.4%,.723 12.9%,.938 16.7%,1.017 20.2%,1.043 24%,
    //        1.035 28.4%,.998 38.5%,.99 44.1%,1.001 60.7%,1)
    readonly property var spring: linearEasing([
        [0, 0], [0.0105, 0.009], [0.021, 0.035], [0.044, 0.141], [0.129, 0.723],
        [0.167, 0.938], [0.202, 1.017], [0.24, 1.043],
        [0.441, 0.99], [0.607, 1.001], [1, 1]])
    // linear(0,.02,.08 3.2%,.3 7.5%,.84 17%,1.01 25%,1.025 31%,1.004 45%,1)
    readonly property var springSoft: linearEasing([
        [0, 0], [0.016, 0.02], [0.032, 0.08], [0.075, 0.3], [0.17, 0.84],
        [0.25, 1.01], [0.31, 1.025], [0.45, 1.004], [1, 1]])
    // cubic-bezier(.2,.8,.2,1)
    readonly property var easeOut: [0.2, 0.8, 0.2, 1.0, 1.0, 1.0]

    // --- type (--f-ui / --f-mono and the .h1/.h2/.h3 scale) ---------------
    // --f-ui is "Geist", "Noto Sans SC", ...: Geist is not on this machine (D4), so
    // the design's own second choice, which is also what Edge rendered the reference
    // with. Noto Sans SC ships as a variable font; Qt 6.8.3 maps font.weight 600 to a
    // thin instance of it (500 and 700 are fine), so every DemiBold also sets the wght
    // axis directly (G1.3, tmp/.../min/Font.qml).
    // Per interface language (Qt.uiLanguage, set by the bridge): Japanese kana and kanji
    // and Traditional characters read best in their own Windows UI fonts.
    readonly property string fontUi: Qt.uiLanguage === "ja" ? "Yu Gothic UI"
                                   : Qt.uiLanguage === "zh-TW" ? "Microsoft JhengHei UI"
                                   : Qt.uiLanguage === "ar" ? "Segoe UI" : "Noto Sans SC"
    readonly property var axesDemiBold: ({ "wght": 600 })
    // 500: the design's .seek .peek time strip is `font: 500 11px/1 var(--f-mono)`.
    // Same reason as DemiBold - 600 lands on a thin instance, so the axis is set
    // explicitly rather than left to font.weight.
    readonly property var axesMedium: ({ "wght": 500 })
    readonly property string fontMono: "Consolas"
    readonly property real fsEyebrow: 11
    readonly property real fsSmall: 11.5
    readonly property real fsBody: 12.5
    readonly property real fsH3: 15
    readonly property real fsH2: 18
    readonly property real fsH1: 26

    // --- component metrics from the CSS -----------------------------------
    readonly property int ctlHeight: 32      // .btn / .pill
    readonly property int rowMinHeight: 40   // .row (Glide: 38 + room to breathe)
    readonly property int switchWidth: 34    // .sw
    readonly property int switchHeight: 20
    readonly property int switchKnob: 14
    readonly property int sliderKnob: 13
    readonly property int tagHeight: 20
    readonly property int dialogWidth: 640
    readonly property int popoverMinWidth: 220
}
