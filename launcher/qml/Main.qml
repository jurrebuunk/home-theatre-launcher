import QtQuick

Window {
    id: window
    width: 1920
    height: 1080
    visible: false
    // Transparent while the in-app menu dims a running app behind it.
    color: overlayMode ? "transparent" : Theme.background
    title: "Home Theatre"

    // "" (home), "bluetooth" or "network"
    property string panel: ""
    property string debugView: ""
    readonly property bool overlayMode: Overlay.open && Life.state === "running"

    onHeightChanged: Theme.screenHeight = height
    Component.onCompleted: {
        Theme.screenHeight = height
        Qt.callLater(restoreFocus)
    }

    function openPanel(name) {
        if (Life.state !== "launcher" || !Cfg.valid)
            return
        panel = name
    }
    function openBluetooth() { openPanel("bluetooth") }
    function openNetwork() { openPanel("network") }
    function closePanel() {
        panel = ""
        Qt.callLater(restoreFocus)
    }
    function restoreFocus() {
        if (overlayMode)
            return
        if (Life.state === "error")
            return // ErrorView focuses itself
        if (panel !== "")
            panelLoader.item && panelLoader.item.focusDefault()
        else
            home.focusTile(Life.lastIndex)
    }

    onDebugViewChanged: {
        if (debugView === "bluetooth" || debugView === "pairing" || debugView === "bt-no-adapter")
            panel = "bluetooth"
        else if (debugView.startsWith("network"))
            panel = "network"
    }

    FocusScope {
        id: rootScope
        anchors.fill: parent
        focus: true

        HomeView {
            id: home
            anchors.fill: parent
            focus: true
            visible: window.panel === "" && !window.overlayMode
            onOpenBluetooth: window.openBluetooth()
            onOpenNetwork: window.openNetwork()
        }

        Loader {
            id: panelLoader
            anchors.fill: parent
            active: window.panel !== ""
            visible: !window.overlayMode
            sourceComponent: window.panel === "bluetooth" ? btComponent : netComponent
            onLoaded: Qt.callLater(() => { item.forceActiveFocus(); item.focusDefault() })
        }
        Component {
            id: btComponent
            BluetoothView {
                forceNoAdapter: window.debugView === "bt-no-adapter"
                onCloseRequested: window.closePanel()
            }
        }
        Component {
            id: netComponent
            NetworkView {
                debugView: window.debugView
                onCloseRequested: window.closePanel()
            }
        }

        TransitionView {
            anchors.fill: parent
            visible: opacity > 0 && !window.overlayMode
            shown: Life.state === "launching" || Life.state === "closing" || Life.state === "running"
        }

        ErrorView {
            id: errorView
            anchors.fill: parent
            shown: Life.state === "error"
        }

        IdleView {
            anchors.fill: parent
        }

        // Screenshots only: a stand-in for the running app behind the menu.
        Image {
            anchors.fill: parent
            visible: window.overlayMode && window.debugView.startsWith("overlay")
            source: visible ? "image://icon/background/mountains" : ""
            sourceSize: Qt.size(width, height)
        }
        Loader {
            id: overlayLoader
            anchors.fill: parent
            active: window.overlayMode
            sourceComponent: OverlayView {}
            onLoaded: {
                item.forceActiveFocus()
                const sub = window.debugView.replace(/^overlay-?/, "")
                item.open(sub === "" ? "main" : sub)
            }
        }

        FatalConfigView {
            anchors.fill: parent
            visible: !Cfg.valid
        }
    }

    Connections {
        target: Life
        function onChanged() {
            if (Life.state === "launcher")
                Qt.callLater(window.restoreFocus)
        }
        function onHomeAtLauncher() {
            if (window.panel !== "") {
                if (panelLoader.item)
                    panelLoader.item.close()
                else
                    window.closePanel()
            }
        }
    }
}
