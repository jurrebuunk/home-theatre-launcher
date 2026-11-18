import QtQuick

Window {
    id: window
    width: 1920
    height: 1080
    visible: false
    // Transparent while the in-app menu dims a running app behind it.
    color: overlayMode ? "transparent" : Theme.background
    title: "Home Theatre"

    // "" (home), "bluetooth", "network", "sound", "moonlight" or "library"
    property string panel: ""
    // The Cfg.apps entry whose game library is open (panel "library")
    property var libraryApp: null
    // Home tile to focus when a panel opened from a tile closes (-1: last launched)
    property int returnIndex: -1
    // Notifications / volume drawn inside this window (windowed, screenshots)
    // instead of on their own click-through surface.
    property bool inlineIndicators: true
    property alias indicatorWindow: indicatorWindow
    readonly property bool fakeApp: debugView === "notifications" || debugView === "volume"
                                    || (overlayMode && debugView.startsWith("overlay"))
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
    function openSound() { openPanel("sound") }
    function openMoonlight(index) {
        const i = Cfg.apps.findIndex(a => a.type === "moonlight")
        if (i < 0)
            return
        returnIndex = index >= 0 ? index : i
        openPanel("moonlight")
    }
    function openMoonlightLibrary() { openMoonlight(-1) } // control API
    function openLibrary(index) {
        if (index < 0 || index >= Cfg.apps.length)
            return
        libraryApp = Cfg.apps[index]
        returnIndex = index
        openPanel("library")
    }
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
        else {
            home.focusTile(returnIndex >= 0 ? returnIndex : Life.lastIndex)
            returnIndex = -1
        }
    }

    onDebugViewChanged: {
        if (debugView === "bluetooth" || debugView === "pairing" || debugView === "bt-no-adapter")
            panel = "bluetooth"
        else if (debugView.startsWith("network"))
            panel = "network"
        else if (debugView === "sound")
            panel = "sound"
        else if (debugView.startsWith("moonlight"))
            panel = "moonlight"
        else if (debugView === "library")
            openLibrary(Cfg.apps.findIndex(a => a.type === "library"))
    }

    FocusScope {
        id: rootScope
        anchors.fill: parent
        focus: true

        // Behind home and the panels (not over a running app).
        BackgroundView {
            id: menuBackground
            anchors.fill: parent
            visible: !window.overlayMode && !window.fakeApp
            playing: Life.state === "launcher" && !Idle.idle
            readonly property var cfg: Theme.layout.background
            readonly property bool inPanel: window.panel !== ""
            bg: cfg
            images: Idle.imagesFor(cfg)
            dim: inPanel && cfg.panels.dim >= 0 ? cfg.panels.dim : cfg.dim
            blur: inPanel && cfg.panels.blur >= 0 ? cfg.panels.blur : cfg.blur
            Behavior on dim { NumberAnimation { duration: Theme.transitionMs * 2 } }
            Behavior on blur { NumberAnimation { duration: Theme.transitionMs * 2 } }
        }

        HomeView {
            id: home
            anchors.fill: parent
            focus: true
            visible: window.panel === "" && !window.overlayMode
            onOpenBluetooth: window.openBluetooth()
            onOpenNetwork: window.openNetwork()
            onOpenSound: window.openSound()
            onOpenMoonlight: (index) => window.openMoonlight(index)
            onOpenLibrary: (index) => window.openLibrary(index)
        }

        Loader {
            id: panelLoader
            anchors.fill: parent
            active: window.panel !== ""
            visible: !window.overlayMode
            sourceComponent: window.panel === "bluetooth" ? btComponent
                           : window.panel === "sound" ? soundComponent
                           : window.panel === "moonlight" ? moonlightComponent
                           : window.panel === "library" ? libraryComponent : netComponent
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
            id: soundComponent
            SoundView {
                onCloseRequested: window.closePanel()
            }
        }
        Component {
            id: moonlightComponent
            MoonlightView {
                debugView: window.debugView
                onCloseRequested: window.closePanel()
            }
        }
        Component {
            id: libraryComponent
            LibraryView {
                app: window.libraryApp
                debugView: window.debugView
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
            visible: opacity > 0 && !window.overlayMode && !window.fakeApp
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
            visible: window.fakeApp
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
                const sub = window.debugView.replace(/^overlay-?/, "").replace(/^stream-?/, "").replace(/^stop$/, "close")
                item.open(sub === "" ? "main" : sub)
            }
        }

        Indicators {
            anchors.fill: parent
            visible: window.inlineIndicators
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

    // Click-through layer-shell surface above everything (set up in C++).
    Window {
        id: indicatorWindow
        visible: false
        color: "transparent"
        width: window.width
        height: window.height
        Indicators {
            anchors.fill: parent
            visible: !window.inlineIndicators
        }
    }
}
