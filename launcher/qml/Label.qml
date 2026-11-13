import QtQuick

// Text in one of the configured roles: display, title, body, supporting
// (serif italic), clock, value, code (mono), idleClock (mono), idleDate (serif italic).
Text {
    property string role: "body"
    readonly property bool monospace: role === "clock" || role === "value" || role === "code" || role === "idleClock"

    font.family: monospace ? Theme.mono : Theme.serif
    font.pixelSize: Theme.font(role)
    font.italic: role === "supporting" || role === "idleDate"
    color: Theme.text
    textFormat: Text.PlainText
    elide: Text.ElideRight
}
