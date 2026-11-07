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
    signal picked()
    signal download()
    signal play()

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
    TapHandler { onTapped: row.picked(); onDoubleTapped: model.status === "done" ? row.play() : model.hasAudio && model.status === "" ? row.download() : undefined }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 12
        spacing: 14

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: Math.min(4, theme.radius)
            color: "#26262c"
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

        // What's happened to it, or a way to start.
        Label {
            visible: model.status !== "" || !model.hasAudio
            color: model.status === "failed" ? "#ff8a80" : model.status === "done" ? theme.accent : "#8a8a93"
            font.pixelSize: 12
            text: !model.hasAudio ? qsTr("No audio")
                : row.playing ? qsTr("Playing")
                : model.status === "done" ? qsTr("Saved")
                : model.status === "downloading" ? qsTr("Downloading")
                : model.status === "waiting" ? qsTr("Queued")
                : model.status === "failed" ? qsTr("Failed") : ""
        }
        Button {
            visible: model.status === "done"
            flat: true
            text: row.playing ? qsTr("Pause") : qsTr("Play")
            font.pixelSize: 13
            onClicked: row.play()
        }
        Button {
            visible: model.hasAudio && (model.status === "" || model.status === "failed")
            flat: true
            text: model.status === "failed" ? qsTr("Retry") : qsTr("Download")
            font.pixelSize: 13
            onClicked: { row.picked(); row.download() }
        }
    }

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#1f1f24" }
}
