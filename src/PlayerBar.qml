import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

// Plays a saved episode. Rewind's episodes run for hours, so it remembers
// where each one stopped and picks up there next time.
Rectangle {
    id: bar
    property string path: ""
    property string title: ""
    property string showName: ""
    property string thumbnail: ""
    property string alias: ""
    readonly property bool loaded: path !== ""
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    property real pendingSeek: 0

    implicitHeight: 72
    color: "#16161a"
    radius: theme.radius
    border.color: "#26262c"

    // Starts a file, or pauses and resumes it if it's the one loaded.
    function play(file, episodeTitle, episodeShow, episodeThumbnail, episodeAlias) {
        if (file === bar.path) {
            toggle()
            return
        }
        remember()
        bar.path = file
        bar.title = episodeTitle
        bar.showName = episodeShow
        bar.thumbnail = episodeThumbnail || ""
        bar.alias = episodeAlias || ""
        bar.pendingSeek = backend.resumePosition(file)
        player.source = backend.fileUrl(file)
        player.play()
    }

    function toggle() {
        if (!bar.loaded)
            return
        if (bar.playing) {
            player.pause()
            remember()
        } else {
            player.play()
        }
    }

    function seekBy(milliseconds) {
        if (bar.loaded && player.seekable)
            player.setPosition(Math.max(0, Math.min(player.duration, player.position + milliseconds)))
    }

    function stop() {
        remember()
        player.stop()
        player.source = ""
        bar.path = ""
        bar.alias = ""
    }

    function remember() {
        if (bar.loaded && player.duration > 0)
            backend.savePosition(bar.path, player.position)
    }

    function clock(milliseconds) {
        const total = Math.max(0, Math.floor(milliseconds / 1000))
        const h = Math.floor(total / 3600), m = Math.floor(total % 3600 / 60), s = total % 60
        const two = (n) => (n < 10 ? "0" : "") + n
        return h > 0 ? h + ":" + two(m) + ":" + two(s) : m + ":" + two(s)
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput {}
        onMediaStatusChanged: {
            if ((mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)
                    && bar.pendingSeek > 0) {
                // Resuming within the last half minute would only replay the ending.
                if (bar.pendingSeek < duration - 30000)
                    setPosition(bar.pendingSeek)
                bar.pendingSeek = 0
            }
            if (mediaStatus === MediaPlayer.EndOfMedia)
                backend.forgetPosition(bar.path)
        }
    }

    // Saved now and then while playing, so a crash or power cut costs little.
    Timer {
        interval: 10000
        repeat: true
        running: bar.playing
        onTriggered: bar.remember()
    }

    Component.onDestruction: remember()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            radius: Math.min(4, theme.radius)
            color: "#26262c"
            clip: true
            Image {
                anchors.fill: parent
                source: bar.thumbnail
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(96, 96)
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 220
            Layout.maximumWidth: 260
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: bar.title
                elide: Text.ElideRight
                font.pixelSize: 14
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 12
                color: player.error !== MediaPlayer.NoError ? "#ff8a80" : "#8a8a93"
                text: player.error !== MediaPlayer.NoError ? qsTr("Can't play this file: %1").arg(player.errorString)
                                                           : bar.showName
            }
        }

        Button {
            flat: true
            text: qsTr("−30s")
            font.pixelSize: 12
            enabled: player.seekable
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Back 30 seconds (←)")
            onClicked: bar.seekBy(-30000)
        }

        // Play and pause, drawn rather than taken from a font that might
        // not have the glyphs.
        AccentButton {
            id: playButton
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            text: ""
            Accessible.name: bar.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: bar.toggle()
            Item {
                anchors.centerIn: parent
                width: 14
                height: 16
                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    visible: bar.playing
                    Rectangle { width: 4; height: 16; color: theme.accentForeground }
                    Rectangle { width: 4; height: 16; color: theme.accentForeground }
                }
                Canvas {
                    id: triangle
                    anchors.fill: parent
                    visible: !bar.playing
                    property color fill: theme.accentForeground
                    onFillChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = fill
                        ctx.beginPath()
                        ctx.moveTo(2, 0)
                        ctx.lineTo(width, height / 2)
                        ctx.lineTo(2, height)
                        ctx.closePath()
                        ctx.fill()
                    }
                }
            }
        }

        Button {
            flat: true
            text: qsTr("+30s")
            font.pixelSize: 12
            enabled: player.seekable
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Forward 30 seconds (→)")
            onClicked: bar.seekBy(30000)
        }

        Label {
            text: bar.clock(seek.pressed ? seek.value : player.position)
            font.pixelSize: 12
            font.features: { "tnum": 1 }
            color: "#8a8a93"
        }
        Slider {
            id: seek
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, player.duration)
            enabled: player.seekable
            onMoved: player.setPosition(value)
            // Follows playback unless the user is dragging it.
            Binding on value {
                when: !seek.pressed
                value: player.position
                restoreMode: Binding.RestoreNone
            }
        }
        Label {
            text: bar.clock(player.duration)
            font.pixelSize: 12
            font.features: { "tnum": 1 }
            color: "#8a8a93"
        }

        Button {
            flat: true
            text: "✕"
            Layout.preferredWidth: 40
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Stop and close the player")
            onClicked: bar.stop()
        }
    }
}
