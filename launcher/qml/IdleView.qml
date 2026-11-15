import QtQuick

// Idle screen: large clock and date over a colour, image, slideshow or
// video. Input is handled (and swallowed) by IdleMonitor in C++.
Item {
    id: root
    readonly property var bg: Idle.background

    visible: opacity > 0
    opacity: Idle.idle ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 600 } }

    BackgroundView {
        anchors.fill: parent
        bg: root.bg
        images: Idle.images
        dim: root.bg.dim
        blur: root.bg.blur || 0
    }

    StatusIcons {
        visible: Cfg.idle.showStatus
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical) + Theme.px(20)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
    }

    Column {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical) + Theme.px(40)
        spacing: -Theme.px(28)
        Label {
            role: "idleClock"
            text: Clock.text
            color: Qt.lighter(Theme.text, 1.17)
        }
        Label {
            role: "idleDate"
            text: Clock.dateText
            color: Qt.lighter(Theme.text, 1.09)
            leftPadding: Theme.px(8)
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: Idle.idle
    }
}
