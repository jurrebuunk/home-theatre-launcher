import QtQuick

// Button prompts along the bottom; entries the current device lacks are hidden.
// An entry may name a second action (b) that shares the label, e.g. volume down / up.
Row {
    property var hints: [{ a: "select", t: "Select" }, { a: "back", t: "Back" }, { a: "home", t: "Home" }]
    spacing: Theme.px(48)
    Repeater {
        model: parent.hints
        Row {
            spacing: Theme.px(16)
            visible: glyph.available
            Glyph {
                id: glyph
                action: modelData.a
                anchors.verticalCenter: parent.verticalCenter
            }
            Glyph {
                visible: modelData.b !== undefined
                action: modelData.b || ""
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
