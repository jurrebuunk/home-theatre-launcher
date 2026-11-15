import QtQuick

// Moonlight library: the apps of the selected host as box art (A plays),
// the Hosts page (Y) to wake, choose, remove and pair hosts, and pairing.
FocusScope {
    id: root
    property string debugView
    signal closeRequested()

    property string page: "library" // library | hosts | pair | address | quit
    property int pendingApp: -1
    property string pendingName
    property string hostsFocus: ""  // "add": land on Add by address

    readonly property var cur: Moonlight.current
    readonly property bool hasHost: !!cur.uuid
    readonly property var runningApp: Moonlight.apps.find(a => a.running) || null
    readonly property bool showGrid: hasHost && cur.state === "online" && Moonlight.apps.length > 0

    function focusDefault() {
        if (page === "library") {
            if (showGrid) grid.forceActiveFocus()
            else if (statusButton.visible) statusButton.forceActiveFocus()
            else hostButton.forceActiveFocus()
        } else if (page === "hosts") {
            const rows = hostRows()
            const want = hostsFocus === "add" ? addRow
                       : debugView === "moonlight-hosts" ? rows[1] // as in the design
                       : rows.find(r => r.host && r.host.uuid === cur.uuid)
            hostsFocus = ""
            ;(want || rows[0]).focusFirst()
        } else if (page === "pair") {
            pairing.focusDefault()
        } else if (page === "address") {
            entry.focusDefault()
        } else if (page === "quit") {
            quitStart.forceActiveFocus()
        }
    }
    function go(p) {
        page = p
        Qt.callLater(focusDefault)
    }
    function close() {
        Moonlight.close()
        closeRequested()
    }
    function back() {
        if (page === "library") close()
        else if (page === "pair") { Moonlight.cancelPair(); go("hosts") }
        else if (page === "quit") go("library")
        else if (page === "address") go("hosts")
        else go("library")
    }
    function play(app) {
        const r = Moonlight.play(app.id)
        if (r === "quit") {
            pendingApp = app.id
            pendingName = app.name
            go("quit")
        }
    }
    function startPair(address) {
        const err = Moonlight.pair(address)
        if (err !== "") {
            entry.error = err
            return false
        }
        go("pair")
        return true
    }
    function hostRows() {
        const out = []
        for (let i = 0; i < pairedRepeater.count; ++i) out.push(pairedRepeater.itemAt(i))
        for (let i = 0; i < foundRepeater.count; ++i) out.push(foundRepeater.itemAt(i))
        out.push(addRow)
        return out.filter(r => !!r)
    }
    function moveRow(item, d) {
        const rows = hostRows()
        const i = rows.indexOf(item) + d
        if (i >= 0 && i < rows.length) {
            rows[i].focusFirst()
            Qt.callLater(ensureVisible, rows[i])
        }
    }
    function ensureVisible(item) {
        if (!item || hostList.height <= 0)
            return
        const y = item.mapToItem(hostColumn, 0, 0).y
        let target = hostList.contentY
        if (y < target) target = y
        else if (y + item.height > target + hostList.height) target = y + item.height - hostList.height
        hostList.contentY = Math.max(0, Math.min(target, hostList.contentHeight - hostList.height))
    }

    // Pause polling while a stream runs; resume when back.
    readonly property bool live: Life.state === "launcher"
    onLiveChanged: live ? Moonlight.open() : Moonlight.close()
    Component.onCompleted: {
        Moonlight.open()
        syncApps()
        if (debugView === "moonlight-hosts") page = "hosts"
        else if (debugView === "moonlight-pair") page = "pair"
    }
    Connections {
        target: Moonlight
        function onPairingChanged() {
            if (Moonlight.pairing.state === "done" && root.page === "pair")
                pairDone.start()
        }
        function onAppsChanged() { root.syncApps(); Qt.callLater(root.refocusLibrary) }
        function onHostsChanged() { Qt.callLater(root.refocusLibrary) }
    }
    // Box art and "running" arrive while browsing: update tiles in place so
    // the highlighted tile stays where it is.
    ListModel { id: appModel }
    function syncApps() {
        const apps = Moonlight.apps
        let same = apps.length === appModel.count
        for (let i = 0; same && i < apps.length; ++i)
            same = appModel.get(i).appId === apps[i].id
        if (same) {
            for (let i = 0; i < apps.length; ++i)
                appModel.set(i, { appId: apps[i].id, name: apps[i].name, art: apps[i].art, running: apps[i].running })
            return
        }
        const keep = grid.currentIndex >= 0 && grid.currentIndex < appModel.count ? appModel.get(grid.currentIndex).appId : -1
        appModel.clear()
        for (const a of apps)
            appModel.append({ appId: a.id, name: a.name, art: a.art, running: a.running })
        let i = apps.findIndex(a => a.id === keep)
        if (i < 0) i = root.debugView === "moonlight" ? 3 : Math.max(0, apps.findIndex(a => a.running))
        grid.currentIndex = Math.min(i, apps.length - 1)
    }
    function appAt(i) {
        if (i < 0 || i >= appModel.count)
            return null
        const id = appModel.get(i).appId
        return Moonlight.apps.find(a => a.id === id) || null
    }
    // Focus left on something that just disappeared (e.g. Wake once waking).
    function refocusLibrary() {
        if (page !== "library" || !root.activeFocus)
            return
        const onHeader = backButton.activeFocus || hostButton.activeFocus || addHost.activeFocus
        if ((grid.activeFocus && !showGrid) || (statusButton.activeFocus && (!statusButton.visible || showGrid))
                || (!onHeader && !grid.activeFocus && !statusButton.activeFocus))
            focusDefault()
    }

    Timer {
        id: pairDone
        interval: 900
        onTriggered: { Moonlight.cancelPair(); root.go("library") }
    }

    // The focused app's box art behind the library (optional), over the
    // menu background. Follows focus after a short pause, so scrolling
    // through the row does not decode every picture on the way.
    readonly property var artBg: Theme.layout.moonlight.boxArtBackground
    property string backdropArt: ""
    readonly property string focusedArt: {
        const a = page === "library" && showGrid ? appAt(grid.currentIndex) : null
        return a && a.art ? a.art : ""
    }
    onFocusedArtChanged: artDelay.restart()
    Timer {
        id: artDelay
        interval: root.backdropArt === "" ? 0 : 250
        onTriggered: root.backdropArt = root.focusedArt
    }
    BackgroundView {
        anchors.fill: parent
        visible: root.artBg.enable && opacity > 0
        opacity: root.artBg.enable && root.backdropArt !== "" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: root.artBg.fadeMs } }
        bg: ({ type: "image", fill: "cover", fadeMs: root.artBg.fadeMs, color: "transparent" })
        images: root.backdropArt !== "" ? [root.backdropArt] : []
        dim: root.artBg.dim
        blur: root.artBg.blur
    }
    MouseArea { anchors.fill: parent }

    component HeaderButton: Item {
        id: hb
        property string iconName
        property string text
        property string trailingIcon
        property int iconSize: Theme.px(38)
        signal activated()
        activeFocusOnTab: true
        width: hbRow.width + Theme.px(32)
        height: Theme.px(64)
        Rectangle { anchors.fill: parent; color: hb.activeFocus ? Theme.accent : "transparent" }
        FocusFrame {}
        Row {
            id: hbRow
            anchors.centerIn: parent
            spacing: Theme.px(18)
            Icon {
                visible: hb.iconName !== ""
                name: hb.iconName
                size: hb.iconSize
                tint: hb.activeFocus ? Theme.background : Theme.accent
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                visible: hb.text !== ""
                text: hb.text
                font.pixelSize: Theme.px(36)
                color: hb.activeFocus ? Theme.background : Theme.text
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: hb.trailingIcon !== ""
                name: hb.trailingIcon
                size: Theme.px(30)
                tint: hb.activeFocus ? Theme.background : Theme.muted
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Keys.onPressed: (event) => {
            if (Theme.isSelect(event.key)) {
                event.accepted = true
                hb.activated()
            }
        }
        MouseArea { anchors.fill: parent; onClicked: { hb.forceActiveFocus(); hb.activated() } }
    }

    Item {
        id: frame
        anchors.fill: parent
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical)
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical)

        // ------------------------------------------------------------ library
        FocusScope {
            id: library
            anchors.fill: parent
            visible: root.page === "library"
            focus: visible

            Item {
                id: header
                width: parent.width
                height: Theme.px(Theme.layout.statusRowHeight + 20)
                HeaderButton {
                    id: backButton
                    anchors.verticalCenter: parent.verticalCenter
                    x: -Theme.px(16)
                    iconName: "chevron-left"
                    iconSize: Theme.px(36)
                    onActivated: root.close()
                    KeyNavigation.right: hostButton
                    Keys.onDownPressed: root.focusDefault()
                }
                HeaderButton {
                    id: hostButton
                    anchors.left: backButton.right
                    anchors.leftMargin: Theme.px(24)
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "monitor"
                    text: root.hasHost ? root.cur.name : "Moonlight"
                    trailingIcon: Moonlight.hosts.length > 1 ? "chevron-down" : ""
                    onActivated: Moonlight.hosts.length > 1 ? Moonlight.cycleHost() : root.go("hosts")
                    KeyNavigation.left: backButton
                    KeyNavigation.right: addHost
                    Keys.onDownPressed: if (root.showGrid) grid.forceActiveFocus(); else if (statusButton.visible) statusButton.forceActiveFocus()
                }
                HeaderButton {
                    id: addHost
                    anchors.right: parent.right
                    anchors.rightMargin: -Theme.px(16)
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "plus"
                    iconSize: Theme.px(32)
                    text: "Add host"
                    onActivated: { root.hostsFocus = "add"; root.go("hosts") }
                    KeyNavigation.left: hostButton
                    Keys.onDownPressed: if (root.showGrid) grid.forceActiveFocus(); else if (statusButton.visible) statusButton.forceActiveFocus()
                }
            }
            Separator { id: headerLine; anchors.top: header.bottom; width: parent.width }

            // One row of box art, like the home screen.
            ListView {
                id: grid
                readonly property real cardHeight: Theme.px(Theme.layout.moonlight.cardHeight)
                readonly property real cardWidth: Math.round(cardHeight * 204 / 320)
                readonly property real gap: Theme.px(40)
                readonly property real pad: Theme.px(28) // room for the focused (scaled) card
                y: headerLine.y + headerLine.height + (footerLine.y - headerLine.y - headerLine.height - height) / 2
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: -pad
                anchors.rightMargin: -pad
                height: Math.min(cardHeight * 1.08 + 2 * Theme.px(12), footerLine.y - headerLine.y - headerLine.height)
                visible: root.showGrid
                clip: true
                orientation: ListView.Horizontal
                spacing: gap
                // Centred while it fits (or left-aligned, as the home screen).
                readonly property real spare: width - 2 * pad - (count * cardWidth + Math.max(0, count - 1) * gap)
                leftMargin: pad + (Theme.layout.alignment === "left" ? 0 : Math.max(0, spare / 2))
                rightMargin: pad
                model: appModel
                keyNavigationEnabled: true
                highlightFollowsCurrentItem: false
                boundsBehavior: Flickable.StopAtBounds
                delegate: Item {
                    id: cell
                    required property int index
                    required property string name
                    required property string art
                    required property bool running
                    width: grid.cardWidth
                    height: grid.height
                    BoxArtTile {
                        anchors.verticalCenter: parent.verticalCenter
                        cardHeight: grid.cardHeight
                        name: cell.name
                        art: cell.art
                        running: cell.running
                        focused: grid.activeFocus && cell.ListView.isCurrentItem
                        onActivated: {
                            grid.currentIndex = cell.index
                            grid.forceActiveFocus()
                            const app = root.appAt(cell.index)
                            if (app) root.play(app)
                        }
                    }
                    z: ListView.isCurrentItem ? 2 : 0
                }
                Keys.onUpPressed: hostButton.forceActiveFocus()
                Keys.onPressed: (event) => {
                    const app = root.appAt(currentIndex)
                    if (Theme.isSelect(event.key) && app) {
                        event.accepted = true
                        root.play(app)
                    }
                }
            }

            // No grid: what is going on with the host instead.
            Column {
                id: status
                visible: !root.showGrid
                anchors.centerIn: parent
                width: Theme.px(900)
                spacing: Theme.px(18)
                readonly property string st: !root.hasHost ? "none"
                                             : root.cur.state === "online" ? (Moonlight.loadingApps ? "loading" : "empty")
                                             : root.cur.state
                Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: status.st !== "loading" && status.st !== "checking"
                    name: status.st === "offline" || status.st === "waking" ? "moon" : "monitor"
                    size: Theme.px(96)
                    tint: status.st === "waking" ? Theme.accent : Theme.muted
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    role: "title"
                    text: {
                        const n = root.cur.name
                        switch (status.st) {
                        case "none": return "No host paired yet"
                        case "checking": return "Connecting to " + n + "…"
                        case "loading": return "Loading apps…"
                        case "empty": return "No apps on " + n
                        case "waking": return "Waking " + n + "…"
                        default: return root.cur.canWake ? n + " is asleep" : n + " is offline"
                        }
                    }
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    role: "supporting"
                    color: Theme.muted
                    visible: text !== ""
                    text: status.st === "none" ? "Pair a PC running Sunshine or GeForce Experience to stream its games."
                        : status.st === "offline" && !root.cur.canWake ? "Turn it on, then it shows up here."
                        : status.st === "waking" ? "This can take a minute."
                        : ""
                }
                Item { width: 1; height: Theme.px(16) }
                IndeterminateBar {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: status.st === "checking" || status.st === "loading" || status.st === "waking"
                }
                FlatButton {
                    id: statusButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: status.st === "none" || (status.st === "offline" && !!root.cur.canWake)
                    text: status.st === "none" ? "Add host" : "Wake"
                    iconName: status.st === "none" ? "plus" : "power"
                    onActivated: status.st === "none" ? (root.hostsFocus = "add", root.go("hosts"))
                                                      : Moonlight.wake(root.cur.uuid)
                    Keys.onUpPressed: hostButton.forceActiveFocus()
                }
            }
            Separator { id: footerLine; anchors.bottom: footer.top; anchors.bottomMargin: Theme.px(28); width: parent.width }
            Item {
                id: footer
                anchors.bottom: parent.bottom
                width: parent.width
                height: Theme.px(40)
                HintBar {
                    anchors.verticalCenter: parent.verticalCenter
                    hints: [{ a: "select", t: root.showGrid ? "Play" : "Select" }, { a: "alt", t: "Hosts" }, { a: "back", t: "Back" }]
                }
                Label {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    role: "supporting"
                    color: Theme.error
                    text: Moonlight.message
                }
            }
        }

        // ------------------------------------------------------------ hosts
        FocusScope {
            id: hostsPage
            anchors.fill: parent
            visible: root.page === "hosts"

            Row {
                id: hostsHeader
                height: Theme.px(Theme.layout.statusRowHeight + 20)
                spacing: Theme.px(28)
                Icon { name: "monitor"; size: Theme.px(48); tint: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                Label { role: "display"; text: "Hosts"; anchors.verticalCenter: parent.verticalCenter }
            }
            Separator { id: hostsLine; anchors.top: hostsHeader.bottom; width: parent.width }
            IndeterminateBar {
                // Looking for hosts on the network
                anchors.verticalCenter: hostsLine.verticalCenter
                width: parent.width
                color: "transparent"
                visible: hostsPage.visible && Cfg.moonlight.discovery
            }

            Flickable {
                id: hostList
                anchors.top: hostsLine.bottom
                anchors.topMargin: Theme.px(20)
                anchors.bottom: hostsFooterLine.top
                anchors.bottomMargin: Theme.px(20)
                anchors.left: parent.left
                width: Math.min(parent.width, Theme.px(1240))
                contentHeight: hostColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: hostColumn
                    width: parent.width
                    topPadding: Theme.px(10)
                    Label {
                        role: "supporting"
                        color: Theme.muted
                        text: "Paired"
                        bottomPadding: Theme.px(14)
                    }
                    Separator { width: parent.width }
                    Repeater {
                        id: pairedRepeater
                        model: Moonlight.hosts
                        HostRow {
                            id: pairedRow
                            required property var modelData
                            kind: "paired"
                            host: modelData
                            onUpPressed: root.moveRow(pairedRow, -1)
                            onDownPressed: root.moveRow(pairedRow, 1)
                            onOpenRequested: (uuid) => { Moonlight.selectHost(uuid); root.go("library") }
                        }
                    }
                    Label {
                        visible: Moonlight.hosts.length === 0
                        role: "supporting"
                        color: Theme.faint
                        text: "None yet"
                        topPadding: Theme.px(20)
                        bottomPadding: Theme.px(20)
                        leftPadding: Theme.px(24)
                    }
                    Item { width: 1; height: Theme.px(36) }
                    Label {
                        role: "supporting"
                        color: Theme.muted
                        text: "Found on network"
                        bottomPadding: Theme.px(14)
                    }
                    Separator { width: parent.width }
                    Repeater {
                        id: foundRepeater
                        model: Moonlight.found
                        HostRow {
                            id: foundRow
                            required property var modelData
                            kind: "found"
                            host: modelData
                            onUpPressed: root.moveRow(foundRow, -1)
                            onDownPressed: root.moveRow(foundRow, 1)
                            onPairRequested: (address) => root.startPair(address)
                        }
                    }
                    HostRow {
                        id: addRow
                        kind: "add"
                        onUpPressed: root.moveRow(addRow, -1)
                        onDownPressed: root.moveRow(addRow, 1)
                        onAddRequested: root.go("address")
                    }
                }
            }

            Separator { id: hostsFooterLine; anchors.bottom: hostsHints.top; anchors.bottomMargin: Theme.px(28); width: parent.width }
            HintBar {
                id: hostsHints
                anchors.bottom: parent.bottom
                hints: [{ a: "select", t: "Select" }, { a: "back", t: "Back" }]
            }
        }

        // ------------------------------------------------------------ quit running app?
        FocusScope {
            anchors.fill: parent
            visible: root.page === "quit"
            Column {
                anchors.centerIn: parent
                width: Theme.px(900)
                spacing: Theme.px(18)
                Icon { anchors.horizontalCenter: parent.horizontalCenter; name: "stop"; size: Theme.px(96); tint: Theme.warning }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    role: "title"
                    text: "Quit " + (root.runningApp ? root.runningApp.name : "the running app") + "?"
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    role: "supporting"
                    color: Theme.muted
                    text: (root.runningApp ? root.runningApp.name : "Another app") + " is running on " + (root.cur.name || "the host")
                          + ". Starting " + root.pendingName + " quits it; unsaved progress may be lost."
                }
                Item { width: 1; height: Theme.px(20) }
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Theme.px(16)
                    FlatButton {
                        id: quitStart
                        text: "Quit and start"
                        iconName: "play"
                        onActivated: { Moonlight.quitAndPlay(root.pendingApp); root.go("library") }
                        KeyNavigation.right: quitCancel
                    }
                    FlatButton {
                        id: quitCancel
                        text: "Cancel"
                        iconName: "x"
                        onActivated: root.go("library")
                        KeyNavigation.left: quitStart
                    }
                }
            }
        }

        StreamPairing {
            id: pairing
            anchors.fill: parent
            visible: root.page === "pair"
            onCancelled: { Moonlight.cancelPair(); root.go("hosts") }
            onRetry: { const a = Moonlight.pairing.address; root.startPair(a) }
        }
    }

    TextEntry {
        id: entry
        anchors.fill: parent
        visible: root.page === "address"
        enabled: visible
        title: "Add host"
        iconName: "monitor"
        submitText: "Pair"
        address: true
        hint: "IP address or name of the PC, e.g. 192.168.1.20"
        onSubmitted: (text) => root.startPair(text)
        onCancelled: root.go("hosts")
        backdrop: "transparent"
        onVisibleChanged: if (visible) { text = ""; error = ""; mode = "lower"; row = 0; col = 0 }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_F2 && (root.page === "library" || root.page === "hosts")) {
            event.accepted = true
            root.go(root.page === "hosts" ? "library" : "hosts")
            return
        }
        if (!Theme.isBack(event.key) || root.page === "address")
            return
        event.accepted = true
        root.back()
    }
}
