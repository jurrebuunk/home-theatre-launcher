import QtQuick

// One device. Focusing the row expands it into its action buttons:
// paired → Connect/Disconnect, Trust/Untrust, Forget; nearby → Pair.
FocusScope {
    id: root
    property string path
    property string name
    property string kind
    property bool connected
    property bool trusted
    property bool paired
    property bool busy
    signal upPressed()
    signal downPressed()
    signal forgetRequested(string path, string name)

    readonly property bool expanded: activeFocus
    readonly property bool hasError: Bt.errorPath === path && Bt.lastError !== ""

    width: parent ? parent.width : 0
    height: Theme.px(Theme.layout.rowHeight) + (expanded ? actions.height + Theme.px(24) : 0)
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
        height: Theme.px(Theme.layout.rowHeight)

        Icon {
            id: kindIcon
            anchors.left: parent.left
            anchors.leftMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            name: Theme.deviceIcon(root.kind)
            size: Theme.px(40)
            tint: Theme.muted
        }
        Label {
            anchors.left: kindIcon.right
            anchors.leftMargin: Theme.px(28)
            anchors.right: status.left
            anchors.rightMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            text: root.name
        }
        Row {
            id: status
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(40)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(32)
            Label {
                visible: root.busy
                role: "supporting"
                color: Theme.muted
                text: "Working…"
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: root.paired
                name: "link"
                size: Theme.px(32)
                tint: root.connected ? Theme.accent : Theme.faint
                opacity: root.expanded ? 0.4 : 1
            }
            Icon {
                visible: root.paired
                name: "shield-check"
                size: Theme.px(32)
                tint: root.trusted ? Theme.accent : Theme.faint
                opacity: root.expanded ? 0.4 : 1
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
            text: !root.paired ? "Pair" : root.connected ? "Disconnect" : "Connect"
            iconName: !root.paired ? "bluetooth" : "link"
            onActivated: !root.paired ? Bt.pair(root.path)
                       : root.connected ? Bt.disconnectDevice(root.path) : Bt.connectDevice(root.path)
            KeyNavigation.right: root.paired ? trust : null
        }
        FlatButton {
            id: trust
            visible: root.paired
            compact: true
            text: root.trusted ? "Untrust" : "Trust"
            iconName: "shield-check"
            onActivated: Bt.setTrusted(root.path, !root.trusted)
            KeyNavigation.left: first
            KeyNavigation.right: forget
        }
        FlatButton {
            id: forget
            visible: root.paired
            compact: true
            text: "Forget"
            iconName: "trash"
            onActivated: root.forgetRequested(root.path, root.name)
            KeyNavigation.left: trust
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
        text: Bt.lastError
    }

    Keys.onUpPressed: root.upPressed()
    Keys.onDownPressed: root.downPressed()
}
