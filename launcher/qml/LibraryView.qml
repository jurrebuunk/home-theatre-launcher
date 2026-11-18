import QtQuick

// Game library of an app with type "library" (e.g. an emulator): the game
// files in its folders as box art (A plays the game in the app).
FocusScope {
    id: root
    property var app: null
    property string debugView
    signal closeRequested()

    property var games: []
    readonly property bool showGrid: games.length > 0

    function focusDefault() {
        grid.forceActiveFocus()
    }
    function reload() {
        const keep = grid.currentIndex >= 0 && grid.currentIndex < games.length ? games[grid.currentIndex].path : ""
        games = root.app ? Games.games(root.app.id) : []
        const i = games.findIndex(g => g.path === keep)
        grid.currentIndex = games.length > 0 ? Math.max(0, i) : -1
    }
    function play(game) {
        if (game && root.app)
            Games.play(root.app.id, game.path)
    }

    // Rescan when back from a game (files may have been added meanwhile).
    readonly property bool live: Life.state === "launcher"
    onLiveChanged: if (live) reload()
    Component.onCompleted: reload()

    // The focused game's box art behind the library, as in Moonlight.
    readonly property var artBg: Theme.layout.moonlight.boxArtBackground
    property string backdropArt: ""
    readonly property string focusedArt: {
        const g = grid.currentIndex >= 0 && grid.currentIndex < games.length ? games[grid.currentIndex] : null
        return g && g.art ? g.art : ""
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

    Item {
        id: frame
        anchors.fill: parent
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical)
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical)

        Row {
            id: header
            height: Theme.px(Theme.layout.statusRowHeight + 20)
            spacing: Theme.px(28)
            Icon { name: "gamepad"; size: Theme.px(48); tint: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
            Label { role: "display"; text: root.app ? root.app.label : ""; anchors.verticalCenter: parent.verticalCenter }
        }
        Separator { id: headerLine; anchors.top: header.bottom; width: parent.width }

        // One row of box art, like the home screen.
        ListView {
            id: grid
            readonly property real cardHeight: Theme.px(Theme.layout.moonlight.cardHeight)
            readonly property real cardWidth: Math.round(cardHeight * 204 / 320)
            readonly property real gap: Theme.px(40)
            readonly property real grow: Theme.layout.moonlight.focusScale
            // Room for the focused (scaled) card.
            readonly property real pad: cardWidth * (grow - 1) / 2 + Theme.px(12)
            y: headerLine.y + headerLine.height + (footerLine.y - headerLine.y - headerLine.height - height) / 2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: -pad
            anchors.rightMargin: -pad
            height: Math.min(cardHeight * grow + 2 * Theme.px(12), footerLine.y - headerLine.y - headerLine.height)
            visible: root.showGrid
            clip: true
            orientation: ListView.Horizontal
            spacing: gap
            // Centred while it fits (or left-aligned, as the home screen).
            readonly property real spare: width - 2 * pad - (count * cardWidth + Math.max(0, count - 1) * gap)
            leftMargin: pad + (Theme.layout.alignment === "left" ? 0 : Math.max(0, spare / 2))
            rightMargin: pad
            model: root.games
            keyNavigationEnabled: true
            highlightFollowsCurrentItem: false
            boundsBehavior: Flickable.StopAtBounds
            delegate: Item {
                id: cell
                required property int index
                required property var modelData
                width: grid.cardWidth
                height: grid.height
                BoxArtTile {
                    anchors.verticalCenter: parent.verticalCenter
                    cardHeight: grid.cardHeight
                    name: cell.modelData.name
                    art: cell.modelData.art
                    running: false
                    focused: grid.activeFocus && cell.ListView.isCurrentItem
                    onActivated: {
                        grid.currentIndex = cell.index
                        grid.forceActiveFocus()
                        root.play(cell.modelData)
                    }
                }
                z: ListView.isCurrentItem ? 2 : 0
            }
            Keys.onPressed: (event) => {
                if (Theme.isSelect(event.key) && currentIndex >= 0 && currentIndex < root.games.length) {
                    event.accepted = true
                    root.play(root.games[currentIndex])
                }
            }
        }

        Column {
            visible: !root.showGrid
            anchors.centerIn: parent
            width: Theme.px(900)
            spacing: Theme.px(18)
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: "gamepad"
                size: Theme.px(96)
                tint: Theme.muted
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                role: "title"
                text: "No games found"
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                role: "supporting"
                color: Theme.muted
                text: "Put game files in the library folder."
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
                hints: root.showGrid ? [{ a: "select", t: "Play" }, { a: "back", t: "Back" }] : [{ a: "back", t: "Back" }]
            }
        }
    }

    Keys.onPressed: (event) => {
        if (!Theme.isBack(event.key))
            return
        event.accepted = true
        root.closeRequested()
    }
}
