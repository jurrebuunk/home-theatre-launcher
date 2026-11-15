import QtQuick

// One app of a Moonlight host: portrait box art over its name. A small
// play badge marks the app that is running on the host right now.
Item {
    id: root
    property string name
    property string art
    property bool running
    property bool focused: activeFocus
    property real cardHeight: Theme.px(320)
    signal activated()

    // Sizes below are for a 320 px card; everything scales with it.
    readonly property real u: cardHeight / 320
    width: Math.round(cardHeight * 204 / 320)
    height: cardHeight
    z: focused ? 2 : 0
    scale: focused ? 1.08 : 1
    Behavior on scale { NumberAnimation { duration: Theme.transitionMs } }

    readonly property real strip: Math.round(52 * u)

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
                font.pixelSize: Math.round(72 * u)
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
            anchors.leftMargin: Math.round(12 * u)
            anchors.rightMargin: Math.round(8 * u)
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: Math.round(22 * u)
            text: root.name
            color: root.focused ? Theme.background : Theme.text
        }
    }
    Rectangle {
        visible: root.running
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.borderWidth
        width: Math.round(44 * u)
        height: width
        color: Theme.accent
        Icon {
            anchors.centerIn: parent
            name: "play"
            size: Math.round(30 * u)
            tint: Theme.background
        }
    }
    FocusFrame { shown: root.focused }

    MouseArea {
        anchors.fill: parent
        onClicked: root.activated()
    }
}
