import QtQuick

// Square on/off switch: frame with a filled knob.
Rectangle {
    property bool checked: false
    property color knob: Theme.accent
    width: Theme.px(66)
    height: Theme.px(34)
    color: "transparent"
    border.color: Theme.muted
    border.width: Theme.borderWidth

    Rectangle {
        width: parent.height - Theme.px(10)
        height: width
        anchors.verticalCenter: parent.verticalCenter
        x: parent.checked ? parent.width - width - Theme.px(5) : Theme.px(5)
        color: parent.checked ? parent.knob : Theme.faint
        Behavior on x { NumberAnimation { duration: Theme.transitionMs } }
    }
}
