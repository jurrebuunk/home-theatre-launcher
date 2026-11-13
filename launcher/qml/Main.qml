import QtQuick

Window {
    id: window
    width: 1920
    height: 1080
    visible: false
    color: Theme.background
    title: "Home Theatre"

    // "" (home), "bluetooth" or "network"
    property string panel: ""
    property string debugView: ""

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
            visible: window.panel === ""
            onOpenBluetooth: window.openBluetooth()
            onOpenNetwork: window.openNetwork()
        }

        Loader {
            id: panelLoader
            anchors.fill: parent
            active: window.panel !== ""
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
