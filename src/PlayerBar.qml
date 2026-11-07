import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

// Plays an episode: from disk when it's saved, streamed from NTS's host
// when it isn't. Episodes run for hours, so it remembers where each one
// stopped, and it follows the tracklist to say what's playing now.
Rectangle {
    id: bar
    property var episode: ({})
    property string key: ""
    property string path: ""          // the saved file, or "" while streaming
    property bool resolving: false
    property string failure: ""
    property bool retried: false
    property real pendingSeek: 0
    property real lastPosition: 0

    readonly property bool loaded: key !== ""
    readonly property bool streaming: loaded && path === ""
    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property var tracks: backend.tracks
    readonly property int trackIndex: {
        // The last track that has started by now; NTS's times are exact for
        // most tracks and estimated for the rest.
        const now = player.position / 1000
        let found = -1
        for (let i = 0; i < tracks.length; ++i)
            if (tracks[i].start >= 0 && tracks[i].start <= now)
                found = i
        return found
    }
    readonly property var track: trackIndex >= 0 ? tracks[trackIndex] : null

    implicitHeight: 72
    color: "#16161a"
    radius: theme.radius
    border.color: "#26262c"

    function label(t) {
        return !t ? "" : t.artist && t.title ? t.artist + " – " + t.title : (t.title || t.artist)
    }

    // Starts an episode, or pauses and resumes it if it's the one loaded.
    function play(e) {
        const key = backend.episodeKey(e)
        if (key === bar.key && bar.failure === "") {
            toggle()
            return
        }
        remember()
        player.stop()
        bar.episode = e
        bar.key = key
        bar.failure = ""
        bar.retried = false
        bar.path = backend.savedPath(e)
        bar.pendingSeek = backend.resumePosition(key)
        backend.loadTracks(e, bar.path)
        if (bar.path !== "") {
            bar.resolving = false
            player.source = backend.fileUrl(bar.path)
            player.play()
        } else {
            bar.resolving = true
            player.source = ""
            backend.resolveStream(e)
        }
    }

    function toggle() {
        if (!bar.loaded || bar.resolving)
            return
        if (bar.playing) {
            player.pause()
            remember()
        } else {
            player.play()
        }
    }

    function seekTo(milliseconds) {
        if (bar.loaded && player.seekable)
            player.setPosition(Math.max(0, Math.min(player.duration, milliseconds)))
    }

    function seekBy(milliseconds) {
        seekTo(player.position + milliseconds)
    }

    function stop() {
        remember()
        player.stop()
        player.source = ""
        bar.key = ""
        bar.path = ""
        bar.resolving = false
        bar.failure = ""
        trackList.close()
        backend.clearTracks()
    }

    function remember() {
        if (bar.loaded && player.duration > 0)
            backend.savePosition(bar.key, player.position)
    }

    function clock(milliseconds) {
        const total = Math.max(0, Math.floor(milliseconds / 1000))
        const h = Math.floor(total / 3600), m = Math.floor(total % 3600 / 60), s = total % 60
        const two = (n) => (n < 10 ? "0" : "") + n
        return h > 0 ? h + ":" + two(m) + ":" + two(s) : m + ":" + two(s)
    }

    Connections {
        target: backend
        function onStreamReady(key, url) {
            if (key !== bar.key)
                return
            bar.resolving = false
            player.source = url
            player.play()
        }
        function onStreamFailed(key, error) {
            if (key !== bar.key)
                return
            bar.resolving = false
            bar.failure = error
        }
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput { objectName: "rewindAudio" }
        onPositionChanged: if (player.position > 0) bar.lastPosition = player.position
        onMediaStatusChanged: {
            if ((mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)
                    && bar.pendingSeek > 0) {
                // Resuming within the last half minute would only replay the ending.
                if (bar.pendingSeek < duration - 30000)
                    setPosition(bar.pendingSeek)
                bar.pendingSeek = 0
            }
            if (mediaStatus === MediaPlayer.EndOfMedia)
                backend.forgetPosition(bar.key)
        }
        onErrorOccurred: (error, message) => {
            // Stream addresses expire after a while; ask for a fresh one once
            // and carry on from the same spot.
            if (bar.streaming && !bar.retried) {
                bar.retried = true
                bar.pendingSeek = bar.lastPosition
                bar.resolving = true
                backend.resolveStream(bar.episode)
                return
            }
            bar.failure = qsTr("Can't play this episode: %1").arg(message)
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
                source: bar.episode.thumbnail || ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(96, 96)
            }
        }

        // The episode, and what's on now.
        ColumnLayout {
            Layout.preferredWidth: 260
            Layout.maximumWidth: 300
            spacing: 2
            Label {
                Layout.fillWidth: true
                text: bar.episode.name || ""
                elide: Text.ElideRight
                font.pixelSize: 14
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: 12
                color: bar.failure !== "" ? "#ff8a80" : bar.track ? theme.accent : "#8a8a93"
                text: bar.failure !== "" ? bar.failure
                    : bar.resolving ? qsTr("Finding the stream…")
                    : bar.track ? bar.label(bar.track)
                    : (bar.episode.showName || "")
                ToolTip.visible: tip.hovered && truncated
                ToolTip.text: text
                HoverHandler { id: tip }
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
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            text: ""
            enabled: !bar.resolving
            Accessible.name: bar.playing ? qsTr("Pause") : qsTr("Play")
            onClicked: bar.failure !== "" ? bar.play(bar.episode) : bar.toggle()
            BusyIndicator {
                anchors.centerIn: parent
                width: 28
                height: 28
                running: bar.resolving
                visible: running
            }
            Item {
                anchors.centerIn: parent
                width: 14
                height: 16
                visible: !bar.resolving
                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    visible: bar.playing
                    Rectangle { width: 4; height: 16; color: theme.accentForeground }
                    Rectangle { width: 4; height: 16; color: theme.accentForeground }
                }
                Canvas {
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
        Label {
            visible: bar.streaming
            text: qsTr("Streaming")
            font.pixelSize: 11
            color: "#8a8a93"
        }

        Button {
            flat: true
            text: qsTr("Tracks")
            font.pixelSize: 12
            enabled: bar.tracks.length > 0
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Show the tracklist (T)")
            onClicked: bar.toggleTracks()
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

    function toggleTracks() {
        if (trackList.visible)
            trackList.close()
        else if (bar.tracks.length > 0)
            trackList.open()
    }

    // The tracklist, rising out of the bar. A track with a start time is a
    // place to jump to.
    Popup {
        id: trackList
        x: parent.width - width
        y: -height - 8
        width: Math.min(440, bar.width)
        height: Math.min(420, list.contentHeight + 56)
        padding: 0
        focus: true
        Material.roundedScale: theme.radius
        background: Rectangle { color: "#16161a"; radius: theme.radius; border.color: "#26262c" }
        onOpened: list.positionViewAtIndex(Math.max(0, bar.trackIndex), ListView.Center)

        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            Label {
                Layout.fillWidth: true
                Layout.margins: 14
                text: backend.tracksTimed ? qsTr("Tracklist · times are NTS's, some estimated")
                                          : qsTr("Tracklist")
                color: "#8a8a93"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: bar.tracks
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    readonly property bool now: index === bar.trackIndex
                    width: list.width
                    height: 40
                    color: now ? Qt.rgba(1, 1, 1, 0.06) : jump.hovered && modelData.start >= 0 ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
                    HoverHandler { id: jump }
                    TapHandler { onTapped: if (modelData.start >= 0) bar.seekTo(modelData.start * 1000) }
                    Rectangle { width: 3; height: parent.height; color: theme.accent; visible: parent.now }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        spacing: 12
                        Label {
                            Layout.preferredWidth: 56
                            text: modelData.start >= 0 ? bar.clock(modelData.start * 1000) : ""
                            font.pixelSize: 12
                            font.features: { "tnum": 1 }
                            color: "#8a8a93"
                        }
                        Label {
                            Layout.fillWidth: true
                            text: bar.label(modelData)
                            elide: Text.ElideRight
                            font.pixelSize: 13
                            color: parent.parent.now ? theme.accent : "#e6e6ea"
                        }
                    }
                }
            }
        }
    }
}
