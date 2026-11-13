pragma Singleton
import QtQuick

// Design tokens from the generated configuration. Sizes are px at 1920×1080
// multiplied by uiScale; px() maps them to the real output.
QtObject {
    property real screenHeight: 1080
    readonly property real k: (Cfg.layout.uiScale || 1) * screenHeight / 1080
    function px(v) { return Math.round(v * k) }
    function line(v) { return v > 0 ? Math.max(1, px(v)) : 0 }

    readonly property var layout: Cfg.layout
    readonly property color background: Cfg.colors.background
    readonly property color surface: Cfg.colors.surface
    readonly property color surfaceAlt: Cfg.colors.surfaceAlt
    readonly property color text: Cfg.colors.text
    readonly property color muted: Cfg.colors.muted
    readonly property color faint: Cfg.colors.faint
    readonly property color border: Cfg.colors.border
    readonly property color borderStrong: Cfg.colors.borderStrong
    readonly property color accent: Cfg.colors.accent
    readonly property color focus: Cfg.colors.focus
    readonly property color success: Cfg.colors.success
    readonly property color warning: Cfg.colors.warning
    readonly property color error: Cfg.colors.error

    readonly property string serif: Cfg.fonts.serif
    readonly property string mono: Cfg.fonts.mono
    function font(role) { return px(Cfg.fonts.sizes[role] || 28) }

    readonly property int borderWidth: line(layout.borderWidth)
    readonly property int focusWidth: line(layout.focusWidth)
    readonly property int focusOffset: px(layout.focusOffset)
    readonly property int transitionMs: layout.transitionMs

    function isSelect(key) { return Cfg.selectKeys.indexOf(key) >= 0 }
    function isBack(key) { return Cfg.backKeys.indexOf(key) >= 0 }

    function hex(c) { return String(c).substring(1) }
    function icon(name, c) { return "image://icon/" + name + "?c=" + hex(c) }
    function appIcon(path, c) {
        if (!path)
            return icon("device", c)
        if (path.startsWith("builtin:"))
            return "image://icon/builtin/" + path.substring(8) + "?c=" + hex(c)
        return "image://icon/file" + path + "?c=" + hex(c)
    }
    function deviceIcon(kind) {
        return ["gamepad", "keyboard", "mouse", "headphones", "speaker", "phone", "remote"].indexOf(kind) >= 0
            ? kind : "bluetooth"
    }
}
