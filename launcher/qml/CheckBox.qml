import QtQuick

Item {
    id: root
    property bool checked: false
    property string text
    property string iconName
    signal toggled()

    activeFocusOnTab: true
    implicitWidth: row.implicitWidth + Theme.px(24)
    implicitHeight: Theme.px(56)
    width: implicitWidth
    height: implicitHeight

    readonly property color fg: activeFocus ? Theme.background : Theme.text

    Rectangle { anchors.fill: parent; color: root.activeFocus ? Theme.accent : "transparent" }
    FocusFrame {}
    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.px(16)
        Rectangle {
            width: Theme.px(36)
            height: width
            anchors.verticalCenter: parent.verticalCenter
            color: "transparent"
            border.color: root.activeFocus ? Theme.background : Theme.text
            border.width: Theme.borderWidth
            Icon {
                anchors.centerIn: parent
                visible: root.checked
                name: "check"
                size: Theme.px(28)
                tint: root.fg
            }
        }
        Icon {
            visible: root.iconName !== ""
            name: root.iconName
            size: Theme.px(28)
            tint: root.activeFocus ? Theme.background : Theme.muted
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
            root.checked = !root.checked
            root.toggled()
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: { root.forceActiveFocus(); root.checked = !root.checked; root.toggled() }
    }
}
