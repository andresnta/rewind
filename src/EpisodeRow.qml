import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One past episode in a show's archive.
Rectangle {
    id: row
    property bool current: false
    property string dateText: ""
    property bool playing: false
    // Room at the right edge, more of it while the list's scroll bar shows.
    property real endMargin: 12
    signal picked()
    signal download()
    signal play()
    signal cancel()

    implicitHeight: 64
    color: current ? Qt.rgba(1, 1, 1, 0.06) : hover.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"

    // The highlighted row is marked in the accent, so the keyboard's place
    // is visible at a glance.
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: theme.accent
        visible: row.current
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: row.picked(); onDoubleTapped: if (model.hasAudio) row.play() }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: row.endMargin
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: Math.min(4, theme.radius)
            color: Qt.rgba(1, 1, 1, 0.06)
            clip: true
            Image {
                anchors.fill: parent
                source: model.thumbnail
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(80, 80)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: model.name
                elide: Text.ElideRight
                font.pixelSize: 15
                opacity: model.hasAudio ? 1 : 0.5
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: "#8a8a93"
                font.pixelSize: 12
                text: [row.dateText].concat(model.genres.slice(0, 3)).join(" · ")
            }
        }

        // Two fixed slots, so every row's buttons line up: play, then
        // download or what's happened to it. A saved episode's Play is filled.
        RowButton {
            Layout.preferredWidth: 84
            visible: model.hasAudio
            vivid: model.status === "done"
            text: row.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: row.play()
        }
        Item {
            Layout.preferredWidth: 96
            Layout.preferredHeight: 32
            RowButton {
                anchors.fill: parent
                visible: model.hasAudio && (model.status === "" || model.status === "failed")
                text: model.status === "failed" ? qsTr("Retry") : qsTr("Download")
                onClicked: { row.picked(); row.download() }
            }
            // A download in progress is its own progress bar; clicking it
            // cancels, as hovering says.
            RowButton {
                anchors.fill: parent
                visible: model.status === "waiting" || model.status === "downloading"
                fill: model.status === "downloading" ? Math.max(0, model.progress) : -1
                text: hovered ? qsTr("Cancel")
                    : model.status === "waiting" ? qsTr("Queued")
                    : qsTr("%1%").arg(Math.floor(Math.max(0, model.progress) * 100).toLocaleString(Qt.locale(), 'f', 0))
                onClicked: row.cancel()
            }
            Label {
                anchors.centerIn: parent
                visible: !model.hasAudio || model.status === "done"
                font.pixelSize: 12
                color: model.status === "done" ? theme.accent : "#8a8a93"
                text: !model.hasAudio ? qsTr("No audio") : qsTr("Saved")
            }
        }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.05) }
}
