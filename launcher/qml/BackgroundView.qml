import QtQuick
import QtQuick.Effects

// A configured background: colour, picture, slideshow or looping video,
// blurred and darkened. The blurred result is kept in a layer, so it is
// only redrawn when the picture underneath changes.
Item {
    id: root
    property var bg: ({})       // type, color, fill, interval, fadeMs, video, muted
    property var images: []     // picture URLs (image / slideshow)
    property real dim: 0
    property real blur: 0       // strength 0..64, scaled like px
    property bool playing: true // video only while this is true

    readonly property bool pictures: bg.type === "image" || bg.type === "slideshow"
    readonly property bool video: bg.type === "video"
    // The software renderer has no shader effects: show the picture sharp.
    readonly property bool canBlur: GraphicsInfo.api !== GraphicsInfo.Software
    readonly property int blurPx: canBlur ? Math.min(64, Theme.px(blur)) : 0
    clip: true

    Rectangle {
        anchors.fill: parent
        color: root.bg.color || Theme.background
    }

    Item {
        id: content
        anchors.fill: parent
        // Blur pulls transparent edges in; draw past them, out of sight.
        anchors.margins: -root.blurPx
        visible: root.blurPx === 0
        Loader {
            anchors.fill: parent
            active: root.visible && root.pictures
            sourceComponent: Slideshow {
                sources: root.images
                interval: (root.bg.interval || 30) * 1000
                fade: root.bg.fadeMs !== undefined ? root.bg.fadeMs : 1500
                fill: root.bg.fill || "cover"
                running: root.visible
            }
        }
        Loader {
            anchors.fill: parent
            active: root.visible && root.video && root.playing
            source: "IdleVideo.qml"
            onLoaded: {
                const v = String(root.bg.video)
                item.source = v.startsWith("/") ? "file://" + encodeURI(v) : v
                item.muted = root.bg.muted !== false
                item.fill = root.bg.fill || "cover"
            }
        }
    }
    MultiEffect {
        anchors.fill: content
        visible: root.blurPx > 0 && (root.pictures || root.video)
        source: visible ? content : null
        autoPaddingEnabled: false
        blurEnabled: true
        blur: 1
        blurMax: Math.max(1, root.blurPx)
        layer.enabled: visible
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.pictures || root.video ? root.dim : 0
    }
}
