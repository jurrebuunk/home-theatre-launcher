import QtQuick

// One Wi-Fi network. Focusing the row expands it into its actions:
// Connect, or Disconnect / Forget for the active or a saved network.
FocusScope {
    id: root
    property string ssid
    property int bars
    property bool secured
    property bool active
    property bool saved
    signal upPressed()
    signal downPressed()
    signal connectRequested(string ssid)

    readonly property bool expanded: activeFocus
    readonly property bool working: Nm.connecting.ssid === ssid && Nm.connecting.stage === "working"
    readonly property bool hasError: Nm.connecting.ssid === ssid && Nm.connecting.stage === "error"

    width: parent ? parent.width : 0
    height: Theme.px(Theme.layout.rowHeight + 8) + (expanded ? actions.height + Theme.px(24) : 0)
            + (hasError ? errorText.height + Theme.px(8) : 0)

    function focusFirst() { first.forceActiveFocus() }

    Rectangle {
        anchors.fill: parent
        color: root.expanded ? Theme.surfaceAlt : "transparent"
    }
    Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !root.expanded
    }

    Item {
        id: header
        width: parent.width
        height: Theme.px(Theme.layout.rowHeight + 8)

        // Signal: all arcs dimmed, the received ones drawn on top.
        Item {
            id: signal
            anchors.left: parent.left
            anchors.leftMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(40)
            height: width
            Icon { name: "wifi"; size: parent.width; tint: Qt.darker(Theme.faint, 1.5) }
            Icon { name: Theme.wifiIcon(root.bars); size: parent.width; tint: Theme.text }
        }
        Label {
            anchors.left: signal.right
            anchors.leftMargin: Theme.px(28)
            anchors.right: status.left
            anchors.rightMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            text: root.ssid
        }
        Row {
            id: status
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(40)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(32)
            Label {
                visible: root.working
                role: "supporting"
                color: Theme.muted
                text: "Connecting…"
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: root.secured
                name: "lock"
                size: Theme.px(32)
                tint: Theme.muted
                anchors.verticalCenter: parent.verticalCenter
            }
            Item {
                width: Theme.px(32)
                height: Theme.px(32)
                anchors.verticalCenter: parent.verticalCenter
                Icon {
                    visible: root.active
                    name: "link"
                    size: parent.width
                    tint: Theme.accent
                }
            }
        }
    }

    Row {
        id: actions
        visible: root.expanded
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(92)
        spacing: Theme.px(16)

        FlatButton {
            id: first
            compact: true
            focus: true
            enabled: !root.working
            text: root.active ? "Disconnect" : "Connect"
            iconName: "link"
            onActivated: root.active ? Nm.disconnectWifi() : root.connectRequested(root.ssid)
            KeyNavigation.right: root.saved ? forget : null
        }
        FlatButton {
            id: forget
            visible: root.saved
            compact: true
            text: "Forget"
            iconName: "trash"
            onActivated: Nm.forget(root.ssid)
            KeyNavigation.left: first
        }
    }

    Label {
        id: errorText
        visible: root.hasError
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.px(8)
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(92)
        role: "supporting"
        color: Theme.error
        text: Nm.connecting.message || ""
    }

    Keys.onUpPressed: root.upPressed()
    Keys.onDownPressed: root.downPressed()
}
