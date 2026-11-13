import QtQuick

// Couch-friendly code entry: Left/Right pick a cell, Up/Down change the
// digit, number keys type directly.
FocusScope {
    id: root
    property int length: 6
    property var digits: []
    property int cursor: 0
    readonly property string value: digits.join("")

    function reset() {
        const d = []
        for (let i = 0; i < length; ++i) d.push(0)
        digits = d
        cursor = 0
    }
    function bump(delta) {
        const d = digits.slice()
        d[cursor] = (d[cursor] + delta + 10) % 10
        digits = d
    }
    onLengthChanged: reset()
    Component.onCompleted: reset()

    activeFocusOnTab: true
    width: row.width
    height: Theme.px(120)

    Row {
        id: row
        spacing: Theme.px(12)
        Repeater {
            model: root.length
            Rectangle {
                readonly property bool current: root.activeFocus && index === root.cursor
                width: Theme.px(88)
                height: Theme.px(120)
                color: current ? Theme.accent : Theme.surface
                border.color: Theme.border
                border.width: current ? 0 : Theme.borderWidth
                Label {
                    anchors.centerIn: parent
                    role: "code"
                    font.pixelSize: Theme.px(64)
                    text: root.digits[index] !== undefined ? root.digits[index] : 0
                    color: parent.current ? Theme.background : Theme.text
                }
            }
        }
    }
    FocusFrame { shown: root.activeFocus }

    Keys.onLeftPressed: cursor = Math.max(0, cursor - 1)
    Keys.onRightPressed: cursor = Math.min(length - 1, cursor + 1)
    Keys.onUpPressed: bump(1)
    Keys.onDownPressed: (event) => {
        // Down from the cell moves to the buttons; use the digit keys or Up to change.
        event.accepted = false
    }
    Keys.onPressed: (event) => {
        if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
            const d = digits.slice()
            d[cursor] = event.key - Qt.Key_0
            digits = d
            cursor = Math.min(length - 1, cursor + 1)
            event.accepted = true
        }
    }
}
