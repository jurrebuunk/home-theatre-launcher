import QtQuick

// Shown instead of the launcher when the configuration is invalid.
Rectangle {
    color: Theme.background
    MouseArea { anchors.fill: parent }

    Column {
        anchors.centerIn: parent
        width: Theme.px(1200)
        spacing: Theme.px(40)
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "error-square"
            size: Theme.px(150)
            tint: Theme.error
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            role: "display"
            text: "Configuration error"
        }
        Rectangle {
            width: parent.width
            height: errors.implicitHeight + Theme.px(48)
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            Label {
                id: errors
                anchors.fill: parent
                anchors.margins: Theme.px(24)
                role: "value"
                color: Theme.muted
                wrapMode: Text.Wrap
                elide: Text.ElideNone
                text: Cfg.errors.join("\n")
            }
        }
    }
}
