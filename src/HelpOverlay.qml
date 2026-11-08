import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?.
Popup {
    id: help
    anchors.centerIn: Overlay.overlay
    modal: true
    focus: true
    padding: 24
    onClosed: if (parent && parent.ApplicationWindow.window) parent.ApplicationWindow.window.contentItem.forceActiveFocus()
    Material.roundedScale: theme.radius

    background: Rectangle { color: theme.background; radius: theme.radius; border.color: Qt.rgba(1, 1, 1, 0.08) }

    readonly property var keys: [
        ["Ctrl+L", qsTr("Paste or type a link")],
        ["Enter", qsTr("Open the link")],
        ["J / K", qsTr("Move through episodes")],
        ["Space", qsTr("Download the highlighted episode")],
        ["P", qsTr("Play or pause the highlighted episode")],
        ["← / →", qsTr("Skip back or forward 30 seconds")],
        ["T", qsTr("Show the tracklist")],
        ["A", qsTr("Download every listed episode")],
        ["M", qsTr("Load more episodes")],
        ["X", qsTr("Cancel the running download")],
        ["R", qsTr("Retry failed downloads")],
        ["C", qsTr("Clear finished downloads")],
        ["O", qsTr("Open the download folder")],
        [",", qsTr("Change the download folder")],
        ["Esc", qsTr("Leave the link field")],
        ["?", qsTr("Show or hide shortcuts")],
        ["Q", qsTr("Quit")]
    ]

    ColumnLayout {
        spacing: 14
        Label { text: qsTr("Keyboard shortcuts"); font.pixelSize: 18; font.weight: Font.DemiBold }
        GridLayout {
            columns: 2
            columnSpacing: 24
            rowSpacing: 8
            Repeater {
                model: help.keys.length * 2
                Label {
                    readonly property var entry: help.keys[Math.floor(index / 2)]
                    text: index % 2 === 0 ? entry[0] : entry[1]
                    font.family: index % 2 === 0 ? "monospace" : Qt.application.font.family
                    color: index % 2 === 0 ? theme.accent : "#e6e6ea"
                    Layout.alignment: Qt.AlignLeft
                }
            }
        }
    }
}
