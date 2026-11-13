import QtQuick

Item {
    id: root
    property var app
    property int index
    signal activated()

    width: Theme.px(Theme.layout.tile.width)
    height: Theme.px(Theme.layout.tile.height)

    readonly property bool blocked: Life.cleanupIncomplete
    readonly property color fg: activeFocus ? Theme.background : Theme.text

    Rectangle {
        anchors.fill: parent
        color: root.activeFocus ? Theme.accent : Theme.surface
        border.color: Theme.border
        border.width: root.activeFocus ? 0 : Theme.borderWidth
    }
    FocusFrame {}

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.px(Theme.layout.tile.padding)
        spacing: Theme.px(40)
        Icon {
            anchors.horizontalCenter: parent.horizontalCenter
            isApp: true
            appPath: root.app.icon
            size: Theme.px(Theme.layout.iconSize)
            tint: root.activeFocus ? Theme.background : (root.blocked ? Theme.faint : Theme.accent)
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            role: "title"
            text: root.app.label
            color: root.blocked && !root.activeFocus ? Theme.faint : root.fg
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
