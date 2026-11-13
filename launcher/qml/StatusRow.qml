import QtQuick

// Quiet status row: Bluetooth (opens the Bluetooth menu), internet, clock.
Item {
    id: root
    property alias bluetoothItem: btItem
    signal openBluetooth()
    signal downPressed()

    height: Theme.px(Theme.layout.statusRowHeight)
    readonly property int iconSize: Theme.px(Theme.layout.statusIconSize)

    Item {
        id: btItem
        activeFocusOnTab: true
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: btRow.implicitWidth + Theme.px(24)
        height: root.iconSize + Theme.px(24)

        Rectangle {
            anchors.fill: parent
            color: btItem.activeFocus ? Theme.accent : "transparent"
        }
        FocusFrame {}
        Row {
            id: btRow
            anchors.centerIn: parent
            spacing: Theme.px(16)
            Icon {
                name: Bt.hasAdapter ? "bluetooth" : "bluetooth-off"
                size: root.iconSize
                tint: btItem.activeFocus ? Theme.background : (Bt.powered ? Theme.muted : Theme.faint)
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                role: "value"
                visible: Bt.connectedCount > 0
                text: Bt.connectedCount
                color: btItem.activeFocus ? Theme.background : Theme.muted
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Keys.onDownPressed: root.downPressed()
        Keys.onPressed: (event) => {
            if (Theme.isSelect(event.key)) {
                event.accepted = true
                root.openBluetooth()
            }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: root.openBluetooth()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.px(40)

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(12)
            readonly property color tone: Net.state === "online" ? Theme.muted
                : Net.state === "checking" ? Theme.faint
                : Net.reason === "nolink" ? Theme.error : Theme.warning
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                size: root.iconSize
                name: Net.state === "offline" && Net.reason === "nolink" ? "ethernet-off" : "ethernet"
                tint: parent.tone
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                role: "value"
                visible: Net.state !== "online"
                color: parent.tone
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
