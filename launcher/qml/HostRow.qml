import QtQuick

// One row of the Hosts page. A paired host expands into its actions while it
// has focus (Wake or Open, Main host, Remove); a host found on the network
// and the "Add by address" row start pairing.
FocusScope {
    id: root
    property string kind: "paired" // paired | found | add
    property var host: ({})
    signal upPressed()
    signal downPressed()
    signal openRequested(string uuid)
    signal pairRequested(string address)
    signal addRequested()

    readonly property bool expanded: kind === "paired" && activeFocus
    readonly property string hostState: host.state || ""
    readonly property bool canWake: hostState === "offline" && !!host.canWake
    readonly property bool canOpen: hostState === "online"
    readonly property bool canMain: !host.main
    property bool confirmRemove: false
    onActiveFocusChanged: if (!activeFocus) confirmRemove = false

    width: parent ? parent.width : 0
    height: Theme.px(Theme.layout.rowHeight + 8) + (expanded ? actions.height + Theme.px(24) : 0)

    function focusFirst() {
        if (kind !== "paired") {
            forceActiveFocus()
            return
        }
        // The buttons are hidden until the row has focus: decide by state.
        ;(canWake ? wake : canOpen ? open : canMain ? main : removeButton).forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: root.expanded ? Theme.surfaceAlt : "transparent"
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.px(Theme.layout.rowHeight + 8)
        visible: root.kind !== "paired" && root.activeFocus
        color: Theme.accent
    }
    Item {
        width: parent.width
        height: Theme.px(Theme.layout.rowHeight + 8)
        z: 10
        FocusFrame { shown: root.kind !== "paired" && root.activeFocus }
    }
    Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !root.expanded
    }

    readonly property color fg: root.kind !== "paired" && root.activeFocus ? Theme.background : Theme.text

    Item {
        id: header
        width: parent.width
        height: Theme.px(Theme.layout.rowHeight + 8)

        Icon {
            id: lead
            anchors.left: parent.left
            anchors.leftMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            name: root.kind === "add" ? "plus" : "monitor"
            size: Theme.px(38)
            tint: root.kind !== "paired" && root.activeFocus ? Theme.background : root.kind === "add" ? Theme.muted : Theme.text
        }
        Label {
            anchors.left: lead.right
            anchors.leftMargin: Theme.px(28)
            anchors.right: address.left
            anchors.rightMargin: Theme.px(24)
            anchors.verticalCenter: parent.verticalCenter
            role: root.kind === "add" ? "supporting" : "body"
            font.pixelSize: Theme.px(30)
            text: root.kind === "add" ? "Add by address" : root.host.name || ""
            color: root.kind === "add" && !root.activeFocus ? Theme.muted : root.fg
        }
        Label {
            id: address
            x: parent.width * 0.68
            anchors.verticalCenter: parent.verticalCenter
            role: "value"
            font.pixelSize: Theme.px(21)
            text: root.host.address || ""
            color: root.kind !== "paired" && root.activeFocus ? Theme.background : Theme.muted
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(28)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(26)
            Icon {
                visible: root.kind === "paired"
                anchors.verticalCenter: parent.verticalCenter
                name: root.host.main ? "star-filled" : "star"
                size: Theme.px(32)
                tint: root.host.main ? Theme.accent : Qt.darker(Theme.faint, 1.4)
            }
            Item {
                visible: root.kind === "paired"
                width: Theme.px(32)
                height: width
                anchors.verticalCenter: parent.verticalCenter
                Icon {
                    id: stateIcon
                    visible: root.hostState !== "checking"
                    name: root.hostState === "online" ? "link" : "moon"
                    size: parent.width
                    tint: root.hostState === "online" || root.hostState === "waking" ? Theme.accent : Theme.muted
                    SequentialAnimation on opacity {
                        running: root.hostState === "waking"
                        loops: Animation.Infinite
                        onRunningChanged: if (!running) stateIcon.opacity = 1
                        NumberAnimation { to: 0.3; duration: 700 }
                        NumberAnimation { to: 1; duration: 700 }
                    }
                }
            }
            Icon {
                visible: root.kind === "found"
                anchors.verticalCenter: parent.verticalCenter
                name: "plus"
                size: Theme.px(32)
                tint: root.activeFocus ? Theme.background : Theme.muted
            }
        }
    }

    Row {
        id: actions
        visible: root.expanded
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(90)
        spacing: Theme.px(16)

        FlatButton {
            id: wake
            visible: root.canWake
            compact: true
            text: "Wake"
            iconName: "power"
            onActivated: Moonlight.wake(root.host.uuid)
            KeyNavigation.right: root.canMain ? main : removeButton
        }
        FlatButton {
            id: open
            visible: root.canOpen
            compact: true
            text: "Open"
            iconName: "play"
            onActivated: root.openRequested(root.host.uuid)
            KeyNavigation.right: root.canMain ? main : removeButton
        }
        FlatButton {
            id: main
            visible: root.canMain
            compact: true
            text: "Main host"
            iconName: "star"
            onActivated: Moonlight.setMain(root.host.uuid)
            KeyNavigation.left: root.canWake ? wake : root.canOpen ? open : null
            KeyNavigation.right: removeButton
        }
        FlatButton {
            id: removeButton
            compact: true
            text: root.confirmRemove ? "Remove " + (root.host.name || "") + "?" : "Remove"
            iconName: "trash"
            onActivated: {
                if (!root.confirmRemove) {
                    root.confirmRemove = true
                    return
                }
                Moonlight.remove(root.host.uuid)
            }
            KeyNavigation.left: root.canMain ? main : root.canWake ? wake : root.canOpen ? open : null
        }
    }

    // Losing the focused button (e.g. Wake once the host is up) keeps focus in the row.
    onHostStateChanged: if (expanded && !(wake.activeFocus || open.activeFocus || main.activeFocus || removeButton.activeFocus))
                            Qt.callLater(focusFirst)

    Keys.onUpPressed: root.upPressed()
    Keys.onDownPressed: root.downPressed()
    Keys.onPressed: (event) => {
        if (!Theme.isSelect(event.key) || root.kind === "paired")
            return
        event.accepted = true
        if (root.kind === "found")
            root.pairRequested(root.host.address)
        else
            root.addRequested()
    }
    MouseArea {
        anchors.fill: header
        onClicked: {
            root.focusFirst()
            if (root.kind === "found") root.pairRequested(root.host.address)
            else if (root.kind === "add") root.addRequested()
        }
    }
}
