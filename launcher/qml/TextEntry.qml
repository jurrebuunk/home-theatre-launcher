import QtQuick

// Full-screen text entry with an on-screen keyboard for controllers and
// remotes: A types the highlighted key, X deletes, Menu submits, B cancels.
// A physical keyboard types directly.
FocusScope {
    id: root
    property string title
    property string iconName: "lock"
    property bool secret: false
    property string text
    property string submitText: "Connect"
    property string mode: "lower"        // lower, upper, symbols, symbols2
    property int maxLength: 64
    property string error
    property string hint
    // Host names / IP addresses: '.' and ':' instead of '-' and '_'.
    property bool address: false
    signal submitted(string text)
    signal cancelled()

    property bool reveal: false
    property int row: 2
    property int col: 7
    property bool onEye: false

    function focusDefault() { cursor.forceActiveFocus() }

    readonly property real unit: Theme.px(88)
    readonly property real gap: Theme.px(12)
    function keyWidth(w) { return w * unit + (w - 1) * gap }

    function chars(s, w) { return s.split("").map(c => ({ k: "char", t: c, w: w || 1 })) }
    readonly property var rows: {
        const sym = mode === "symbols" || mode === "symbols2"
        const upper = mode === "upper"
        const r = []
        r.push(chars("1234567890"))
        if (sym) {
            r.push(chars("!@#$%^&*()"))
            r.push(chars("-_=+[]{}<>"))
            r.push([{ k: "shift", t: mode === "symbols2" ? "1/2" : "2/2", w: 1.5 }]
                   .concat(chars(mode === "symbols2" ? "\"\\|~`€£" : ".,?/:;'"))
                   .concat([{ k: "bksp", w: 1.5 }]))
        } else {
            const l = s => upper ? s.toUpperCase() : s
            r.push(chars(l("qwertyuiop")))
            r.push(chars(l("asdfghjkl") + (address ? (upper ? ":" : ".") : (upper ? "_" : "-"))))
            r.push([{ k: "shift", w: 1.5 }].concat(chars(l("zxcvbnm"))).concat([{ k: "bksp", w: 1.5 }]))
        }
        r.push([{ k: "mode", t: sym ? "abc" : "#+=", w: 2 }, { k: "space", w: 5 }, { k: "submit", t: submitText, w: 3 }])
        return r
    }

    function keyX(r, c) {
        let x = 0
        for (let i = 0; i < c; ++i)
            x += keyWidth(rows[r][i].w) + gap
        return x
    }
    function moveVertical(d) {
        if (onEye) {
            if (d > 0) { onEye = false; row = 0; col = rows[0].length - 1 }
            return
        }
        const target = row + d
        if (target < 0) {
            if (secret) onEye = true
            return
        }
        if (target >= rows.length)
            return
        const center = keyX(row, col) + keyWidth(rows[row][col].w) / 2
        let best = 0, bestDist = 1e9
        for (let i = 0; i < rows[target].length; ++i) {
            const c = keyX(target, i) + keyWidth(rows[target][i].w) / 2
            if (Math.abs(c - center) < bestDist) { bestDist = Math.abs(c - center); best = i }
        }
        row = target
        col = best
    }
    function moveHorizontal(d) {
        if (onEye)
            return
        col = Math.max(0, Math.min(rows[row].length - 1, col + d))
    }
    function insert(s) {
        if (text.length + s.length > maxLength)
            return
        text += s
        error = ""
        if (mode === "upper")
            mode = "lower" // shift is one-shot
    }
    function backspace() { text = text.substring(0, text.length - 1); error = "" }
    function press(key) {
        switch (key.k) {
        case "char": insert(key.t); break
        case "space": insert(" "); break
        case "bksp": backspace(); break
        case "shift":
            mode = mode === "lower" ? "upper" : mode === "upper" ? "lower" : mode === "symbols" ? "symbols2" : "symbols"
            break
        case "mode": mode = (mode === "symbols" || mode === "symbols2") ? "lower" : "symbols"; break
        case "submit": root.submitted(text); break
        }
    }
    onRowsChanged: col = Math.min(col, rows[row].length - 1)

    Rectangle { anchors.fill: parent; color: Theme.background }
    MouseArea { anchors.fill: parent }

    Item {
        id: cursor
        focus: true
        Keys.onPressed: (event) => {
            const k = event.key
            const ctrl = event.modifiers & Qt.ControlModifier
            event.accepted = true
            if (k === Qt.Key_Up) root.moveVertical(-1)
            else if (k === Qt.Key_Down) root.moveVertical(1)
            else if (k === Qt.Key_Left) root.moveHorizontal(-1)
            else if (k === Qt.Key_Right) root.moveHorizontal(1)
            else if (k === Qt.Key_Backspace || k === Qt.Key_Delete) root.backspace()
            else if (k === Qt.Key_Menu || (ctrl && (k === Qt.Key_Return || k === Qt.Key_Enter))) root.submitted(root.text)
            else if (k === Qt.Key_Escape || k === Qt.Key_Back) root.cancelled()
            else if (event.text.length === 1 && event.text.charCodeAt(0) >= 32 && event.text.charCodeAt(0) !== 127)
                root.insert(event.text)
            else if (Theme.isSelect(k)) {
                if (root.onEye) root.reveal = !root.reveal
                else root.press(root.rows[root.row][root.col])
            } else if (Theme.isBack(k)) root.cancelled()
            else event.accepted = false
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical) + Theme.px(16)
        spacing: Theme.px(44)

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.px(36)
            Icon { name: root.iconName; size: Theme.px(52); tint: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
            Label { role: "display"; text: root.title; anchors.verticalCenter: parent.verticalCenter }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            Rectangle {
                width: root.keyWidth(10) - (root.secret ? Theme.px(88) : 0)
                height: Theme.px(88)
                color: Theme.surface
                border.color: Theme.border
                border.width: Theme.borderWidth
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.px(36)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.px(2)
                    Label {
                        id: field
                        anchors.verticalCenter: parent.verticalCenter
                        role: "value"
                        font.pixelSize: Theme.px(32)
                        font.letterSpacing: root.secret && !root.reveal ? Theme.px(8) : 0
                        text: root.secret && !root.reveal ? "•".repeat(root.text.length) : root.text
                        width: Math.min(implicitWidth, root.keyWidth(10) - Theme.px(200))
                        elide: Text.ElideLeft
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(2, Theme.px(3))
                        height: Theme.px(40)
                        color: Theme.accent
                    }
                }
            }
            KeyButton {
                visible: root.secret
                width: Theme.px(88)
                height: Theme.px(88)
                iconName: root.reveal ? "eye-off" : "eye"
                highlighted: root.onEye && cursor.activeFocus
                onClicked: root.reveal = !root.reveal
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: root.gap
            Repeater {
                model: root.rows
                Row {
                    id: keyRow
                    readonly property int rowIndex: index
                    spacing: root.gap
                    Repeater {
                        model: modelData
                        KeyButton {
                            width: root.keyWidth(modelData.w)
                            height: Theme.px(86)
                            text: modelData.k === "submit" ? root.submitText : (modelData.t || "")
                            iconName: modelData.k === "shift" && !modelData.t ? "shift"
                                    : modelData.k === "bksp" ? "backspace"
                                    : modelData.k === "space" ? "space" : ""
                            primary: modelData.k === "submit"
                            highlighted: !root.onEye && cursor.activeFocus && root.row === keyRow.rowIndex && root.col === index
                            border.color: modelData.k === "shift" && root.mode === "upper" && !highlighted ? Theme.focus
                                        : primary && !highlighted ? Theme.accent : Theme.border
                            onClicked: { root.row = keyRow.rowIndex; root.col = index; root.onEye = false; root.press(modelData) }
                        }
                    }
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.error !== "" || root.hint !== ""
            role: "supporting"
            color: root.error !== "" ? Theme.error : Theme.muted
            text: root.error || root.hint
        }
    }

    Separator {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        anchors.bottom: hints.top
        anchors.bottomMargin: Theme.px(28)
    }
    HintBar {
        id: hints
        anchors.left: parent.left
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical)
        hints: [{ a: "select", t: "Type" }, { a: "delete", t: "Delete" }, { a: "menu", t: root.submitText },
                { a: "back", t: "Cancel" }]
    }
}
