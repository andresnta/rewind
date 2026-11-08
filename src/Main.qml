import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1040
    height: 700
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: backend.archive.showName ? qsTr("%1 — Rewind").arg(backend.archive.showName) : "Rewind"
    // The theme's background, see-through like the terminals when Omarchy's
    // windows are glassy, so Hyprland's blur shows through.
    readonly property color base: theme.background
    color: theme.glassy ? Qt.rgba(base.r, base.g, base.b, theme.glassOpacity) : base

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    Material.background: theme.background

    // Panels tint whatever is behind them rather than covering it.
    readonly property color panel: Qt.rgba(1, 1, 1, 0.035)
    readonly property color line: Qt.rgba(1, 1, 1, 0.08)
    readonly property color dim: "#8a8a93"
    // Letter shortcuts stand aside while the user is typing.
    readonly property bool typing: linkField.activeFocus || folderField.activeFocus
    readonly property bool overlay: help.visible || settings.visible || quitDialog.visible
    property bool quitting: false
    readonly property int queueWidth: 340
    // Where every link starts, so pasting a show's name is all that's left.
    readonly property string linkPrefix: "nts.live/shows/"
    property string pendingAlias: ""

    function openLink() {
        const parsed = linkField.text.trim()
        if (parsed === "")
            return
        // An episode link highlights that episode once its show has loaded.
        const match = parsed.match(/\/episodes\/([A-Za-z0-9_-]+)/)
        win.pendingAlias = match ? match[1].toLowerCase() : ""
        if (backend.open(parsed)) {
            episodes.forceActiveFocus()
            selectPending()
        }
    }

    function selectPending() {
        if (win.pendingAlias === "")
            return
        const i = backend.archive.indexOf(win.pendingAlias)
        if (i >= 0) {
            episodes.currentIndex = i
            episodes.positionViewAtIndex(i, ListView.Contain)
            win.pendingAlias = ""
        }
    }

    function downloadCurrent() {
        if (episodes.currentIndex < 0 || episodes.count === 0)
            return
        const e = backend.archive.episode(episodes.currentIndex)
        if (e.hasAudio)
            backend.queue.enqueue(e)
    }

    // Plays an episode from the archive, saved or streamed, or pauses it
    // if it's the one playing.
    function playRow(row) {
        const e = backend.archive.episode(row)
        if (e.hasAudio)
            player.play(e)
    }

    // P plays the highlighted episode, or pauses and resumes it once it's
    // the one loaded.
    function playCurrent() {
        if (episodes.count > 0 && episodes.currentIndex >= 0) {
            const e = backend.archive.episode(episodes.currentIndex)
            if (e.hasAudio && backend.episodeKey(e) !== player.key) {
                player.play(e)
                return
            }
        }
        player.toggle()
    }

    function downloadAll() {
        const list = backend.archive.pending()
        for (let i = 0; i < list.length; ++i)
            backend.queue.enqueue(list[i])
    }

    function formatDate(iso) {
        // Reading the language re-renders dates when it switches.
        translator.language
        if (!iso)
            return ""
        const d = new Date(iso + "T12:00:00")
        return isNaN(d) ? iso : d.toLocaleDateString(Qt.locale(), "d MMM yyyy")
    }

    onClosing: (close) => {
        player.remember()
        if (backend.queue.activeCount > 0 && !win.quitting) {
            close.accepted = false
            quitDialog.open()
        }
    }

    function requestQuit() {
        if (backend.queue.activeCount > 0)
            quitDialog.open()
        else
            Qt.quit()
    }

    Component.onCompleted: {
        if (initialLink !== "") {
            linkField.text = initialLink
            openLink()
        } else {
            linkField.text = win.linkPrefix
            linkField.forceActiveFocus()
            linkField.cursorPosition = linkField.text.length
        }
    }

    Connections {
        target: backend.archive
        function onPageLoaded() { win.selectPending() }
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.typing || help.visible; onActivated: help.visible ? help.close() : help.open() }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: win.requestQuit() }
    Shortcut { sequence: "Ctrl+Q"; context: Qt.ApplicationShortcut; onActivated: win.requestQuit() }
    Shortcut { sequence: "Ctrl+L"; context: Qt.ApplicationShortcut; onActivated: { linkField.forceActiveFocus(); linkField.selectAll() } }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: win.downloadCurrent() }
    Shortcut { sequences: ["J", "Down"]; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: episodes.incrementCurrentIndex() }
    Shortcut { sequences: ["K", "Up"]; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: episodes.decrementCurrentIndex() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: win.playCurrent() }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay && player.loaded; onActivated: player.seekBy(-30000) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay && player.loaded; onActivated: player.seekBy(30000) }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: !win.typing && !help.visible && !settings.visible && player.loaded; onActivated: player.toggleTracks() }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: win.downloadAll() }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: backend.archive.loadMore() }
    Shortcut { sequence: "O"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: backend.openFolder() }
    Shortcut { sequence: "X"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: backend.queue.cancelCurrent() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: backend.queue.retryFailed() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: backend.queue.clearFinished() }
    Shortcut { sequence: ","; context: Qt.ApplicationShortcut; enabled: !win.typing && !win.overlay; onActivated: settings.open() }
    Shortcut {
        sequence: "Escape"; context: Qt.ApplicationShortcut
        enabled: linkField.activeFocus
        onActivated: episodes.forceActiveFocus()
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // The link bar, split like the panels below it: the field spans the
            // episode list, the buttons sit over the downloads.
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                TextField {
                    id: linkField
                    Layout.fillWidth: true
                    placeholderText: qsTr("NTS show or episode link")
                    selectByMouse: true
                    onAccepted: win.openLink()
                }
                RowLayout {
                    Layout.preferredWidth: win.queueWidth
                    Layout.maximumWidth: win.queueWidth
                    spacing: 6
                    AccentButton {
                        text: qsTr("Open")
                        enabled: linkField.text.trim() !== "" && linkField.text.trim() !== win.linkPrefix
                        onClicked: win.openLink()
                    }
                    Button {
                        flat: true
                        Layout.fillWidth: true
                        text: backend.displayDirectory
                        font.pixelSize: 13
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Change where episodes are saved (,)")
                        onClicked: settings.open()
                    }
                    Button {
                        flat: true
                        text: "?"
                        Layout.preferredWidth: 44
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Keyboard shortcuts")
                        onClicked: help.open()
                    }
                }
            }

            // Problems are said, not worked around.
            Rectangle {
                Layout.fillWidth: true
                visible: backend.missingTools.length > 0
                implicitHeight: toolsRow.implicitHeight + 20
                radius: Math.min(6, theme.radius)
                color: "#3a1d1d"
                RowLayout {
                    id: toolsRow
                    anchors.fill: parent
                    anchors.margins: 10
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: qsTr("%1 isn't installed, so nothing can be downloaded. Install it with: omarchy-pkg-add %2")
                                .arg(backend.missingTools.join(", ")).arg(backend.missingTools.join(" "))
                    }
                    Button { flat: true; text: qsTr("Check again"); onClicked: backend.checkTools() }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                visible: backend.message !== ""
                implicitHeight: messageRow.implicitHeight + 16
                radius: Math.min(6, theme.radius)
                color: "#26221a"
                RowLayout {
                    id: messageRow
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 4
                    Label { Layout.fillWidth: true; text: backend.message; wrapMode: Text.WordWrap }
                    Button { flat: true; text: "✕"; Layout.preferredWidth: 40; onClicked: backend.clearMessage() }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // The show's archive.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: win.panel
                    radius: theme.radius
                    border.color: win.line
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        spacing: 0
                        visible: backend.archive.showAlias !== ""

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.margins: 16
                            spacing: 14

                            Rectangle {
                                Layout.preferredWidth: 56
                                Layout.preferredHeight: 56
                                radius: Math.min(4, theme.radius)
                                color: win.line
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    source: backend.archive.thumbnail
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label {
                                    Layout.fillWidth: true
                                    text: backend.archive.showName || backend.archive.showAlias
                                    font.pixelSize: 22
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    color: win.dim
                                    elide: Text.ElideRight
                                    text: {
                                        const parts = []
                                        if (backend.archive.total > 0)
                                            parts.push(qsTr("%n episodes", "", backend.archive.total))
                                        if (backend.archive.location)
                                            parts.push(backend.archive.location)
                                        return parts.join(" · ")
                                    }
                                }
                            }
                            Button {
                                text: qsTr("Download all listed")
                                flat: true
                                enabled: episodes.count > 0 && backend.missingTools.length === 0
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Queue every listed episode that isn't saved yet (A)")
                                onClicked: win.downloadAll()
                            }
                        }

                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: win.line }

                        ListView {
                            id: episodes
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: backend.archive
                            currentIndex: 0
                            keyNavigationEnabled: false
                            highlightMoveDuration: 0
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            focus: true

                            delegate: EpisodeRow {
                                width: episodes.width
                                current: ListView.isCurrentItem
                                dateText: win.formatDate(model.date)
                                onPicked: episodes.currentIndex = index
                                onDownload: backend.queue.enqueue(model.episode)
                                playing: player.playing && player.key === model.episode.show + "/" + model.alias
                                onPlay: { episodes.currentIndex = index; win.playRow(index) }
                            }

                            footer: Item {
                                width: episodes.width
                                height: backend.archive.canLoadMore || backend.archive.loading || backend.archive.error ? 64 : 12
                                BusyIndicator {
                                    anchors.centerIn: parent
                                    running: backend.archive.loading
                                    visible: running
                                }
                                Button {
                                    anchors.centerIn: parent
                                    visible: backend.archive.canLoadMore
                                    flat: true
                                    text: qsTr("Load more (M)")
                                    onClicked: backend.archive.loadMore()
                                }
                                Label {
                                    anchors.centerIn: parent
                                    width: parent.width - 32
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    visible: backend.archive.error !== "" && !backend.archive.loading
                                    text: backend.archive.error
                                    color: "#ff8a80"
                                }
                            }

                            // Scrolling near the end fetches the next page.
                            onAtYEndChanged: if (atYEnd && count > 0) backend.archive.loadMore()
                        }
                    }

                    // Nothing open yet.
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - 64, 420)
                        visible: backend.archive.showAlias === ""
                        spacing: 10
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Paste a link from nts.live")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            color: win.dim
                            text: qsTr("A show link lists its past episodes to pick from. An episode link starts downloading it straight away. Each one is saved in its original quality with its artwork, tags, and tracklist, ready to play here.")
                        }
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            horizontalAlignment: Text.AlignHCenter
                            color: win.dim
                            font.family: "monospace"
                            font.pixelSize: 12
                            text: "nts.live/shows/…"
                        }
                    }
                }

                // The queue.
                Rectangle {
                    Layout.preferredWidth: win.queueWidth
                    Layout.fillHeight: true
                    color: win.panel
                    radius: theme.radius
                    border.color: win.line
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 1
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 16
                            Layout.rightMargin: 8
                            Layout.preferredHeight: 52
                            Label {
                                Layout.fillWidth: true
                                text: backend.queue.activeCount > 0
                                      ? qsTr("Downloads · %1").arg(backend.queue.activeCount.toLocaleString(Qt.locale(), 'f', 0))
                                      : qsTr("Downloads")
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                            }
                            Button {
                                flat: true
                                text: qsTr("Clear")
                                visible: backend.queue.finishedCount > 0
                                Layout.preferredHeight: 40
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Clear finished downloads (C)")
                                onClicked: backend.queue.clearFinished()
                            }
                        }
                        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: win.line }

                        ListView {
                            id: queueList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: backend.queue
                            spacing: 0
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            delegate: QueueRow {
                                width: queueList.width
                                dateText: win.formatDate(model.date)
                                onCancel: backend.queue.cancel(index)
                                onRetry: backend.queue.retry(index)
                                onReveal: backend.queue.reveal(index)
                                playing: player.playing && player.key === model.key
                                onPlay: player.play(model.episode)
                            }

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 48
                                visible: queueList.count === 0
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                color: win.dim
                                text: qsTr("Episodes you pick show up here. Space downloads the highlighted one.")
                            }
                        }
                    }
                }
            }

            PlayerBar {
                id: player
                Layout.fillWidth: true
                visible: loaded
            }
        }
    }

    HelpOverlay { id: help }

    Popup {
        id: settings
        anchors.centerIn: Overlay.overlay
        focus: true
        width: Math.min(520, win.width - 64)
        modal: true
        padding: 20
        Material.roundedScale: theme.radius
        onOpened: { folderField.text = backend.directory; folderField.forceActiveFocus(); folderField.selectAll() }
        onClosed: episodes.forceActiveFocus()

        function save() {
            backend.directory = folderField.text
            settings.close()
        }

        background: Rectangle { color: theme.background; radius: theme.radius; border.color: win.line }

        ColumnLayout {
            anchors.fill: parent
            spacing: 14
            Label { text: qsTr("Save episodes to"); font.pixelSize: 17; font.weight: Font.DemiBold }
            TextField {
                id: folderField
                Layout.fillWidth: true
                selectByMouse: true
                onAccepted: settings.save()
                Keys.onEscapePressed: settings.close()
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: win.dim
                text: qsTr("Each show gets its own folder inside it.")
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { flat: true; text: qsTr("Cancel"); onClicked: settings.close() }
                AccentButton {
                    text: qsTr("Save")
                    onClicked: settings.save()
                }
            }
        }
    }

    Dialog {
        id: quitDialog
        anchors.centerIn: Overlay.overlay
        focus: true
        modal: true
        width: Math.min(460, win.width - 80)
        title: qsTr("Stop downloading?")
        Material.roundedScale: theme.radius
        background: Rectangle { color: theme.background; radius: theme.radius; border.color: win.line }
        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            wrapMode: Text.WordWrap
            text: qsTr("%n episodes are still downloading or waiting. Quitting stops them, and anything unfinished is thrown away.", "", backend.queue.activeCount)
        }
        footer: DialogButtonBox {
            background: null
            Button { text: qsTr("Keep downloading"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            AccentButton {
                text: qsTr("Quit")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: { win.quitting = true; backend.queue.cancelAll(); Qt.quit() }
        onClosed: episodes.forceActiveFocus()
    }
}
