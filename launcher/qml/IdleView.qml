import QtQuick

// Idle screen: large clock and date over a colour, image, slideshow or
// video. Input is handled (and swallowed) by IdleMonitor in C++.
Item {
    id: root
    readonly property var bg: Idle.background
    readonly property bool pictures: bg.type === "image" || bg.type === "slideshow"

    visible: opacity > 0
    opacity: Idle.idle ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 600 } }

    Rectangle {
        anchors.fill: parent
        color: root.bg.color
    }
    Loader {
        anchors.fill: parent
        active: root.visible && root.pictures
        sourceComponent: Slideshow {
            sources: Idle.images
            interval: root.bg.interval * 1000
            fade: root.bg.fadeMs
            fill: root.bg.fill
        }
    }
    Loader {
        anchors.fill: parent
        active: root.visible && root.bg.type === "video"
        source: "IdleVideo.qml"
        onLoaded: {
            const v = String(root.bg.video)
            item.source = v.startsWith("/") ? "file://" + encodeURI(v) : v
            item.muted = root.bg.muted
            item.fill = root.bg.fill
        }
    }
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.bg.type === "color" ? 0 : root.bg.dim
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
