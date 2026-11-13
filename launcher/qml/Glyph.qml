import QtQuick

// Button prompt for an action ("select", "back", "home") on the last-used
// device: real Xbox / PlayStation glyphs, key caps for keyboard and remote.
Item {
    id: root
    property string action: "select"
    property int size: Theme.px(40)

    readonly property string device: Input.lastDevice
    readonly property bool east: Cfg.input.confirmButton === "east"
    readonly property bool color: Cfg.layout.glyphStyle !== "mono"
    readonly property string gamepadAction: east && action === "select" ? "back"
                                           : east && action === "back" ? "select" : action

    readonly property string glyphFile: {
        if (device === "xbox") {
            if (gamepadAction === "select") return color ? "xbox/xbox_button_color_a" : "xbox/xbox_button_a"
            if (gamepadAction === "back") return color ? "xbox/xbox_button_color_b" : "xbox/xbox_button_b"
            return "xbox/xbox_guide"
        }
        if (device === "playstation") {
            if (gamepadAction === "select") return color ? "playstation/playstation_button_color_cross" : "playstation/playstation_button_cross"
            if (gamepadAction === "back") return color ? "playstation/playstation_button_color_circle" : "playstation/playstation_button_circle"
            return "" // no PS-button glyph in the pack: in-style home icon
        }
        return ""
    }
    readonly property string keyText: {
        if (device === "keyboard")
            return action === "select" ? "Enter" : action === "back" ? "Esc" : "Home"
        if (device === "remote")
            return action === "select" ? "OK" : action === "back" ? "Back" : "Home"
        return ""
    }

    implicitWidth: keyText !== "" ? keyCap.width : size
    implicitHeight: size

    Image {
        visible: root.glyphFile !== ""
        anchors.centerIn: parent
        width: root.size * 1.25
        height: width
        sourceSize: Qt.size(width, height)
        source: root.glyphFile !== "" ? "image://icon/glyph/" + root.glyphFile : ""
        smooth: true
    }
    Icon {
        visible: root.device === "playstation" && root.gamepadAction === "home"
        anchors.centerIn: parent
        name: "home-circle"
        tint: Theme.text
        size: root.size
    }
    Rectangle {
        id: keyCap
        visible: root.keyText !== ""
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
