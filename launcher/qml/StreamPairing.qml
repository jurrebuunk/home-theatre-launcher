import QtQuick

// Pairing with a Moonlight host: the PIN to type on the host while
// moonlight-qt completes the handshake in the background.
FocusScope {
    id: root
    signal cancelled()
    signal retry()

    readonly property var p: Moonlight.pairing
    readonly property string stage: p.state || ""
    readonly property bool failed: stage === "failed"

    function focusDefault() {
        if (failed) retryButton.forceActiveFocus()
        else cancelButton.forceActiveFocus()
    }
    onFailedChanged: Qt.callLater(focusDefault)

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.px(30)
        width: Theme.px(642)
        spacing: 0

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(40)
            Icon { name: "tv"; size: Theme.px(104); tint: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
            Icon { name: "arrow-right"; size: Theme.px(40); tint: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
            Icon { name: "monitor"; size: Theme.px(104); tint: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
        }
        Item { width: 1; height: Theme.px(30) }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            role: "display"
            font.pixelSize: Theme.px(50)
            text: root.p.name || ""
        }
        Item { width: 1; height: Theme.px(40) }
        Rectangle {
            width: parent.width
            height: Theme.px(220)
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            Label {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: font.letterSpacing / 2
                role: "code"
                font.pixelSize: Theme.px(112)
                font.letterSpacing: Theme.px(40)
                text: root.p.pin || ""
                color: root.failed ? Theme.faint : Theme.text
            }
        }
        Item { width: 1; height: Theme.px(44) }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            role: "supporting"
            font.pixelSize: Theme.px(30)
            color: root.failed ? Theme.error : Theme.muted
            text: root.failed ? (root.p.error || "Pairing failed")
                : root.stage === "connecting" ? "Connecting to " + (root.p.name || "") + "…"
                : root.stage === "done" ? "Paired"
                : "Enter this PIN on the host"
        }
        Item { width: 1; height: Theme.px(40) }
        Item {
            width: parent.width
            height: Theme.px(4)
            IndeterminateBar {
                anchors.fill: parent
                visible: !root.failed && root.stage !== "done"
            }
        }
        Item { width: 1; height: Theme.px(34) }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(16)
            FlatButton {
                id: retryButton
                visible: root.failed
                text: "Try again"
                iconName: "refresh"
                onActivated: root.retry()
                KeyNavigation.right: cancelButton
            }
            FlatButton {
                id: cancelButton
                text: root.failed ? "Back" : "Cancel"
                iconName: root.failed ? "chevron-left" : "x"
                onActivated: root.cancelled()
                KeyNavigation.left: retryButton.visible ? retryButton : null
            }
        }
    }
}
