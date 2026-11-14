import QtQuick

// Status row + one simple grid of app tiles. Nothing else.
FocusScope {
    id: root
    signal openBluetooth()
    signal openNetwork()
    signal openSound()
    signal openMoonlight(int index)

    // Configured columns, reduced to what fits the safe area (never overflow).
    readonly property int fittingColumns: Math.max(1, Math.floor((area.width + Theme.px(Theme.layout.gap))
                                                       / (Theme.px(Theme.layout.tile.width) + Theme.px(Theme.layout.gap))))
    readonly property int columns: Math.max(1, Math.min(Theme.layout.columns, Cfg.apps.length, fittingColumns))
    readonly property int count: Cfg.apps.length

    function focusTile(i) {
        const item = tiles.itemAt(Math.max(0, Math.min(count - 1, i)))
        if (item)
            item.forceActiveFocus()
        else
            status.bluetoothItem.forceActiveFocus()
    }

    function move(i, dx, dy) {
        const col = i % columns
        if (dx < 0 && col > 0) focusTile(i - 1)
        else if (dx > 0 && col < columns - 1 && i + 1 < count) focusTile(i + 1)
        else if (dy < 0) {
            if (i - columns >= 0) focusTile(i - columns)
            else status.bluetoothItem.forceActiveFocus()
        } else if (dy > 0 && i + columns < count) focusTile(i + columns)
        else if (dy > 0 && Math.floor(i / columns) < Math.floor((count - 1) / columns)) focusTile(count - 1)
    }

    StatusRow {
        id: status
        anchors.top: parent.top
        anchors.topMargin: Theme.px(Theme.layout.safeMarginVertical)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        onOpenBluetooth: root.openBluetooth()
        onOpenNetwork: root.openNetwork()
        onOpenSound: root.openSound()
        onDownPressed: root.focusTile(Life.lastIndex)
    }

    Item {
        id: area
        anchors.top: status.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.px(Theme.layout.safeMargin)
        anchors.rightMargin: Theme.px(Theme.layout.safeMargin)
        anchors.bottomMargin: Theme.px(Theme.layout.safeMarginVertical) + status.height

        Grid {
            id: grid
            columns: root.columns
            spacing: Theme.px(Theme.layout.gap)
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.layout.alignment === "left" ? 0 : (parent.width - width) / 2

            Repeater {
                id: tiles
                model: Cfg.apps
                AppTile {
                    app: modelData
                    index: model.index
                    // A Moonlight tile opens its library; streams start from there.
                    onActivated: app.type === "moonlight" ? root.openMoonlight(index) : Life.launch(app.id)
                    Keys.onLeftPressed: root.move(index, -1, 0)
                    Keys.onRightPressed: root.move(index, 1, 0)
                    Keys.onUpPressed: root.move(index, 0, -1)
                    Keys.onDownPressed: root.move(index, 0, 1)
                }
            }
        }

        Label {
            visible: root.count === 0
            anchors.centerIn: parent
            role: "supporting"
            color: Theme.muted
            text: "No apps configured"
        }
    }
}
