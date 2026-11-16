import QtQuick

// Notifications (top-centre, max 3) and the volume indicator (bottom-centre).
// Purely informative: no dimming, no input. Notifications swoosh in from the
// top edge and pop out; the volume indicator pops in and out.
Item {
    id: root

    // Notes.items is replaced as a whole; mirror it into a ListModel by id so
    // the list can animate each card in and out.
    ListModel { id: cards }
    function sync() {
        const items = Notes.items
        const ids = items.map(n => n.id)
        for (let i = cards.count - 1; i >= 0; --i)
            if (ids.indexOf(cards.get(i).nid) < 0)
                cards.remove(i)
        items.forEach((n, i) => {
            let j = -1
            for (let k = 0; k < cards.count; ++k)
                if (cards.get(k).nid === n.id) { j = k; break }
            if (j < 0) {
                cards.insert(i, { nid: n.id, json: JSON.stringify(n) })
                return
            }
            if (j !== i)
                cards.move(j, i, 1)
            const json = JSON.stringify(n)
            if (cards.get(i).json !== json)
                cards.setProperty(i, "json", json)
        })
    }
    Connections {
        target: Notes
        function onItemsChanged() { root.sync() }
    }
    Component.onCompleted: sync()

    ListView {
        id: list
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical)
        width: Theme.px(600)
        height: parent.height / 2
        interactive: false
        spacing: Theme.px(20)
        model: cards
        // Swoosh down from above the screen edge.
        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "y"; from: -list.anchors.topMargin - Theme.px(104); duration: 420; easing.type: Easing.OutCubic }
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 220 }
            }
        }
        // Pop: a quick swell, then shrink away.
        remove: Transition {
            SequentialAnimation {
                NumberAnimation { property: "scale"; to: 1.06; duration: 80; easing.type: Easing.OutQuad }
                ParallelAnimation {
                    NumberAnimation { property: "scale"; to: 0.5; duration: 160; easing.type: Easing.InBack }
                    NumberAnimation { property: "opacity"; to: 0; duration: 160 }
                }
            }
        }
        displaced: Transition {
            NumberAnimation { property: "y"; duration: 300; easing.type: Easing.OutCubic }
            NumberAnimation { properties: "scale,opacity"; to: 1; duration: 150 }
        }

        delegate: Rectangle {
            id: card
            required property string json
            readonly property var n: JSON.parse(json)
            readonly property color toneColor: n.tone === "critical" ? Theme.error
                                             : n.tone === "warning" ? Theme.warning : Theme.accent
            width: Theme.px(600)
            height: Theme.px(104)
            color: Theme.surface
            border.color: n.tone === "critical" ? Theme.error : Theme.border
            border.width: Theme.borderWidth

            Row {
                id: lead
                anchors.left: parent.left
                anchors.leftMargin: Theme.px(30)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(24)
                // Sound output switched: speaker → output
                Icon {
                    visible: card.n.style === "sound"
                    anchors.verticalCenter: parent.verticalCenter
                    name: "volume"; size: Theme.px(32); tint: Theme.muted
                }
                Icon {
                    visible: card.n.style === "sound"
                    anchors.verticalCenter: parent.verticalCenter
                    name: "arrow-right"; size: Theme.px(22); tint: Theme.faint
                }
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: card.n.icon || "bluetooth"
                    size: Theme.px(40)
                    tint: card.n.tone === "normal" && card.n.trailing === "link-off" ? Theme.muted : card.toneColor
                }
            }
            Label {
                anchors.left: lead.right
                anchors.leftMargin: Theme.px(24)
                anchors.right: trail.left
                anchors.rightMargin: Theme.px(20)
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: Theme.px(30)
                text: card.n.title
            }
            Row {
                id: trail
                anchors.right: parent.right
                anchors.rightMargin: Theme.px(30)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(24)
                BatteryValue {
                    anchors.verticalCenter: parent.verticalCenter
                    percent: card.n.value
                    tint: card.n.tone === "normal" ? Theme.muted : card.toneColor
                }
                Icon {
                    visible: !!card.n.trailing
                    anchors.verticalCenter: parent.verticalCenter
                    name: card.n.trailing || "link"
                    size: Theme.px(30)
                    tint: card.n.trailing === "down" ? Theme.error
                        : card.n.trailing === "link-off" ? Theme.muted : Theme.accent
                }
            }
        }
    }

    Rectangle {
        id: volume
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical + 40)
        width: Theme.px(640)
        height: Theme.px(96)
        color: Theme.surface
        border.color: Theme.border
        border.width: Theme.borderWidth
        // Pop in (overshoot) and out (shrink), no sliding.
        readonly property bool shown: Notes.volumeShown
        opacity: shown ? 1 : 0
        scale: shown ? 1 : 0.6
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: volume.shown ? 140 : 160 } }
        Behavior on scale {
            NumberAnimation {
                duration: volume.shown ? 260 : 160
                easing.type: volume.shown ? Easing.OutBack : Easing.InBack
            }
        }

        Icon {
            id: volIcon
            anchors.left: parent.left
            anchors.leftMargin: Theme.px(36)
            anchors.verticalCenter: parent.verticalCenter
            name: Sound.muted ? "volume-off" : "volume"
            size: Theme.px(36)
            tint: Theme.text
        }
        Rectangle {
            anchors.left: volIcon.right
            anchors.leftMargin: Theme.px(28)
            anchors.right: volValue.left
            anchors.rightMargin: Theme.px(40)
            anchors.verticalCenter: parent.verticalCenter
            height: Math.max(3, Theme.px(8))
            color: Theme.surfaceAlt
            Rectangle {
                height: parent.height
                width: Sound.muted ? 0 : parent.width * Math.min(1, Sound.volume / Math.max(1, Sound.maxVolume))
                color: Theme.accent
                Behavior on width { NumberAnimation { duration: Theme.transitionMs } }
            }
        }
        Label {
            id: volValue
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(32)
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(64)
            horizontalAlignment: Text.AlignRight
            role: "value"
            font.pixelSize: Theme.px(32)
            text: Sound.muted ? "" : Sound.volume
        }
    }
}
