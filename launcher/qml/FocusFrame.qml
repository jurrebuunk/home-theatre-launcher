import QtQuick

// Square focus outline drawn outside the element so focus never moves layout.
Rectangle {
    property bool shown: parent.activeFocus
    anchors.fill: parent
    anchors.margins: -(Theme.focusOffset + Theme.focusWidth)
    color: "transparent"
    border.color: Theme.focus
    border.width: Theme.focusWidth
    visible: shown
    z: 10
}
