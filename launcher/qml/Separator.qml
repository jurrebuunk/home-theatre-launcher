import QtQuick

Rectangle {
    property bool vertical: false
    implicitHeight: vertical ? 0 : Math.max(1, Theme.borderWidth)
    implicitWidth: vertical ? Math.max(1, Theme.borderWidth) : 0
    color: Theme.border
}
