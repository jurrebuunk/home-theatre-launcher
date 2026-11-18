import QtQuick

// Button prompt for an action ("select", "back", "home", "delete", "menu", "alt", "volumeUp",
// "volumeDown") on the last-used device: real Xbox / PlayStation / Wii / keyboard glyphs, key caps for the
// remote, or the pictures of a custom device (behavior.input.devices), which fall back to its base.
Item {
    id: root
    property string action: "select"
    property int size: Theme.px(40)

    readonly property var custom: (Cfg.input.devices || []).find(d => d.id === Input.lastDevice) || null
    readonly property string customFile: custom && custom.glyphs ? (custom.glyphs[action] || "") : ""
    readonly property string device: custom ? (custom.base || "remote") : Input.lastDevice
    readonly property bool east: Cfg.input.confirmButton === "east"
    readonly property bool color: Cfg.layout.glyphStyle !== "mono"
    readonly property string gamepadAction: east && action === "select" ? "back"
                                           : east && action === "back" ? "select" : action

    readonly property string glyphFile: {
        if (device === "xbox") {
            if (gamepadAction === "select") return color ? "xbox/xbox_button_color_a" : "xbox/xbox_button_a"
            if (gamepadAction === "back") return color ? "xbox/xbox_button_color_b" : "xbox/xbox_button_b"
            if (gamepadAction === "delete") return color ? "xbox/xbox_button_color_x" : "xbox/xbox_button_x"
            if (gamepadAction === "menu") return "xbox/xbox_button_menu"
            if (gamepadAction === "alt") return color ? "xbox/xbox_button_color_y" : "xbox/xbox_button_y"
            return gamepadAction === "home" ? "xbox/xbox_guide" : ""
        }
        if (device === "playstation") {
            if (gamepadAction === "select") return color ? "playstation/playstation_button_color_cross" : "playstation/playstation_button_cross"
            if (gamepadAction === "back") return color ? "playstation/playstation_button_color_circle" : "playstation/playstation_button_circle"
            if (gamepadAction === "delete") return color ? "playstation/playstation_button_color_square" : "playstation/playstation_button_square"
            if (gamepadAction === "menu") return "playstation/playstation5_button_options"
            if (gamepadAction === "alt") return color ? "playstation/playstation_button_color_triangle" : "playstation/playstation_button_triangle"
            return "" // no PS-button glyph in the pack: in-style home icon
        }
        if (device === "wii") { // Wii Remote held upright: A / B, 1 / 2, +
            const b = ({ select: "a", back: "b", delete: "1", menu: "plus", alt: "2", home: "home" })[gamepadAction]
            return b ? "wii/wii_button_" + b : ""
        }
        return ""
    }
    // Keyboard prompts: one key picture each, Ctrl+Enter as two.
    readonly property var keyFiles: device !== "keyboard" ? []
        : ({ select: ["enter"], back: ["escape"], home: ["home"], delete: ["backspace"],
             menu: ["ctrl", "enter"], alt: ["f2"] })[action] || []
    readonly property var glyphFiles: keyFiles.length > 0 ? keyFiles.map(k => "keyboard/keyboard_" + k)
                                      : glyphFile !== "" ? [glyphFile] : []
    readonly property string keyText: {
        if (device === "remote")
            return ({ select: "OK", back: "Back", home: "Home" })[action] || ""
        return ""
    }
    // Remotes have no Delete / Menu buttons: the on-screen keys do that.
    readonly property bool available: customFile !== "" || glyphFiles.length > 0 || keyText !== ""
                                      || (device === "playstation" && gamepadAction === "home")

    implicitWidth: customFile !== "" ? size
                   : keyText !== "" ? keyCap.width
                   : glyphFiles.length > 1 ? glyphRow.width : size
    implicitHeight: size

    Image {
        visible: root.customFile !== ""
        anchors.centerIn: parent
        width: root.size * 1.2
        height: width
        sourceSize: Qt.size(width, height)
        source: root.customFile !== ""
                ? "image://icon/file" + root.customFile + (root.custom.invert ? "?invert=1" : "") : ""
        smooth: true
    }
    Row {
        id: glyphRow
        visible: root.customFile === ""
        anchors.centerIn: parent
        spacing: -root.size * 0.15 // the pictures have transparent margins
        Repeater {
            model: root.glyphFiles
            Image {
                required property string modelData
                width: root.size * 1.25
                height: width
                sourceSize: Qt.size(width, height)
                source: "image://icon/glyph/" + modelData
                smooth: true
            }
        }
    }
    // The pack has no PS button: a filled disc with a home icon in the same style.
    Rectangle {
        visible: root.customFile === "" && root.device === "playstation" && root.gamepadAction === "home"
        anchors.centerIn: parent
        width: root.size * 0.95
        height: width
        radius: width / 2
        color: Theme.text
        Icon {
            anchors.centerIn: parent
            name: "home"
            tint: Theme.background
            size: parent.width * 0.7
        }
    }
    Rectangle {
        id: keyCap
        visible: root.customFile === "" && root.keyText !== ""
        anchors.verticalCenter: parent.verticalCenter
        width: keyLabel.implicitWidth + Theme.px(24)
        height: root.size
        color: "transparent"
        border.color: Theme.muted
        border.width: Theme.borderWidth
        Label {
            id: keyLabel
            anchors.centerIn: parent
            role: "value"
            text: root.keyText
            color: Theme.text
        }
    }
}
