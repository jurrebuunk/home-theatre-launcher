import QtQuick

// Text in one of the configured roles: display, title, body, supporting
// (serif italic), clock, value, code (mono).
Text {
    property string role: "body"
    readonly property bool monospace: role === "clock" || role === "value" || role === "code"

    font.family: monospace ? Theme.mono : Theme.serif
    font.pixelSize: Theme.font(role)
    font.italic: role === "supporting"
    color: Theme.text
    textFormat: Text.PlainText
    elide: Text.ElideRight
}
