import QtQuick

// In-app menu over a running app: the app keeps running (and moving) behind
// a dim layer; a side panel offers Resume, Bluetooth, Network, Sound and
// Close app. For a Moonlight stream it is about the game: Stop playing quits
// it on the host, Disconnect leaves it running there.
FocusScope {
    id: root
    property string page: "main" // main | bluetooth | network | sound | close
    property int runtime: 0      // minutes the app has been running
    readonly property bool stream: !!Life.session.host

    function open(p) {
        page = p || "main"
        updateRuntime()
        Qt.callLater(focusPage)
    }
    function focusPage() {
        if (page === "main")
            resume.forceActiveFocus()
        else if (page === "close")
            confirmClose.forceActiveFocus()
        else if (subLoader.item) {
            subLoader.item.forceActiveFocus()
            subLoader.item.focusDefault()
        }
    }
    function go(p) {
        page = p
        Qt.callLater(focusPage)
    }
    function updateRuntime() {
        runtime = Life.runningSince > 0 ? Math.max(0, Math.floor((Date.now() - Life.runningSince) / 60000)) : 0
    }
    readonly property string runtimeText: Math.floor(runtime / 60) + ":" + String(runtime % 60).padStart(2, "0")

    readonly property real mainWidth: Theme.px(680)
    readonly property real wideWidth: Theme.px(1000)
    readonly property bool fullWidth: page === "network" && subLoader.item && subLoader.item.entryOpen

    Timer {
        interval: 15000
        repeat: true
        running: root.visible
        onTriggered: root.updateRuntime()
    }

    // Dim layer: the app stays visible and keeps running underneath.
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: Overlay.dim
    }
    MouseArea {
        anchors.fill: parent
        onClicked: Overlay.close()
    }

    Rectangle {
        id: panel
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: root.fullWidth ? root.width : (root.page === "main" || root.page === "close") ? root.mainWidth : root.wideWidth
        color: Theme.surface
        Behavior on width { NumberAnimation { duration: Theme.transitionMs } }
        MouseArea { anchors.fill: parent }

        Separator {
            vertical: true
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
        }

        Item {
            id: content
            anchors.fill: parent
            anchors.leftMargin: root.fullWidth ? 0 : Theme.px(64)
            anchors.rightMargin: root.fullWidth ? 0 : Theme.px(Theme.layout.safeMargin)
            anchors.topMargin: root.fullWidth ? 0 : Theme.px(Theme.layout.safeMarginVertical)
            anchors.bottomMargin: root.fullWidth ? 0 : Theme.px(Theme.layout.safeMarginVertical)

            // ---------------------------------------------------- main
            FocusScope {
                id: mainPage
                anchors.fill: parent
                visible: root.page === "main"

                Column {
                    id: clock
                    y: Theme.px(10)
                    width: parent.width
                    spacing: -Theme.px(6)
                    Label {
                        role: "idleClock"
                        font.pixelSize: Theme.px(112)
                        text: Clock.text
                        color: Qt.lighter(Theme.text, 1.05)
                    }
                    Label {
                        role: "idleDate"
                        font.pixelSize: Theme.px(30)
                        text: Clock.dateText
                        color: Theme.muted
                        leftPadding: Theme.px(2)
                    }
                }

                Separator { id: line1; anchors.top: clock.bottom; anchors.topMargin: Theme.px(36); width: parent.width }
                Item {
                    id: appRow
                    anchors.top: line1.bottom
                    width: parent.width
                    height: Theme.px(108)
                    Icon {
                        id: appIcon
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !cover.visible
                        isApp: true
                        appPath: Life.appIcon
                        size: Theme.px(48)
                        tint: Theme.accent
                    }
                    // A stream shows the game's box art instead of the app icon.
                    Image {
                        id: cover
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.stream && status === Image.Ready
                        width: Theme.px(54)
                        height: Theme.px(72)
                        source: root.stream ? (Life.session.art || "") : ""
                        sourceSize: Qt.size(width, height)
                        fillMode: Image.PreserveAspectCrop
                    }
                    Column {
                        anchors.left: cover.visible ? cover.right : appIcon.right
                        anchors.leftMargin: Theme.px(26)
                        anchors.right: runtimeRow.left
                        anchors.rightMargin: Theme.px(20)
                        anchors.verticalCenter: parent.verticalCenter
                        Label {
                            width: parent.width
                            font.pixelSize: Theme.px(34)
                            text: Life.appLabel
                        }
                        Label {
                            visible: root.stream
                            width: parent.width
                            role: "supporting"
                            color: Theme.muted
                            text: "on " + (Life.session.hostName || "")
                        }
                    }
                    Row {
                        id: runtimeRow
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.px(16)
                        visible: Life.runningSince > 0
                        Icon { name: "clock"; size: Theme.px(26); tint: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                        Label { role: "value"; font.pixelSize: Theme.px(24); text: root.runtimeText; color: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
                Separator { id: line2; anchors.top: appRow.bottom; width: parent.width }

                Column {
                    anchors.top: line2.bottom
                    anchors.topMargin: Theme.px(24)
                    width: parent.width
                    spacing: Theme.px(12)
                    OverlayButton {
                        id: resume
                        text: "Resume"; iconName: "play"
                        KeyNavigation.down: btButton
                        onActivated: Overlay.close()
                    }
                    OverlayButton {
                        id: btButton
                        text: "Bluetooth"; iconName: Bt.hasAdapter ? "bluetooth" : "bluetooth-off"
                        value: Bt.connectedCount > 0 ? String(Bt.connectedCount) : ""
                        KeyNavigation.up: resume
                        KeyNavigation.down: netButton
                        onActivated: root.go("bluetooth")
                    }
                    OverlayButton {
                        id: netButton
                        text: "Network"; iconName: Theme.networkIcon()
                        KeyNavigation.up: btButton
                        KeyNavigation.down: soundButton.visible ? soundButton : closeButton
                        onActivated: root.go("network")
                    }
                    OverlayButton {
                        id: soundButton
                        visible: Cfg.sound.enable
                        text: "Sound"; iconName: Sound.muted ? "volume-off" : "volume"
                        valueIcon: Sound.available ? Theme.soundIcon(Sound.currentKind) : ""
                        value: !Sound.available ? "" : Sound.muted ? "—" : String(Sound.volume)
                        KeyNavigation.up: netButton
                        KeyNavigation.down: closeButton
                        onActivated: root.go("sound")
                    }
                    Item { width: 1; height: Theme.px(10) }
                    OverlayButton {
                        id: closeButton
                        text: root.stream ? "Stop playing" : "Close app"
                        iconName: root.stream ? "stop" : "power"; iconTint: Theme.error
                        KeyNavigation.up: soundButton.visible ? soundButton : netButton
                        onActivated: root.go("close")
                    }
                }

                Separator { anchors.bottom: mainHints.top; anchors.bottomMargin: Theme.px(28); width: parent.width }
                HintBar {
                    id: mainHints
                    anchors.bottom: parent.bottom
                    hints: [{ a: "select", t: "Select" }, { a: "back", t: "Resume" }]
                }
                Keys.onPressed: (event) => {
                    if (Theme.isBack(event.key)) {
                        event.accepted = true
                        Overlay.close()
                    }
                }
            }

            // ---------------------------------------------------- close confirmation
            FocusScope {
                id: closePage
                anchors.fill: parent
                visible: root.page === "close"
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: -Theme.px(40)
                    width: parent.width
                    spacing: Theme.px(12)
                    Icon { name: root.stream ? "stop" : "power"; size: Theme.px(96); tint: Theme.error }
                    Item { width: 1; height: Theme.px(28) }
                    Label {
                        role: "display"
                        text: (root.stream ? "Stop playing " : "Close ") + Life.appLabel + "?"
                        width: parent.width
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                    }
                    Label {
                        visible: root.stream
                        width: parent.width
                        wrapMode: Text.WordWrap
                        role: "supporting"
                        color: Theme.muted
                        text: Life.appLabel + " quits on " + (Life.session.hostName || "the host")
                              + ". Disconnect leaves it running there."
                    }
                    Item { width: 1; height: Theme.px(16) }
                    OverlayButton {
                        id: confirmClose
                        text: root.stream ? "Stop playing" : "Close"
                        iconName: root.stream ? "stop" : "power"; iconTint: Theme.text
                        KeyNavigation.down: root.stream ? disconnect : keepPlaying
                        // Stops the app (and for a stream the game on the host); the menu closes with it.
                        onActivated: root.stream ? Moonlight.stopPlaying() : Life.home()
                    }
                    OverlayButton {
                        id: disconnect
                        visible: root.stream
                        text: "Disconnect"
                        iconName: "link-off"; iconTint: Theme.text
                        KeyNavigation.up: confirmClose
                        KeyNavigation.down: keepPlaying
                        onActivated: Life.home()
                    }
                    OverlayButton {
                        id: keepPlaying
                        text: "Keep playing"; iconName: "play"; iconTint: Theme.text
                        KeyNavigation.up: root.stream ? disconnect : confirmClose
                        onActivated: Overlay.close()
                    }
                }
                Separator { anchors.bottom: closeHints.top; anchors.bottomMargin: Theme.px(28); width: parent.width }
                HintBar {
                    id: closeHints
                    anchors.bottom: parent.bottom
                    hints: [{ a: "select", t: "Select" }, { a: "back", t: "Back" }]
                }
                Keys.onPressed: (event) => {
                    if (Theme.isBack(event.key)) {
                        event.accepted = true
                        root.go("main")
                    }
                }
            }

            // ---------------------------------------------------- Bluetooth / Network
            Loader {
                id: subLoader
                anchors.fill: parent
                active: root.page === "bluetooth" || root.page === "network" || root.page === "sound"
                sourceComponent: root.page === "bluetooth" ? btPage : root.page === "sound" ? soundPage : netPage
                onLoaded: Qt.callLater(root.focusPage)
            }
            Component {
                id: btPage
                BluetoothView {
                    compact: true
                    onCloseRequested: root.go("main")
                }
            }
            Component {
                id: soundPage
                SoundView {
                    compact: true
                    onCloseRequested: root.go("main")
                }
            }
            Component {
                id: netPage
                NetworkView {
                    compact: true
                    onCloseRequested: root.go("main")
                }
            }
        }
    }
}
