import QtQuick
import QtMultimedia

// Looping background video; loaded only while the idle screen is shown.
Item {
    id: root
    property url source
    property bool muted: true
    property string fill: "cover"

    MediaPlayer {
        id: player
        source: root.source
        loops: MediaPlayer.Infinite
        videoOutput: output
        audioOutput: root.muted ? null : audio
        Component.onCompleted: play()
        onErrorOccurred: (error, message) => console.warn("idle video:", message)
    }
    AudioOutput { id: audio }
    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: root.fill === "contain" ? VideoOutput.PreserveAspectFit : VideoOutput.PreserveAspectCrop
    }
}
