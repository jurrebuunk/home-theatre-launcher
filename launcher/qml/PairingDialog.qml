import QtQuick

// Pairing / agent prompt: passkey confirmation, code display, PIN entry,
// authorisation. Trust is only granted through the explicit checkbox.
FocusScope {
    id: root
    readonly property var req: Bt.request
    readonly property var pair: Bt.pairing
    readonly property string type: req.type || ""
    readonly property string stage: pair.stage || ""
    readonly property string deviceName: req.name || pair.name || ""
    readonly property string kind: req.kind || pair.kind || "device"

    readonly property bool showsCode: type === "confirm" || type === "displayPasskey" || type === "displayPin"
    readonly property bool asksCode: type === "requestPin" || type === "requestPasskey"
    readonly property bool asksAuth: type === "authorize" || type === "authorizeService"
    readonly property bool isError: stage === "error" && type === ""
    readonly property bool working: type === "" && stage === "working"
    property int remaining: Bt.pairingTimeout

    function focusDefault() {
        if (isError || working || type === "displayPasskey" || type === "displayPin")
            cancelButton.forceActiveFocus()
        else if (asksCode)
            digits.forceActiveFocus()
        else
            okButton.forceActiveFocus()
    }
    function formatted(code) {
        return code.length === 6 ? code.substring(0, 3) + " " + code.substring(3) : code
    }

    onTypeChanged: {
        remaining = Bt.pairingTimeout
        if (visible)
            Qt.callLater(focusDefault)
    }
    onStageChanged: if (visible) Qt.callLater(focusDefault)

    Timer {
        interval: 1000
        repeat: true
        running: root.visible && root.type !== ""
        onTriggered: {
            root.remaining -= 1
            if (root.remaining <= 0)
                Bt.rejectRequest()
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.background }

    Column {
        anchors.centerIn: parent
        spacing: Theme.px(44)

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(56)
            Icon { name: Theme.deviceIcon(root.kind); size: Theme.px(100); tint: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
            Icon { name: "bluetooth"; size: Theme.px(60); tint: Theme.faint; anchors.verticalCenter: parent.verticalCenter }
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            role: "display"
            text: root.deviceName
        }

        // Code shown for confirmation / typing on the device
        Rectangle {
            visible: root.showsCode
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(640)
            height: Theme.px(184)
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            Label {
                anchors.centerIn: parent
                role: "code"
                text: root.formatted(root.req.code || "")
            }
        }
        Label {
            visible: root.type === "displayPasskey" || root.type === "displayPin"
            anchors.horizontalCenter: parent.horizontalCenter
            role: "supporting"; color: Theme.muted
            text: "Type this code on the device"
        }

        // PIN / passkey entry
        DigitEntry {
            id: digits
            visible: root.asksCode
            anchors.horizontalCenter: parent.horizontalCenter
            length: root.type === "requestPasskey" ? 6 : 4
            KeyNavigation.down: okButton
        }

        Label {
            visible: root.asksAuth
            anchors.horizontalCenter: parent.horizontalCenter
            role: "supporting"; color: Theme.muted
            text: "Allow " + root.deviceName + " to connect?"
        }
        Column {
            visible: root.working
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(24)
            Label { anchors.horizontalCenter: parent.horizontalCenter; role: "supporting"; color: Theme.muted; text: "Pairing…" }
            IndeterminateBar { width: Theme.px(500); running: parent.visible }
        }
        Label {
            visible: root.isError
            anchors.horizontalCenter: parent.horizontalCenter
            role: "supporting"; color: Theme.error
            text: root.pair.message || ""
        }

        CheckBox {
            id: trustBox
            visible: !root.asksAuth && !root.isError && root.stage !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Trust"
            iconName: "shield-check"
            checked: false
            onToggled: Bt.setTrustOnPair(checked)
            KeyNavigation.down: okButton.visible ? okButton : cancelButton
            Connections {
                target: Bt
                function onPairingChanged() { if (Object.keys(Bt.pairing).length === 0) trustBox.checked = false }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(16)
            FlatButton {
                id: okButton
                visible: root.type === "confirm" || root.asksCode || root.asksAuth
                text: root.asksAuth ? "Allow" : "Pair"
                iconName: "check"
                KeyNavigation.right: cancelButton
                KeyNavigation.up: root.asksCode ? digits : (trustBox.visible ? trustBox : null)
                onActivated: Bt.acceptRequest(root.asksCode ? digits.value : "")
            }
            FlatButton {
                id: cancelButton
                text: root.isError ? "Close" : root.asksAuth ? "Deny" : "Cancel"
                iconName: "x"
                KeyNavigation.left: okButton.visible ? okButton : null
                KeyNavigation.up: trustBox.visible ? trustBox : null
                onActivated: {
                    if (root.asksAuth && root.stage === "")
                        Bt.rejectRequest()
                    else
                        Bt.cancelPairing()
                }
            }
        }
        Label {
            visible: root.type !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            role: "value"
            color: Theme.faint
            text: Math.floor(root.remaining / 60) + ":" + String(root.remaining % 60).padStart(2, "0")
        }
    }
}
