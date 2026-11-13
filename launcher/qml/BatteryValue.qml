import QtQuick

// Horizontal battery icon + percentage (mono), tinted by level or tone.
Row {
    id: root
    property int percent: -1
    property color tint: Theme.muted
    property int size: Theme.px(30)
    spacing: Theme.px(10)
    visible: percent >= 0
    Icon {
        anchors.verticalCenter: parent.verticalCenter
        name: root.percent >= 60 ? "battery-high" : root.percent > 20 ? "battery-mid" : "battery-low"
        size: root.size
        tint: root.tint
    }
    Label {
        anchors.verticalCenter: parent.verticalCenter
        role: "value"
        font.pixelSize: Theme.px(24)
        text: root.percent
        color: root.tint
    }
}
