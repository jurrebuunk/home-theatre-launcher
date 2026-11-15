import QtQuick

// Sound: volume of the current output and the list of outputs (TV, speakers,
// Bluetooth headphones). Selecting an output makes it the default; playing
// audio follows it.
FocusScope {
    id: root
    property bool compact: false
    signal closeRequested()

    function focusDefault() { volumeRow.forceActiveFocus() }
    function close() { closeRequested() }

    MouseArea { anchors.fill: parent }

    Item {
        id: page
        anchors.fill: parent
        anchors.leftMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMargin)
        anchors.topMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMarginVertical)
        anchors.bottomMargin: root.compact ? 0 : Theme.px(Theme.layout.safeMarginVertical)

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
                name: Sound.muted ? "volume-off" : "volume"
                size: Theme.px(48)
                tint: Theme.accent
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                role: "display"
                text: "Sound"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Separator {
            id: headerLine
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
        }

        Column {
            id: body
            anchors.top: headerLine.bottom
            anchors.topMargin: Theme.px(34)
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Theme.px(16)

            Label {
                visible: !Sound.available
                role: "supporting"
                color: Theme.faint
                text: "No sound server (PipeWire) is running"
            }

            // Volume: Left / Right change it, Select mutes.
            Item {
                id: volumeRow
                visible: Sound.available
                activeFocusOnTab: true
                width: Math.min(parent.width, Theme.px(root.compact ? 2000 : 1200))
                height: Theme.px(96)
                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: volumeRow.activeFocus ? Theme.focus : Theme.border
                    border.width: volumeRow.activeFocus ? Theme.focusWidth : Theme.borderWidth
                }
                Icon {
                    id: minus
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.px(32)
                    anchors.verticalCenter: parent.verticalCenter
                    name: Sound.muted ? "volume-off" : "minus"
                    size: Theme.px(30)
                    tint: Theme.muted
                    MouseArea { anchors.fill: parent; onClicked: Sound.step(-1) }
                }
                Rectangle {
                    id: track
                    anchors.left: minus.right
                    anchors.leftMargin: Theme.px(26)
                    anchors.right: plus.left
                    anchors.rightMargin: Theme.px(26)
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
                Icon {
                    id: plus
                    anchors.right: value.left
                    anchors.rightMargin: Theme.px(36)
                    anchors.verticalCenter: parent.verticalCenter
                    name: "plus"
                    size: Theme.px(30)
                    tint: Theme.muted
                    MouseArea { anchors.fill: parent; onClicked: Sound.step(1) }
                }
                Label {
                    id: value
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.px(30)
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.px(64)
                    horizontalAlignment: Text.AlignRight
                    role: "value"
                    font.pixelSize: Theme.px(30)
                    text: Sound.muted ? "—" : Sound.volume
                }
                Keys.onLeftPressed: Sound.step(-1)
                Keys.onRightPressed: Sound.step(1)
                Keys.onDownPressed: outputs.count > 0 && outputs.itemAt(0).forceActiveFocus()
                Keys.onPressed: (event) => {
                    if (Theme.isSelect(event.key)) {
                        event.accepted = true
                        Sound.toggleMute()
                    }
                }
            }

            Item { width: 1; height: Theme.px(8) }
            Label {
                visible: Sound.available
                role: "supporting"
                color: Theme.muted
                text: "Output"
            }

            Column {
                width: parent.width
                Repeater {
                    id: outputs
                    model: Sound.outputs
                    Item {
                        id: row
                        activeFocusOnTab: true
                        width: parent.width
                        height: Theme.px(Theme.layout.rowHeight + 10)
                        readonly property color fg: activeFocus ? Theme.background : Theme.text
                        Rectangle {
                            anchors.fill: parent
                            color: row.activeFocus ? Theme.accent : "transparent"
                        }
                        FocusFrame {}
                        Icon {
                            id: kindIcon
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.px(24)
                            anchors.verticalCenter: parent.verticalCenter
                            name: Theme.soundIcon(modelData.kind)
                            size: Theme.px(40)
                            tint: row.activeFocus ? Theme.background : Theme.muted
                        }
                        Label {
                            anchors.left: kindIcon.right
                            anchors.leftMargin: Theme.px(28)
                            anchors.right: status.left
                            anchors.rightMargin: Theme.px(24)
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: row.fg
                        }
                        Row {
                            id: status
                            anchors.right: parent.right
                            anchors.rightMargin: Theme.px(40)
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.px(36)
                            Icon {
                                visible: modelData.bluetooth
                                anchors.verticalCenter: parent.verticalCenter
                                name: "bluetooth"
                                size: Theme.px(28)
                                tint: row.activeFocus ? Theme.background : Theme.muted
                            }
                            BatteryValue {
                                anchors.verticalCenter: parent.verticalCenter
                                percent: modelData.battery
                                tint: row.activeFocus ? Theme.background : Theme.muted
                            }
                            Item {
                                width: Theme.px(32)
                                height: Theme.px(32)
                                anchors.verticalCenter: parent.verticalCenter
                                Icon {
                                    visible: modelData.current
                                    name: "check"
                                    size: parent.width
                                    tint: row.activeFocus ? Theme.background : Theme.accent
                                }
                            }
                        }
                        KeyNavigation.up: index > 0 ? outputs.itemAt(index - 1) : volumeRow
                        KeyNavigation.down: index < outputs.count - 1 ? outputs.itemAt(index + 1) : null
                        Keys.onPressed: (event) => {
                            if (Theme.isSelect(event.key)) {
                                event.accepted = true
                                Sound.setOutput(modelData.name)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { row.forceActiveFocus(); Sound.setOutput(modelData.name) }
                        }
                    }
                }
            }
        }

        Separator {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: hints.top
            anchors.bottomMargin: Theme.px(28)
        }
        HintBar {
            id: hints
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            hints: [{ a: "select", t: volumeRow.activeFocus ? "Mute" : "Use" }, { a: "back", t: "Back" }]
                   .concat(Cfg.sound.volumeKeys ? [{ a: "volumeDown", b: "volumeUp", t: "Volume" }] : [])
        }
    }

    Keys.onPressed: (event) => {
        if (Theme.isBack(event.key)) {
            event.accepted = true
            root.close()
        }
    }
}
