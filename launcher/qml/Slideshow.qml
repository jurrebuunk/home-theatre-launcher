import QtQuick

// One image or a slideshow: the next picture loads off-screen and fades in
// over the current one once it is decoded.
Item {
    id: root
    property var sources: []
    property int interval: 30000
    property int fade: 1500
    property string fill: "cover"
    property int index: 0
    property bool running: visible
    property Item front: a
    property Item back: b

    function next() {
        if (sources.length < 2)
            return
        index = (index + 1) % sources.length
        back.source = sources[index]
    }
    function reveal(img) {
        if (img !== back || img.status !== Image.Ready)
            return
        const old = front
        img.z = 1
        old.z = 0
        front = img
        back = old
        fadeIn.target = img
        fadeIn.restart()
    }
    onSourcesChanged: {
        index = 0
        const first = sources.length > 0 ? sources[0] : ""
        if (first !== "" && front.status === Image.Ready && fade > 0) {
            // Something is showing: fade the new picture in over it.
            if (String(front.source) === first)
                return
            if (String(back.source) === first && back.status === Image.Ready)
                reveal(back)
            else
                back.source = first
            return
        }
        fadeIn.stop()
        a.opacity = 1; a.z = 1; b.opacity = 0; b.z = 0
        front = a; back = b
        a.source = first
        b.source = ""
    }

    component Picture: Image {
        anchors.fill: parent
        asynchronous: true
        cache: false
        autoTransform: true
        smooth: true
        fillMode: root.fill === "contain" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        sourceSize: Qt.size(root.width, root.height)
        onStatusChanged: root.reveal(this)
    }
    Picture { id: a }
    Picture { id: b; opacity: 0 }

    NumberAnimation {
        id: fadeIn
        property: "opacity"
        from: 0
        to: 1
        duration: root.fade
        onFinished: root.back.opacity = 0
    }
    Timer {
        interval: root.interval
        repeat: true
        running: root.running && root.sources.length > 1
        onTriggered: root.next()
    }
}
