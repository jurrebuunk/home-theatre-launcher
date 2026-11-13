import QtQuick

// Full-width row button of the in-app menu: icon, label, optional value.
Item {
    id: root
    property string text
    property string iconName
    property color iconTint: Theme.accent
    property string value
    signal activated()

    activeFocusOnTab: true
    width: parent ? parent.width : 0
    height: Theme.px(96)
    readonly property color fg: activeFocus ? Theme.background : Theme.text

    Rectangle {
        anchors.fill: parent
        color: root.activeFocus ? Theme.accent : "transparent"
        border.color: Theme.border
        border.width: root.activeFocus ? 0 : Theme.borderWidth
    }
    FocusFrame {}
    Icon {
        id: icon
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(32)
        anchors.verticalCenter: parent.verticalCenter
        name: root.iconName
        size: Theme.px(34)
        tint: root.activeFocus ? Theme.background : root.iconTint
    }
    Label {
        anchors.left: icon.right
        anchors.leftMargin: Theme.px(28)
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: Theme.px(32)
        text: root.text
        color: root.fg
    }
    Label {
        anchors.right: parent.right
        anchors.rightMargin: Theme.px(28)
        anchors.verticalCenter: parent.verticalCenter
        visible: root.value !== ""
        role: "value"
        font.pixelSize: Theme.px(24)
        text: root.value
        color: root.activeFocus ? Theme.background : Theme.muted
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
