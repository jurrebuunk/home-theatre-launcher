import QtQuick

// Quiet status row: Bluetooth (opens the Bluetooth menu), network (opens the
// Network screen), clock.
Item {
    id: root
    property alias bluetoothItem: btItem
    signal openBluetooth()
    signal openNetwork()
    signal downPressed()

    height: Theme.px(Theme.layout.statusRowHeight)
    readonly property int iconSize: Theme.px(Theme.layout.statusIconSize)

    component StatusButton: Item {
        id: button
        default property alias content: row.data
        signal activated()
        activeFocusOnTab: true
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        width: row.implicitWidth + Theme.px(24)
        height: root.iconSize + Theme.px(24)
        readonly property bool focused: activeFocus

        Rectangle {
            anchors.fill: parent
            color: button.activeFocus ? Theme.accent : "transparent"
        }
        FocusFrame {}
        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.px(12)
        }
        Keys.onDownPressed: root.downPressed()
        Keys.onPressed: (event) => {
            if (Theme.isSelect(event.key)) {
                event.accepted = true
                button.activated()
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: button.activated()
        }
    }

    StatusButton {
        id: btItem
        anchors.left: parent.left
        KeyNavigation.right: netItem
        onActivated: root.openBluetooth()
        Icon {
            name: Bt.hasAdapter ? "bluetooth" : "bluetooth-off"
            size: root.iconSize
            tint: btItem.focused ? Theme.background : (Bt.powered ? Theme.muted : Theme.faint)
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            role: "value"
            visible: Bt.connectedCount > 0
            text: Bt.connectedCount
            color: btItem.focused ? Theme.background : Theme.muted
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(40)

        StatusButton {
            id: netItem
            KeyNavigation.left: btItem
            onActivated: root.openNetwork()
            readonly property color tone: focused ? Theme.background
                : Net.state === "online" ? Theme.muted
                : Net.state === "checking" ? Theme.faint
                : Net.reason === "nolink" ? Theme.error : Theme.warning
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                size: root.iconSize
                name: Theme.networkIcon()
                tint: netItem.tone
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                role: "value"
                visible: Net.state !== "online"
                color: netItem.tone
                text: Net.state === "checking" ? "checking"
                    : Net.reason === "nolink" ? "no cable" : "offline"
            }
        }
        Separator {
            vertical: true
            height: Theme.px(40)
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            role: "clock"
            text: Clock.text
            color: Theme.text
        }
    }

    Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
