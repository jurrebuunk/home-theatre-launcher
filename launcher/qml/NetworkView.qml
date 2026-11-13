import QtQuick

// Network screen: interfaces on the left; for Wi-Fi the power switch, a
// timed scan and the visible networks, for Ethernet the link details.
FocusScope {
    id: root
    property string debugView
    signal closeRequested()

    readonly property var details: Nm.details
    readonly property bool wifi: details.kind === "wifi"
    // Text entry: { purpose: "password" | "hidden-ssid" | "hidden-password" | "ip", ... }
    property var entry: null
    property string hiddenSsid

    function focusDefault() {
        if (debugView === "network-wifi")
            focusRow(2) // matches the design mock-up
        else if (entry)
            entryView.focusDefault()
        else if (wifi && Nm.networks.count > 0 && Nm.wifiEnabled)
            focusRow(0)
        else if (!wifi && details.editable)
            ethernet.focusDefault()
        else
            focusInterface()
    }
    function focusInterface() {
        const i = Nm.interfaces.findIndex(x => x.name === Nm.selected)
        const item = ifaceRepeater.itemAt(Math.max(0, i))
        if (item)
            item.forceActiveFocus()
    }
    readonly property int rowCount: Nm.networks.count + (Nm.wifiEnabled && Nm.managed ? 1 : 0)
    function focusRow(i) {
        if (rowCount === 0)
            return
        i = Math.max(0, Math.min(rowCount - 1, i))
        const item = i < Nm.networks.count ? wifiRows.itemAt(i) : hiddenRow
        if (!item)
            return
        if (item === hiddenRow) hiddenRow.forceActiveFocus()
        else item.focusFirst()
        Qt.callLater(ensureVisible, item)
    }
    function ensureVisible(item) {
        if (!item || list.height <= 0)
            return
        const y = item.mapToItem(listColumn, 0, 0).y
        let target = list.contentY
        if (y < target)
            target = y
        else if (y + item.height > target + list.height)
            target = y + item.height - list.height
        list.contentY = Math.max(0, Math.min(target, list.contentHeight - list.height))
    }
    function enterContent() {
        if (wifi) focusRow(0)
        else if (details.editable) ethernet.focusDefault()
    }
    function openHidden() {
        openEntry({ purpose: "hidden-ssid", title: "Hidden network", icon: "wifi", secret: false, text: "", submit: "Next" })
    }
    function close() {
        Nm.close()
        closeRequested()
    }

    // Connect flow: a password only when the network needs one.
    function requestConnect(ssid) {
        if (Nm.needsPassword(ssid))
            openEntry({ purpose: "password", title: ssid, icon: "lock", secret: true, text: "", submit: "Connect", ssid: ssid })
        else
            Nm.connectNetwork(ssid, "", false)
    }
    function openEntry(e) {
        entry = e
        Qt.callLater(() => entryView.focusDefault())
    }
    function closeEntry() {
        entry = null
        Qt.callLater(focusDefault)
    }
    function submitEntry(text) {
        const e = entry
        if (e.purpose === "hidden-ssid") {
            if (text.trim() === "") { entryView.error = "Enter a network name"; return }
            hiddenSsid = text
            openEntry({ purpose: "hidden-password", title: text, icon: "lock", secret: true, text: "",
                        submit: "Connect", hint: "Leave empty for an open network" })
            return
        }
        if (e.purpose === "ip") {
            ethernet.setDraft(e.field, text.trim())
            entry = null
            Qt.callLater(() => ethernet.forceActiveFocus())
            return
        }
        const ssid = e.purpose === "hidden-password" ? hiddenSsid : e.ssid
        const err = Nm.connectNetwork(ssid, text, e.purpose === "hidden-password")
        if (err !== "") {
            entryView.error = err
            return
        }
        closeEntry()
    }

    Component.onCompleted: {
        Nm.open()
        if (debugView === "network-password")
            openEntry({ purpose: "password", title: "Neighbours", icon: "lock", secret: true, text: "hunter22", submit: "Connect", ssid: "Neighbours" })
    }
    Connections {
        target: Nm
        function onConnectingChanged() {
            if (Nm.connecting.stage === "error" && Nm.connecting.message === "Wrong password" && !root.entry)
                root.openEntry({ purpose: "password", title: Nm.connecting.ssid, icon: "lock", secret: true, text: "",
                                 submit: "Connect", ssid: Nm.connecting.ssid, error: "Wrong password" })
        }
    }

    Rectangle { anchors.fill: parent; color: Theme.background }
    MouseArea { anchors.fill: parent }

    Item {
        id: page
        anchors.fill: parent
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical)
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical)
        visible: !root.entry

        Row {
            id: header
            height: Theme.px(Theme.layout.statusRowHeight + 20)
            spacing: Theme.px(28)
            Icon {
                name: root.wifi ? "wifi" : "ethernet"
                size: Theme.px(48)
                tint: Theme.accent
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                role: "display"
                text: "Network"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Separator {
            id: headerLine
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
        }

        Column {
            id: leftColumn
            anchors.top: headerLine.bottom
            anchors.topMargin: Theme.px(50)
            anchors.left: parent.left
            spacing: Theme.px(22)

            Repeater {
                id: ifaceRepeater
                model: Nm.interfaces
                PanelItem {
                    id: ifacePanel
                    iconName: modelData.kind === "wifi" ? "wifi" : "ethernet"
                    selected: modelData.name === Nm.selected
                    onActiveFocusChanged: if (activeFocus) Nm.select(modelData.name)
                    KeyNavigation.up: index > 0 ? ifaceRepeater.itemAt(index - 1) : null
                    Keys.onDownPressed: {
                        if (index < ifaceRepeater.count - 1) ifaceRepeater.itemAt(index + 1).forceActiveFocus()
                        else if (root.wifi && Nm.managed) powerPanel.forceActiveFocus()
                    }
                    Keys.onRightPressed: root.enterContent()
                    onActivated: root.enterContent()
                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.px(58)
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: ifacePanel.fg
                    }
                    Label {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        role: "value"
                        text: modelData.name
                        color: ifacePanel.activeFocus ? Theme.background : Theme.muted
                    }
                }
            }

            Separator {
                visible: root.wifi && Nm.managed
                width: Theme.px(400)
            }

            PanelItem {
                id: powerPanel
                visible: root.wifi && Nm.managed
                iconName: "power"
                Keys.onUpPressed: ifaceRepeater.itemAt(ifaceRepeater.count - 1).forceActiveFocus()
                KeyNavigation.down: scanPanel
                Keys.onRightPressed: root.focusRow(0)
                onActivated: Nm.setWifiEnabled(!Nm.wifiEnabled)
                Toggle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Nm.wifiEnabled
                    knob: powerPanel.activeFocus ? Theme.background : Theme.accent
                }
            }
            PanelItem {
                id: scanPanel
                visible: root.wifi && Nm.managed
                iconName: "refresh"
                enabled: Nm.wifiEnabled
                opacity: enabled ? 1 : 0.5
                KeyNavigation.up: powerPanel
                Keys.onRightPressed: root.focusRow(0)
                onActivated: Nm.toggleScan()
                Label {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    role: "value"
                    font.pixelSize: Theme.font("body")
                    color: scanPanel.activeFocus ? Theme.background : Theme.muted
                    visible: Nm.scanning
                    text: Math.floor(Nm.scanRemaining / 60) + ":" + String(Nm.scanRemaining % 60).padStart(2, "0")
                }
                Rectangle {
                    visible: Nm.scanning
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.rightMargin: -Theme.px(24)
                    height: Math.max(2, Theme.px(4))
                    width: scanPanel.width * Math.max(0, Nm.scanRemaining) / Math.max(1, Nm.scanDuration)
                    color: Theme.accent
                }
            }
        }

        // Ethernet (or any interface without a network list)
        EthernetDetails {
            id: ethernet
            visible: !root.wifi && Nm.interfaces.length > 0
            anchors.top: headerLine.bottom
            anchors.topMargin: Theme.px(46)
            anchors.left: leftColumn.right
            anchors.leftMargin: Theme.px(80)
            anchors.right: parent.right
            details: root.details
            onLeftPressed: root.focusInterface()
            onEditRequested: (field, title, value) => root.openEntry({ purpose: "ip", field: field, title: title, icon: "ethernet",
                                                                      secret: false, text: value, submit: "Done", mode: "symbols" })
        }

        Label {
            visible: Nm.interfaces.length === 0
            anchors.centerIn: parent
            role: "supporting"
            color: Theme.muted
            text: "No network interfaces"
        }

        // Wi-Fi networks
        Flickable {
            id: list
            visible: root.wifi
            anchors.top: headerLine.bottom
            anchors.topMargin: Theme.px(50)
            anchors.left: leftColumn.right
            anchors.leftMargin: Theme.px(80)
            anchors.right: parent.right
            anchors.bottom: footerLine.top
            anchors.bottomMargin: Theme.px(16)
            clip: true
            contentHeight: listColumn.height
            boundsBehavior: Flickable.StopAtBounds
            Behavior on contentY { NumberAnimation { duration: Theme.transitionMs } }

            Column {
                id: listColumn
                width: list.width

                Separator { width: parent.width; visible: Nm.wifiEnabled && Nm.managed }
                Label {
                    visible: !Nm.managed || !Nm.wifiEnabled || Nm.networks.count === 0
                    role: "supporting"; color: Theme.faint
                    text: !Nm.managed ? "Wi-Fi is not managed by the launcher (enable behavior.network.networkmanager)"
                        : !Nm.wifiEnabled ? "Wi-Fi is off"
                        : Nm.scanning ? "Searching…" : "No networks found"
                    height: Theme.px(Theme.layout.rowHeight); verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.px(24)
                }
                Repeater {
                    id: wifiRows
                    model: Nm.networks
                    NetworkRow {
                        ssid: model.ssid; bars: model.bars; secured: model.secured
                        active: model.active; saved: model.saved
                        onUpPressed: index === 0 ? null : root.focusRow(index - 1)
                        onDownPressed: root.focusRow(index + 1)
                        onConnectRequested: (s) => root.requestConnect(s)
                        Keys.onLeftPressed: root.focusInterface()
                    }
                }
                Item {
                    id: hiddenRow
                    visible: Nm.wifiEnabled && Nm.managed
                    activeFocusOnTab: true
                    width: parent.width
                    height: Theme.px(Theme.layout.rowHeight + 8)
                    Rectangle { anchors.fill: parent; color: hiddenRow.activeFocus ? Theme.accent : "transparent" }
                    FocusFrame {}
                    Separator { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom }
                    Icon {
                        id: plus
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.px(24)
                        anchors.verticalCenter: parent.verticalCenter
                        name: "plus"; size: Theme.px(40)
                        tint: hiddenRow.activeFocus ? Theme.background : Theme.muted
                    }
                    Label {
                        anchors.left: plus.right
                        anchors.leftMargin: Theme.px(28)
                        anchors.verticalCenter: parent.verticalCenter
                        role: "supporting"
                        font.pixelSize: Theme.font("body")
                        color: hiddenRow.activeFocus ? Theme.background : Theme.muted
                        text: "Hidden network"
                    }
                    Keys.onUpPressed: root.focusRow(Nm.networks.count - 1)
                    Keys.onLeftPressed: root.focusInterface()
                    Keys.onPressed: (event) => {
                        if (Theme.isSelect(event.key)) {
                            event.accepted = true
                            root.openHidden()
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { hiddenRow.forceActiveFocus(); root.openHidden() }
                    }
                }
            }
        }

        Separator {
            id: footerLine
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: hints.top
            anchors.bottomMargin: Theme.px(28)
        }
        HintBar {
            id: hints
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            hints: [{ a: "select", t: "Select" }, { a: "back", t: "Back" }]
        }
    }

    TextEntry {
        id: entryView
        anchors.fill: parent
        visible: !!root.entry
        enabled: visible
        title: root.entry ? root.entry.title : ""
        iconName: root.entry ? root.entry.icon : "lock"
        secret: root.entry ? root.entry.secret : false
        submitText: root.entry ? root.entry.submit : "Connect"
        maxLength: root.entry && root.entry.purpose === "hidden-ssid" ? 32 : 63
        onSubmitted: (text) => root.submitEntry(text)
        onCancelled: root.closeEntry()
        Connections {
            target: root
            function onEntryChanged() {
                if (!root.entry)
                    return
                entryView.text = root.entry.text || ""
                entryView.error = root.entry.error || ""
                entryView.hint = root.entry.hint || ""
                entryView.mode = root.entry.mode || "lower"
                entryView.reveal = false
                entryView.onEye = false
                entryView.row = 2
                entryView.col = 7
            }
        }
    }

    Keys.onPressed: (event) => {
        if (!Theme.isBack(event.key))
            return
        event.accepted = true
        if (root.entry)
            root.closeEntry()
        else
            root.close()
    }
}
