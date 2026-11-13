import QtQuick

Window {
    id: window
    width: 1920
    height: 1080
    visible: false
    color: Theme.background
    title: "Home Theatre"

    property bool bluetoothOpen: false
    property string debugView: ""

    onHeightChanged: Theme.screenHeight = height
    Component.onCompleted: {
        Theme.screenHeight = height
        Qt.callLater(restoreFocus)
    }

    function openBluetooth() {
        if (Life.state !== "launcher" || !Cfg.valid)
            return
        bluetoothOpen = true
    }
    function closeBluetooth() {
        bluetoothOpen = false
        Qt.callLater(restoreFocus)
    }
    function restoreFocus() {
        if (Life.state === "error")
            return // ErrorView focuses itself
        if (bluetoothOpen)
            btLoader.item && btLoader.item.focusDefault()
        else
            home.focusTile(Life.lastIndex)
    }

    onDebugViewChanged: {
        if (debugView === "bluetooth" || debugView === "pairing" || debugView === "bt-no-adapter")
            bluetoothOpen = true
    }

    FocusScope {
        id: rootScope
        anchors.fill: parent
        focus: true

        HomeView {
            id: home
            anchors.fill: parent
            focus: true
            visible: !window.bluetoothOpen
            onOpenBluetooth: window.openBluetooth()
        }

        Loader {
            id: btLoader
            anchors.fill: parent
            active: window.bluetoothOpen
            sourceComponent: BluetoothView {
                forceNoAdapter: window.debugView === "bt-no-adapter"
                onCloseRequested: window.closeBluetooth()
            }
            onLoaded: Qt.callLater(() => { item.forceActiveFocus(); item.focusDefault() })
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
            if (window.bluetoothOpen) {
                if (btLoader.item)
                    btLoader.item.close()
                else
                    window.closeBluetooth()
            }
        }
    }
}
