import QtQuick

// Joined square buttons; `current` is outlined, focus fills with accent.
Row {
    id: root
    property var options: []      // [{ value, text, icon? }]
    property string current
    property bool followFocus: false // choose as soon as focus moves (tabs)
    property real optionWidth: 0
    property alias firstItem: repeater.firstItem
    signal chosen(string value)

    function itemAt(i) { return repeater.itemAt(i) }

    Repeater {
        id: repeater
        property Item firstItem: count > 0 ? itemAt(0) : null
        model: root.options
        Item {
            id: option
            activeFocusOnTab: true
            width: root.optionWidth > 0 ? root.optionWidth : Math.max(Theme.px(270), content.implicitWidth + Theme.px(80))
            onActiveFocusChanged: if (activeFocus && root.followFocus && !isCurrent) root.chosen(modelData.value)
            height: Theme.px(Theme.layout.buttonHeight)
            readonly property bool isCurrent: modelData.value === root.current
            KeyNavigation.left: index > 0 ? repeater.itemAt(index - 1) : null
            KeyNavigation.right: index < repeater.count - 1 ? repeater.itemAt(index + 1) : null

            Rectangle {
                anchors.fill: parent
                color: option.activeFocus ? Theme.accent : option.isCurrent ? Theme.surfaceAlt : Theme.surface
                border.color: option.isCurrent && !option.activeFocus ? Theme.focus : Theme.border
                border.width: option.activeFocus ? 0 : Theme.borderWidth
            }
            FocusFrame {}
            Row {
                id: content
                anchors.centerIn: parent
                spacing: Theme.px(16)
                Icon {
                    visible: !!modelData.icon
                    anchors.verticalCenter: parent.verticalCenter
                    name: modelData.icon || ""
                    size: Theme.px(28)
                    tint: option.activeFocus ? Theme.background : Theme.accent
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.text
                    color: option.activeFocus ? Theme.background : option.isCurrent ? Theme.text : Theme.muted
                }
            }
            Keys.onPressed: (event) => {
                if (Theme.isSelect(event.key)) {
                    event.accepted = true
                    root.chosen(modelData.value)
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: { option.forceActiveFocus(); root.chosen(modelData.value) }
            }
        }
    }
}
