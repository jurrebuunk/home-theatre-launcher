import QtQuick

Row {
    spacing: Theme.px(48)
    Repeater {
        model: [{ a: "select", t: "Select" }, { a: "back", t: "Back" }, { a: "home", t: "Home" }]
        Row {
            spacing: Theme.px(16)
            Glyph {
                action: modelData.a
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: modelData.t
                color: Theme.muted
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
