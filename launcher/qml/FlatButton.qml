import QtQuick

Item {
    id: root
    property string text
    property string iconName
    property bool compact: false
    signal activated()

    activeFocusOnTab: true
    implicitHeight: Theme.px(compact ? 64 : Theme.layout.buttonHeight)
    implicitWidth: content.implicitWidth + Theme.px(compact ? 56 : 80)
    width: implicitWidth
    height: implicitHeight

    readonly property color fg: activeFocus ? Theme.background : (enabled ? Theme.text : Theme.faint)

    Rectangle {
        anchors.fill: parent
        color: root.activeFocus ? Theme.accent : "transparent"
        border.color: Theme.border
        border.width: root.activeFocus ? 0 : Theme.borderWidth
    }
    FocusFrame {}

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Theme.px(16)
        Icon {
            visible: root.iconName !== ""
            name: root.iconName
            tint: root.fg
            size: Theme.px(compact ? 24 : 28)
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: root.text
            color: root.fg
            anchors.verticalCenter: parent.verticalCenter
        }
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
