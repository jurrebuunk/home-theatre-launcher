import QtQuick

// One app of a Moonlight host: portrait box art over its name. A small
// play badge marks the app that is running on the host right now.
Item {
    id: root
    property string name
    property string art
    property bool running
    property bool focused: activeFocus
    signal activated()

    width: Theme.px(204)
    height: Theme.px(320)
    z: focused ? 2 : 0
    scale: focused ? 1.08 : 1
    Behavior on scale { NumberAnimation { duration: Theme.transitionMs } }

    readonly property real strip: Theme.px(52)

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        border.color: Theme.border
        border.width: Theme.borderWidth
    }
    Item {
        id: artArea
        x: Theme.borderWidth
        y: Theme.borderWidth
        width: parent.width - 2 * Theme.borderWidth
        height: parent.height - root.strip - Theme.borderWidth
        clip: true
        Rectangle {
            anchors.fill: parent
            visible: cover.status !== Image.Ready
            color: Theme.surfaceAlt
            Label {
                anchors.centerIn: parent
                role: "display"
                font.pixelSize: Theme.px(72)
                color: Theme.faint
                text: root.name.split(" ").slice(0, 2).map(w => w.charAt(0)).join("").toUpperCase()
            }
        }
        Image {
            id: cover
            anchors.fill: parent
            source: root.art
            fillMode: Image.PreserveAspectCrop
            sourceSize: Qt.size(width, height)
            asynchronous: true
            smooth: true
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.borderWidth
        height: root.strip - Theme.borderWidth
        color: root.focused ? Theme.accent : Theme.surface
        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.px(12)
            anchors.rightMargin: Theme.px(8)
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Theme.px(22)
            text: root.name
            color: root.focused ? Theme.background : Theme.text
        }
    }
    Rectangle {
        visible: root.running
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.borderWidth
        width: Theme.px(44)
        height: width
        color: Theme.accent
        Icon {
            anchors.centerIn: parent
            name: "play"
            size: Theme.px(30)
            tint: Theme.background
        }
    }
    FocusFrame { shown: root.focused }

    MouseArea {
        anchors.fill: parent
        onClicked: root.activated()
    }
}
