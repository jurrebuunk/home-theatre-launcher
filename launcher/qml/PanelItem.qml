import QtQuick

// Left-column control in the Bluetooth / Network views: surface box, focus
// fill; `selected` marks the current choice with an accent border.
Item {
    id: root
    property string iconName
    property bool selected: false
    default property alias content: slot.data
    signal activated()

    activeFocusOnTab: true
    width: Theme.px(400)
    height: Theme.px(88)
    readonly property color fg: activeFocus ? Theme.background : Theme.text

    Rectangle {
        anchors.fill: parent
        color: root.activeFocus ? Theme.accent : Theme.surface
        border.color: root.selected ? Theme.focus : Theme.border
        border.width: root.activeFocus ? 0 : Theme.borderWidth
    }
    FocusFrame {}
    Icon {
        visible: root.iconName !== ""
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(28)
        anchors.verticalCenter: parent.verticalCenter
        name: root.iconName
        size: Theme.px(32)
        tint: root.fg
    }
    Item {
        id: slot
        anchors.fill: parent
        anchors.leftMargin: Theme.px(28)
        anchors.rightMargin: Theme.px(24)
    }
    Keys.onPressed: (event) => {
        if (Theme.isSelect(event.key)) {
            event.accepted = true
            root.activated()
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: { root.forceActiveFocus(); root.activated() }
    }
}
