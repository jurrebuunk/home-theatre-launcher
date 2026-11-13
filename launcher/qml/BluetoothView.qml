import QtQuick

// Full Bluetooth menu: adapter, power, scan, paired + nearby devices,
// pairing dialog. Back closes (and stops scanning).
FocusScope {
    id: root
    property bool forceNoAdapter: false
    // Side panel of the in-app menu: no left column, scan / power in the header.
    property bool compact: false
    signal closeRequested()

    readonly property bool noAdapter: forceNoAdapter || !Bt.serviceAvailable || !Bt.hasAdapter
    readonly property bool pairingOpen: !noAdapter && (Object.keys(Bt.pairing).length > 0 || Object.keys(Bt.request).length > 0)
    property string forgetPath
    property string forgetName
    readonly property int rowCount: Bt.paired.count + Bt.nearby.count

    function focusDefault() {
        if (noAdapter)
            noAdapterView.focusDefault()
        else if (rowCount > 0)
            focusRow(0)
        else
            focusHeader()
    }
    function focusHeader() {
        if (compact)
            scanTool.forceActiveFocus()
        else
            scanPanel.forceActiveFocus()
    }
    function rowItem(i) {
        return i < Bt.paired.count ? pairedRows.itemAt(i) : nearbyRows.itemAt(i - Bt.paired.count)
    }
    function focusRow(i) {
        if (rowCount === 0)
            return
        i = Math.max(0, Math.min(rowCount - 1, i))
        const item = rowItem(i)
        if (!item)
            return
        item.focusFirst()
        Qt.callLater(ensureVisible, item)
    }
    function ensureVisible(item) {
        if (!item || list.height <= 0)
            return
        // the row's own height includes its expanded action buttons
        const y = item.mapToItem(listColumn, 0, 0).y
        let target = list.contentY
        if (y - Theme.px(56) < target)
            target = y - Theme.px(56) // keep the section label in view
        else if (y + item.height > target + list.height)
            target = y + item.height - list.height
        list.contentY = Math.max(0, Math.min(target, list.contentHeight - list.height))
    }
    function close() {
        Bt.close()
        closeRequested()
    }

    onNoAdapterChanged: if (activeFocus || visible) Qt.callLater(focusDefault)
    onPairingOpenChanged: if (pairingOpen) pairing.focusDefault(); else Qt.callLater(focusDefault)

    Rectangle { anchors.fill: parent; color: root.compact ? "transparent" : Theme.background }
    MouseArea { anchors.fill: parent }

    component ToolButton: Item {
        id: tool
        property string iconName
        default property alias content: slot.data
        signal activated()
        activeFocusOnTab: true
        width: Theme.px(72)
        height: Theme.px(64)
        readonly property color fg: activeFocus ? Theme.background : Theme.text
        Rectangle {
            anchors.fill: parent
            color: tool.activeFocus ? Theme.accent : "transparent"
            border.color: Theme.border
            border.width: tool.activeFocus ? 0 : Theme.borderWidth
        }
        FocusFrame {}
        Icon { visible: tool.iconName !== ""; anchors.centerIn: parent; name: tool.iconName; size: Theme.px(30); tint: tool.fg }
        Item { id: slot; anchors.fill: parent }
        Keys.onPressed: (event) => {
            if (Theme.isSelect(event.key)) { event.accepted = true; tool.activated() }
        }
        MouseArea { anchors.fill: parent; onClicked: { tool.forceActiveFocus(); tool.activated() } }
    }

    Item {
        id: page
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMargin)
        anchors.topMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMarginVertical)
        anchors.bottomMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMarginVertical)
        visible: !root.noAdapter && !root.pairingOpen && root.forgetPath === ""

        Row {
            id: header
            height: Theme.px(Theme.layout.statusRowHeight + 20)
            spacing: Theme.px(28)
            Icon {
                visible: root.compact
                name: "chevron-left"
                size: Theme.px(36)
                tint: Theme.muted
                anchors.verticalCenter: parent.verticalCenter
                MouseArea { anchors.fill: parent; onClicked: root.close() }
            }
            Icon {
                name: "bluetooth"
                size: Theme.px(48)
                tint: Theme.accent
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                role: "display"
                text: "Bluetooth"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Row {
            id: tools
            visible: root.compact
            anchors.right: parent.right
            anchors.verticalCenter: header.verticalCenter
            spacing: Theme.px(22)
            ToolButton {
                id: scanTool
                iconName: "search"
                enabled: Bt.powered
                opacity: enabled ? 1 : 0.5
                KeyNavigation.right: powerTool
                Keys.onDownPressed: root.focusRow(0)
                onActivated: Bt.toggleScan()
            }
            ToolButton {
                id: powerTool
                width: Theme.px(100)
                KeyNavigation.left: scanTool
                Keys.onDownPressed: root.focusRow(0)
                onActivated: Bt.setPowered(!Bt.powered)
                Toggle {
                    anchors.centerIn: parent
                    checked: Bt.powered
                    knob: powerTool.activeFocus ? Theme.background : Theme.accent
                }
            }
        }
        Separator {
            id: headerLine
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
        }
        Rectangle {
            // remaining scan time under the header tools
            visible: root.compact && (Bt.discovering || Bt.scanRemaining > 0)
            anchors.top: headerLine.top
            anchors.right: parent.right
            height: Math.max(2, Theme.px(4))
            width: tools.width * Math.max(0, Bt.scanRemaining) / Math.max(1, Bt.scanDuration)
            color: Theme.accent
        }

        Column {
            id: leftColumn
            visible: !root.compact
            width: root.compact ? 0 : implicitWidth
            anchors.top: headerLine.bottom
            anchors.topMargin: Theme.px(50)
            anchors.left: parent.left
            spacing: Theme.px(22)

            PanelItem {
                id: adapterPanel
                KeyNavigation.down: powerPanel
                Keys.onLeftPressed: Bt.selectAdapter(-1)
                Keys.onRightPressed: (event) => {
                    if (Bt.adapterNames.length > 1) Bt.selectAdapter(1)
                    else root.focusRow(0)
                }
                Icon {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron-left"
                    size: Theme.px(28)
                    tint: adapterPanel.activeFocus ? Theme.background
                        : Bt.adapterNames.length > 1 ? Theme.muted : Theme.faint
                }
                Label {
                    anchors.centerIn: parent
                    role: "value"
                    font.pixelSize: Theme.font("body")
                    text: Bt.adapterName
                    color: adapterPanel.fg
                }
                Icon {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron-right"
                    size: Theme.px(28)
                    tint: adapterPanel.activeFocus ? Theme.background
                        : Bt.adapterNames.length > 1 ? Theme.muted : Theme.faint
                }
            }
            PanelItem {
                id: powerPanel
                iconName: "power"
                KeyNavigation.up: adapterPanel
                KeyNavigation.down: scanPanel
                Keys.onRightPressed: root.focusRow(0)
                onActivated: Bt.setPowered(!Bt.powered)
                Toggle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Bt.powered
                    knob: powerPanel.activeFocus ? Theme.background : Theme.accent
                }
            }
            PanelItem {
                id: scanPanel
                iconName: "search"
                enabled: Bt.powered
                opacity: enabled ? 1 : 0.5
                KeyNavigation.up: powerPanel
                Keys.onRightPressed: root.focusRow(0)
                onActivated: Bt.toggleScan()
                Label {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    role: "value"
                    font.pixelSize: Theme.font("body")
                    color: scanPanel.activeFocus ? Theme.background : Theme.muted
                    visible: Bt.discovering || Bt.scanRemaining > 0
                    text: Math.floor(Bt.scanRemaining / 60) + ":" + String(Bt.scanRemaining % 60).padStart(2, "0")
                }
                Rectangle {
                    // remaining scan time, shrinking from the right
                    visible: Bt.discovering || Bt.scanRemaining > 0
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.rightMargin: -Theme.px(24)
                    height: Math.max(2, Theme.px(4))
                    width: (scanPanel.width) * Math.max(0, Bt.scanRemaining) / Math.max(1, Bt.scanDuration)
                    color: Theme.accent
                }
            }
        }

        Flickable {
            id: list
            anchors.top: headerLine.bottom
            anchors.topMargin: Theme.px(30)
            anchors.left: root.compact ? parent.left : leftColumn.right
            anchors.leftMargin: root.compact ? 0 : Theme.px(80)
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

                Label { role: "supporting"; color: Theme.muted; text: "Paired"; height: Theme.px(56); verticalAlignment: Text.AlignVCenter }
                Separator { width: parent.width }
                Label {
                    visible: Bt.paired.count === 0
                    role: "supporting"; color: Theme.faint
                    text: "No paired devices"
                    height: Theme.px(Theme.layout.rowHeight); verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.px(24)
                }
                Repeater {
                    id: pairedRows
                    model: Bt.paired
                    DeviceRow {
                        path: model.path; name: model.name; kind: model.kind
                        connected: model.connected; trusted: model.trusted; paired: true; busy: model.busy
                        battery: model.battery
                        onUpPressed: index === 0 ? (root.compact ? root.focusHeader() : null) : root.focusRow(index - 1)
                        onDownPressed: root.focusRow(index + 1)
                        onForgetRequested: (p, n) => { root.forgetPath = p; root.forgetName = n; forgetCancel.forceActiveFocus() }
                        Keys.onLeftPressed: root.compact ? null : powerPanel.forceActiveFocus()
                    }
                }
                Item { width: 1; height: Theme.px(40) }
                Label { role: "supporting"; color: Theme.muted; text: "Nearby"; height: Theme.px(56); verticalAlignment: Text.AlignVCenter }
                Separator { width: parent.width }
                Label {
                    visible: Bt.nearby.count === 0
                    role: "supporting"; color: Theme.faint
                    text: Bt.discovering ? "Searching…" : "Search to find devices"
                    height: Theme.px(Theme.layout.rowHeight); verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.px(24)
                }
                Repeater {
                    id: nearbyRows
                    model: Bt.nearby
                    DeviceRow {
                        path: model.path; name: model.name; kind: model.kind
                        connected: false; trusted: false; paired: false; busy: model.busy
                        battery: model.battery
                        onUpPressed: Bt.paired.count + index === 0 && root.compact ? root.focusHeader()
                                   : root.focusRow(Bt.paired.count + index - 1)
                        onDownPressed: root.focusRow(Bt.paired.count + index + 1)
                        Keys.onLeftPressed: root.compact ? null : scanPanel.forceActiveFocus()
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
            hints: root.compact ? [{ a: "select", t: "Select" }, { a: "back", t: "Back" }]
                                : [{ a: "select", t: "Select" }, { a: "back", t: "Back" }, { a: "home", t: "Home" }]
        }
    }

    // Forget confirmation
    FocusScope {
        anchors.fill: parent
        visible: root.forgetPath !== ""
        Column {
            anchors.centerIn: parent
            spacing: Theme.px(40)
            Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "trash"; size: Theme.px(120); tint: Theme.warning }
            Label { anchors.horizontalCenter: parent.horizontalCenter; role: "display"; text: "Forget " + root.forgetName + "?" }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                role: "supporting"; color: Theme.muted
                text: "It will need to be paired again."
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.px(16)
                FlatButton {
                    id: forgetConfirm
                    text: "Forget"; iconName: "trash"
                    KeyNavigation.right: forgetCancel
                    onActivated: { Bt.forget(root.forgetPath); root.forgetPath = ""; Qt.callLater(root.focusDefault) }
                }
                FlatButton {
                    id: forgetCancel
                    text: "Cancel"; iconName: "x"
                    KeyNavigation.left: forgetConfirm
                    onActivated: { root.forgetPath = ""; Qt.callLater(root.focusDefault) }
                }
            }
        }
    }

    PairingDialog {
        id: pairing
        anchors.fill: parent
        visible: root.pairingOpen
    }

    NoAdapterView {
        id: noAdapterView
        anchors.fill: parent
        visible: root.noAdapter
        unavailable: !Bt.serviceAvailable
        onCloseRequested: root.close()
    }

    Keys.onPressed: (event) => {
        if (!Theme.isBack(event.key))
            return
        event.accepted = true
        if (root.pairingOpen)
            Bt.cancelPairing()
        else if (root.forgetPath !== "") {
            root.forgetPath = ""
            Qt.callLater(root.focusDefault)
        } else
            root.close()
    }
}
