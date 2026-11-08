import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One download and how far along it is. States match Download::State.
Item {
    id: row
    property string dateText: ""
    property bool playing: false
    signal play()
    signal cancel()
    signal retry()
    signal reveal()

    readonly property int waiting: 0
    readonly property int fetching: 1
    readonly property int downloading: 2
    readonly property int saving: 3
    readonly property int done: 4
    readonly property int failed: 5
    readonly property int cancelled: 6
    readonly property bool active: model.state <= saving
    readonly property string percent: Math.floor(model.progress * 100).toLocaleString(Qt.locale(), 'f', 0)

    implicitHeight: column.implicitHeight + 22

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 8
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Label {
                    Layout.fillWidth: true
                    text: model.title
                    elide: Text.ElideRight
                    font.pixelSize: 14
                }
                Label {
                    Layout.fillWidth: true
                    text: row.dateText ? model.showName + " · " + row.dateText : model.showName
                    elide: Text.ElideRight
                    color: "#8a8a93"
                    font.pixelSize: 12
                }
            }
            RowButton {
                implicitWidth: 64
                implicitHeight: 28
                font.pixelSize: 12
                vivid: true
                visible: model.state === row.done && model.path !== ""
                text: row.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: row.play()
            }
            RowButton {
                implicitWidth: 64
                implicitHeight: 28
                font.pixelSize: 12
                visible: row.active || model.state === row.failed || model.state === row.cancelled
                         || (model.state === row.done && model.path !== "")
                text: row.active ? qsTr("Cancel")
                    : model.state === row.done ? qsTr("Show")
                    : qsTr("Retry")
                onClicked: row.active ? row.cancel() : model.state === row.done ? row.reveal() : row.retry()
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            visible: row.active && model.state !== row.waiting
            from: 0; to: 1
            value: model.progress
            indeterminate: model.state === row.fetching || (model.state === row.downloading && model.progress <= 0)
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            font.pixelSize: 12
            color: model.state === row.failed ? "#ff8a80" : model.state === row.done ? theme.accent : "#8a8a93"
            text: model.state === row.waiting ? qsTr("Waiting")
                : model.state === row.fetching ? qsTr("Getting episode details")
                : model.state === row.downloading ? qsTr("Downloading audio · %1%").arg(row.percent)
                : model.state === row.saving ? qsTr("Saving · %1%").arg(row.percent)
                : model.state === row.done ? (model.skipped ? qsTr("Already saved") : qsTr("Saved"))
                : model.state === row.failed ? model.error
                : qsTr("Cancelled")
        }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.05) }
}
