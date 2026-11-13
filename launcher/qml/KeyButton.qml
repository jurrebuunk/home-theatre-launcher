import QtQuick

// One on-screen keyboard key. Focus is drawn from `highlighted` (the
// keyboard keeps a cursor instead of moving Qt focus between 50 keys).
Rectangle {
    id: root
    property string text
    property string iconName
    property bool highlighted: false
    property bool primary: false
    signal clicked()

    readonly property color fg: highlighted ? Theme.background : primary ? Theme.accent : Theme.text

    color: highlighted ? Theme.accent : Theme.surface
    border.color: primary && !highlighted ? Theme.accent : Theme.border
    border.width: highlighted ? 0 : Theme.borderWidth

    FocusFrame { shown: root.highlighted }
    Label {
        anchors.centerIn: parent
        visible: root.iconName === ""
        role: root.primary ? "body" : "value"
        font.pixelSize: root.primary ? Theme.font("body") : Theme.px(32)
        text: root.text
        color: root.fg
    }
    Icon {
        anchors.centerIn: parent
        visible: root.iconName !== ""
        name: root.iconName
        size: Theme.px(36)
        tint: root.fg
    }
    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
