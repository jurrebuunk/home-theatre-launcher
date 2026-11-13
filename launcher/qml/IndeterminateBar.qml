import QtQuick

// Indeterminate progress: a 1/3-width segment sliding along a flat track.
Rectangle {
    id: track
    property bool running: visible
    implicitWidth: Theme.px(500)
    implicitHeight: Math.max(2, Theme.px(3))
    color: Theme.surface

    Rectangle {
        id: segment
        width: track.width / 3
        height: track.height
        color: Theme.accent
        x: 0
        SequentialAnimation on x {
            running: track.running
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: track.width - segment.width; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { from: track.width - segment.width; to: 0; duration: 900; easing.type: Easing.InOutQuad }
        }
    }
}
