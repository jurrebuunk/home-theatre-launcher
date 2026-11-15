import QtQuick

FocusScope {
    id: root
    property bool unavailable: false
    signal closeRequested()

    function focusDefault() { retry.forceActiveFocus() }

    // Opaque by default; "transparent" over the menu background.
    property color backdrop: Theme.background
    Rectangle { anchors.fill: parent; color: root.backdrop }

    Column {
        anchors.centerIn: parent
        spacing: Theme.px(44)
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: "bluetooth-off"
            size: Theme.px(160)
            tint: Theme.error
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            role: "display"
            text: root.unavailable ? "Bluetooth service unavailable" : "No Bluetooth adapter"
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(16)
            FlatButton {
                id: retry
                text: "Retry"
                iconName: "refresh"
                KeyNavigation.right: closeButton
                onActivated: Bt.retry()
            }
            FlatButton {
                id: closeButton
                text: "Close"
                iconName: "x"
                KeyNavigation.left: retry
                onActivated: root.closeRequested()
            }
        }
    }
}
