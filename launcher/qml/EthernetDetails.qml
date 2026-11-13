import QtQuick

// Link speed, reachability, Automatic/Manual and the IPv4 settings of one
// interface. Manual values are edited with the on-screen keyboard.
FocusScope {
    id: root
    property var details: ({})
    signal leftPressed()
    signal editRequested(string field, string title, string value)

    property bool editing: false
    property string draftAddress
    property string draftGateway
    property string draftDns
    property string error

    readonly property bool editable: !!details.editable
    readonly property string onlineText: details.name === Net.interfaceName
        ? (Net.state === "online" ? "online" : Net.state === "checking" ? "checking"
           : Net.reason === "nolink" ? "no cable" : "offline")
        : (!details.carrier ? "no cable" : details.state === "connected" ? "connected" : "not connected")

    function focusDefault() {
        if (editable)
            (methods.itemAt(editing || details.method === "manual" ? 1 : 0) || methods.itemAt(0)).forceActiveFocus()
        else
            root.forceActiveFocus()
    }
    function setDraft(field, value) {
        if (field === "address") draftAddress = value
        else if (field === "gateway") draftGateway = value
        else draftDns = value
    }
    function choose(method) {
        error = ""
        if (method === "manual") {
            draftAddress = details.address || ""
            draftGateway = details.gateway || ""
            draftDns = details.dns || ""
            editing = true
            Qt.callLater(() => addressField.forceActiveFocus())
        } else {
            editing = false
            if (details.method !== "auto")
                error = Nm.setIpv4(details.name, "auto", "", "", "")
        }
    }
    function apply() {
        error = Nm.setIpv4(details.name, "manual", draftAddress, draftGateway, draftDns)
        if (error === "") {
            editing = false
            Qt.callLater(focusDefault)
        }
    }
    onDetailsChanged: if (!editing) error = ""

    Keys.onLeftPressed: root.leftPressed()

    component Field: Item {
        id: field
        property string label
        property string value
        property string key
        property bool editableField: false
        signal activated()
        activeFocusOnTab: editableField
        width: parent ? parent.width : 0
        height: Theme.px(92)
        Label {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            role: "supporting"
            color: Theme.muted
            text: field.label
        }
        Rectangle {
            visible: field.editableField
            x: Theme.px(190)
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(560)
            height: Theme.px(72)
            color: field.activeFocus ? Theme.accent : Theme.surface
            border.color: Theme.border
            border.width: field.activeFocus ? 0 : Theme.borderWidth
            FocusFrame { shown: field.activeFocus }
        }
        Label {
            x: Theme.px(field.editableField ? 214 : 210)
            anchors.verticalCenter: parent.verticalCenter
            role: "value"
            font.pixelSize: Theme.px(30)
            text: field.value || "—"
            color: field.activeFocus ? Theme.background : field.value ? Theme.text : Theme.faint
        }
        Keys.onPressed: (event) => {
            if (field.editableField && Theme.isSelect(event.key)) {
                event.accepted = true
                field.activated()
            }
        }
        MouseArea {
            anchors.fill: parent
            enabled: field.editableField
            onClicked: { field.forceActiveFocus(); field.activated() }
        }
    }

    Column {
        width: parent.width
        spacing: Theme.px(36)

        Row {
            spacing: Theme.px(28)
            height: Theme.px(56)
            Icon { visible: !!root.details.speed; name: "link"; size: Theme.px(32); tint: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
            Label { visible: !!root.details.speed; role: "value"; font.pixelSize: Theme.px(30); text: root.details.speed || ""; anchors.verticalCenter: parent.verticalCenter }
            Separator { visible: !!root.details.speed; vertical: true; height: Theme.px(40); anchors.verticalCenter: parent.verticalCenter }
            Icon {
                name: "globe"; size: Theme.px(32); anchors.verticalCenter: parent.verticalCenter
                tint: root.onlineText === "online" || root.onlineText === "connected" ? Theme.accent : Theme.warning
            }
            Label { role: "value"; font.pixelSize: Theme.px(30); text: root.onlineText; anchors.verticalCenter: parent.verticalCenter }
        }

        Segmented {
            id: methods
            visible: root.editable
            options: [{ value: "auto", text: "Automatic" }, { value: "manual", text: "Manual" }]
            current: root.editing ? "manual" : (root.details.method || "auto")
            onChosen: (value) => root.choose(value)
            Keys.onDownPressed: root.editing ? addressField.forceActiveFocus() : null
            Keys.onLeftPressed: (event) => {
                if (methods.itemAt(0).activeFocus) root.leftPressed()
                else event.accepted = false
            }
        }
        Label {
            visible: !root.editable
            role: "supporting"
            color: Theme.faint
            text: Nm.managed ? "This connection is managed outside the launcher"
                             : "Addresses are configured by the system"
        }

        Separator { width: Math.min(parent.width, Theme.px(1040)) }

        Column {
            width: Math.min(parent.width, Theme.px(1040))
            Field {
                id: addressField
                label: "Address"; key: "address"
                value: root.editing ? root.draftAddress : (root.details.address || "")
                editableField: root.editing
                onActivated: root.editRequested("address", "Address", root.draftAddress)
                KeyNavigation.up: methods.itemAt(1)
                KeyNavigation.down: gatewayField
            }
            Field {
                id: gatewayField
                label: "Gateway"; key: "gateway"
                value: root.editing ? root.draftGateway : (root.details.gateway || "")
                editableField: root.editing
                onActivated: root.editRequested("gateway", "Gateway", root.draftGateway)
                KeyNavigation.up: addressField
                KeyNavigation.down: dnsField
            }
            Field {
                id: dnsField
                label: "DNS"; key: "dns"
                value: root.editing ? root.draftDns : (root.details.dns || "")
                editableField: root.editing
                onActivated: root.editRequested("dns", "DNS", root.draftDns)
                KeyNavigation.up: gatewayField
                KeyNavigation.down: applyButton
            }
        }

        Row {
            visible: root.editing
            spacing: Theme.px(16)
            FlatButton {
                id: applyButton
                text: "Apply"; iconName: "check"
                onActivated: root.apply()
                KeyNavigation.up: dnsField
                KeyNavigation.right: cancelButton
            }
            FlatButton {
                id: cancelButton
                text: "Cancel"; iconName: "x"
                onActivated: { root.editing = false; root.error = ""; Qt.callLater(root.focusDefault) }
                KeyNavigation.up: dnsField
                KeyNavigation.left: applyButton
            }
        }

        Label {
            visible: root.error !== "" || Nm.lastError !== ""
            role: "supporting"
            color: Theme.error
            text: root.error || Nm.lastError
        }
    }
}
