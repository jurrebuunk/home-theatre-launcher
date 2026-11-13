import QtQuick

// Read-only Bluetooth + network icons (idle screen).
Row {
    property int size: Theme.px(Theme.layout.statusIconSize)
    spacing: Theme.px(32)
    Icon {
        visible: Bt.hasAdapter && Bt.powered
        name: "bluetooth"
        size: parent.size
        tint: Theme.muted
    }
    Icon {
        name: Theme.networkIcon()
        size: parent.size
        tint: Net.state === "offline" ? Theme.warning : Theme.muted
    }
}
