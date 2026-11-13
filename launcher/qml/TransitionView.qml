import QtQuick

// Flat transition: app name, "Starting…"/"Closing…", indeterminate bar.
// No percentages, no artificial delays.
Rectangle {
    id: root
    property bool shown: false
    color: Theme.background
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.transitionMs } }

    // Swallow input while visible (Home is handled globally).
    MouseArea { anchors.fill: parent }

    Column {
        anchors.centerIn: parent
        spacing: Theme.px(16)
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            isApp: true
            appPath: Life.appIcon
            size: Theme.px(140)
            tint: Theme.accent
        }
        Item { width: 1; height: Theme.px(16) }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            role: "display"
            text: Life.appLabel
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            role: "supporting"
            color: Theme.muted
            text: Life.state === "closing" ? "Closing…" : "Starting…"
        }
        Item { width: 1; height: Theme.px(24) }
        IndeterminateBar {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(500)
            running: root.visible
        }
    }
}
