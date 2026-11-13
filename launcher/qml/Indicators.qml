import QtQuick

// Notifications (top-right, max 3) and the volume indicator (bottom-centre).
// Purely informative: no dimming, no input.
Item {
    id: root

    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        spacing: Theme.px(20)
        move: Transition { NumberAnimation { properties: "y"; duration: Theme.transitionMs } }
        add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.transitionMs } }

        Repeater {
            model: Notes.items
            Rectangle {
                id: card
                readonly property var n: modelData
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
        opacity: Notes.volumeShown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.transitionMs } }

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
