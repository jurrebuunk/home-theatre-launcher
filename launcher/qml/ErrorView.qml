import QtQuick

// Readable error with Retry / Stop again and Dismiss — never a shell.
FocusScope {
    id: root
    property bool shown: false
    readonly property bool stopKind: Life.errorKind === "stop"
    property bool detailsOpen: false

    visible: shown
    onShownChanged: {
        detailsOpen = false
        if (shown)
            primary.forceActiveFocus()
    }

    Rectangle { anchors.fill: parent; color: Theme.background }
    MouseArea { anchors.fill: parent }

    Column {
        anchors.centerIn: parent
        spacing: Theme.px(40)

        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            name: root.stopKind ? "warning-triangle" : "error-square"
            tint: root.stopKind ? Theme.warning : Theme.error
            size: Theme.px(150)
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            role: "display"
            text: Life.errorTitle
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(16)
            FlatButton {
                id: primary
                text: root.stopKind ? "Stop again" : "Retry"
                iconName: root.stopKind ? "power" : "refresh"
                onActivated: root.stopKind ? Life.stopAgain() : Life.retry()
                KeyNavigation.right: dismiss
                KeyNavigation.down: details
            }
            FlatButton {
                id: dismiss
                text: "Dismiss"
                iconName: "x"
                onActivated: Life.dismiss()
                KeyNavigation.left: primary
                KeyNavigation.down: details
            }
        }
        Item {
            id: details
            anchors.horizontalCenter: parent.horizontalCenter
            activeFocusOnTab: true
            width: detailsRow.implicitWidth + Theme.px(24)
            height: detailsRow.implicitHeight + Theme.px(12)
            KeyNavigation.up: primary
            Rectangle { anchors.fill: parent; color: details.activeFocus ? Theme.accent : "transparent" }
            FocusFrame {}
            Row {
                id: detailsRow
                anchors.centerIn: parent
                spacing: Theme.px(12)
                Icon {
                    name: "chevron-small"
                    size: Theme.px(24)
                    rotation: root.detailsOpen ? 90 : 0
                    tint: details.activeFocus ? Theme.background : Theme.muted
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    role: "supporting"
                    text: "Details"
                    color: details.activeFocus ? Theme.background : Theme.muted
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Keys.onPressed: (event) => {
                if (Theme.isSelect(event.key)) {
                    event.accepted = true
                    root.detailsOpen = !root.detailsOpen
                }
            }
            MouseArea { anchors.fill: parent; onClicked: root.detailsOpen = !root.detailsOpen }
        }
        Rectangle {
            visible: root.detailsOpen
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(1100)
            height: detailsText.implicitHeight + Theme.px(48)
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            Label {
                id: detailsText
                anchors.fill: parent
                anchors.margins: Theme.px(24)
                role: "value"
                color: Theme.muted
                wrapMode: Text.Wrap
                elide: Text.ElideNone
                maximumLineCount: 14
                text: Life.errorDetails
            }
        }
    }

    Keys.onPressed: (event) => {
        if (Theme.isBack(event.key)) {
            event.accepted = true
            Life.dismiss()
        }
    }
}
