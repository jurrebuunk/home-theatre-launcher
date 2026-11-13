import QtQuick

// Recoloured line icon (name from assets/icons) or app icon (appPath).
Image {
    property string name
    property string appPath
    property bool isApp: false
    property color tint: Theme.text
    property int size: Theme.px(32)

    width: size
    height: size
    sourceSize: Qt.size(size, size)
    fillMode: Image.PreserveAspectFit
    smooth: true
    source: isApp ? Theme.appIcon(appPath, tint) : (name ? Theme.icon(name, tint) : "")
}
