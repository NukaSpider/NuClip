import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 20
        anchors.bottomMargin: 16
        spacing: 12
        visible: editor.ready

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: editor.name
                    color: theme.text
                    font.pixelSize: 22
                    font.weight: Font.Light
                    font.family: theme.displayFamily
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: (editor.game !== "" ? editor.game + "    ·    " : "") + editor.resolution + "    ·    " + clock(editor.duration)
                    color: theme.textMuted
                    font.pixelSize: 12
                    font.family: theme.monoFamily
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#000000"
                    radius: 0
                    clip: true
                    VideoOutput {
                        id: videoOut
                        anchors.fill: parent
                        fillMode: VideoOutput.PreserveAspectFit
                        Component.onCompleted: picture.videoOutput = videoOut
                        onVisibleChanged: if (visible) picture.videoOutput = videoOut
                    }
                    Rectangle {
                        anchors.fill: parent
                        color: "#99000000"
                        visible: picture.mediaStatus === MediaPlayer.NoMedia
                                 || (picture.mediaStatus === MediaPlayer.LoadingMedia && !page.placedFrame)
                        Text {
                            anchors.centerIn: parent
                            text: picture.mediaStatus === MediaPlayer.NoMedia ? "No video loaded" : "Loading"
                            color: "#F5F5F5"
                            font.pixelSize: theme.bodySize
                            font.family: theme.fontFamily
                        }
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        visible: picture.error !== MediaPlayer.NoError
                        text: picture.errorString
                        color: theme.danger
                        wrapMode: Text.WordWrap
                        font.pixelSize: theme.labelSize
                        font.family: theme.fontFamily
                    }
                    Rectangle { x: 10; y: 10; width: 14; height: 1; color: "#66FFFFFF" }
                    Rectangle { x: 10; y: 10; width: 1; height: 14; color: "#66FFFFFF" }
                    Rectangle { anchors.right: parent.right; anchors.rightMargin: 10; y: 10; width: 14; height: 1; color: "#66FFFFFF" }
                    Rectangle { anchors.right: parent.right; anchors.rightMargin: 10; y: 10; width: 1; height: 14; color: "#66FFFFFF" }
                    Rectangle { x: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: 14; height: 1; color: "#66FFFFFF" }
                    Rectangle { x: 10; anchors.bottom: parent.bottom; anchors.bottomMargin: 10; width: 1; height: 14; color: "#66FFFFFF" }
                    Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 10; width: 14; height: 1; color: "#66FFFFFF" }
                    Rectangle { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.rightMargin: 10; anchors.bottomMargin: 10; width: 1; height: 14; color: "#66FFFFFF" }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    NcButton {
                        text: picture.playbackState === MediaPlayer.PlayingState ? "Pause" : "Play"
                        kind: "primary"
                        onClicked: {
                            const play = picture.playbackState !== MediaPlayer.PlayingState
                            page.userStarted = play
                            setPlaying(play)
                        }
                    }
                    NcButton { text: "−1"; kind: "ghost"; onClicked: step(-1) }
                    NcButton { text: "+1"; kind: "ghost"; onClicked: step(1) }
                    NcButton { text: "In"; kind: "ghost"; onClicked: { editor.trimIn = picture.position / 1000; previewAt(editor.trimIn) } }
                    NcButton { text: "Out"; kind: "ghost"; onClicked: { editor.trimOut = picture.position / 1000; previewAt(editor.trimOut) } }
                    NcButton { text: "Reset"; kind: "ghost"; onClicked: { editor.trimIn = 0; editor.trimOut = editor.duration; previewAt(0) } }
                    NcButton { text: page.fillPreview ? "Panels" : "Focus"; kind: "ghost"; onClicked: page.fillPreview = !page.fillPreview }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: clock(shownSeconds()) + "  /  " + clock(picture.duration > 0 ? picture.duration / 1000 : editor.duration)
                        color: theme.textSecondary
                        font.pixelSize: 12
                        font.family: theme.monoFamily
                    }
                }

                Timeline {
                    Layout.fillWidth: true
                    duration: picture.duration > 0 ? picture.duration / 1000 : editor.duration
                    position: shownSeconds()
                    trimIn: editor.trimIn
                    trimOut: editor.trimOut > 0 ? editor.trimOut : editor.duration
                    fps: editor.fps
                    waveform: editor.waveformRevision >= 0 ? editor.waveformFor(0) : ""
                    onSeek: seconds => {
                        if (page.scrubbing)
                            requestPreview(seconds * 1000)
                        else
                            seekAll(seconds * 1000)
                    }
                    onTrimInMoved: seconds => {
                        editor.trimIn = seconds
                        requestPreview(seconds * 1000)
                    }
                    onTrimOutMoved: seconds => {
                        editor.trimOut = seconds
                        requestPreview(seconds * 1000)
                    }
                    onDragActive: active => {
                        page.scrubbing = active
                        page.userStarted = false
                        if (picture.playbackState === MediaPlayer.PlayingState)
                            setPlaying(false)
                        if (active) {
                            page.pinFrame = true
                        } else {
                            page.pendingMs = page.latestMs
                            if (!page.seekBusy)
                                issueSeek()
                            finalSeek.restart()
                            pinTimer.restart()
                        }
                    }
                }
            }

            ColumnLayout {
                id: sidePanel
                visible: !page.fillPreview
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                spacing: 10

                Text {
                    text: "AUDIO"
                    color: theme.textMuted
                    font.pixelSize: 11
                    font.letterSpacing: 1.4
                    font.family: theme.fontFamily
                }
                Text {
                    visible: audioRepeater.count === 0
                    text: "This clip has no separate audio tracks."
                    color: theme.textMuted
                    font.pixelSize: theme.labelSize
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    font.family: theme.fontFamily
                }
                Repeater {
                    id: audioRepeater
                    model: editor.tracks
                    delegate: Column {
                        required property int index
                        required property bool enabled
                        required property real volume
                        required property int streamIndex
                        required property string label
                        width: 280
                        spacing: 4
                        Item {
                            width: parent.width
                            height: 36
                            Rectangle {
                                anchors.fill: parent
                                color: trackMouse.containsMouse || trackToggle.activeFocus ? theme.controlHover : "transparent"
                                Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }
                            }
                            MouseArea {
                                id: trackMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: editor.setTrackEnabled(streamIndex, !enabled)
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.right: trackToggle.left
                                anchors.rightMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: label
                                color: theme.text
                                font.pixelSize: theme.labelSize
                                font.family: theme.fontFamily
                                elide: Text.ElideRight
                            }
                            NcToggle {
                                id: trackToggle
                                anchors.right: parent.right
                                anchors.rightMargin: 4
                                anchors.verticalCenter: parent.verticalCenter
                                checked: enabled
                                accessibleName: label
                                rowHovered: trackMouse.containsMouse
                                onToggled: value => editor.setTrackEnabled(streamIndex, value)
                            }
                        }
                        Row {
                            spacing: 8
                            NcSlider {
                                width: 190
                                from: 0
                                to: 2
                                value: volume
                                onMoved: value => editor.setTrackVolume(streamIndex, value)
                            }
                            Text {
                                text: Math.round(volume * 100) + "%"
                                color: theme.textMuted
                                font.pixelSize: theme.smallSize
                                font.family: theme.fontFamily
                            }
                        }
                        Image {
                            width: 260
                            height: 28
                            source: editor.waveformRevision >= 0 ? editor.waveformFor(streamIndex) : ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            visible: source !== ""
                            cache: true
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: "Preview plays each track at its own level. Levels above 100% are kept for export."
                    color: theme.textMuted
                    wrapMode: Text.WordWrap
                    font.pixelSize: theme.smallSize
                    font.family: theme.fontFamily
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: theme.cardBorder }
                Text {
                    text: "EXPORT"
                    color: theme.textMuted
                    font.pixelSize: 11
                    font.letterSpacing: 1.4
                    font.family: theme.fontFamily
                }
                Text {
                    text: settings.exportFormat.toUpperCase() + "    ·    " + settings.exportQuality
                    color: theme.textSecondary
                    font.pixelSize: 12
                    font.family: theme.monoFamily
                }
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    Rectangle {
                        anchors.fill: parent
                        color: discordMouse.containsMouse || discordToggle.activeFocus ? theme.controlHover : "transparent"
                        Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }
                    }
                    MouseArea {
                        id: discordMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settings.discordCompress = !settings.discordCompress
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.right: discordToggle.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Discord copy, " + settings.discordTargetMb + " MB"
                        color: theme.text
                        elide: Text.ElideRight
                        font.pixelSize: theme.labelSize
                        font.family: theme.fontFamily
                    }
                    NcToggle {
                        id: discordToggle
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        checked: settings.discordCompress
                        accessibleName: "Discord copy"
                        rowHovered: discordMouse.containsMouse
                        onToggled: value => settings.discordCompress = value
                    }
                }
                NcButton {
                    text: exporter.running ? "Exporting…" : "Save edited copy"
                    kind: "primary"
                    enabled: editor.ready && !exporter.running && editor.trimOut > editor.trimIn
                    onClicked: editor.exportCurrent()
                }
                NcButton {
                    visible: exporter.running
                    text: "Cancel export"
                    kind: "danger"
                    onClicked: exporter.cancel()
                }
                Rectangle {
                    visible: exporter.running || exporter.progress > 0
                    width: 280
                    height: 4
                    radius: 2
                    color: theme.control
                    Rectangle {
                        width: parent.width * Math.max(0, Math.min(1, exporter.progress))
                        height: parent.height
                        radius: 2
                        color: theme.accent
                    }
                }
                Text {
                    width: 280
                    text: exporter.status + (exporter.eta !== "" ? "  ·  " + exporter.eta : "")
                    color: theme.textSecondary
                    wrapMode: Text.WordWrap
                    font.pixelSize: theme.labelSize
                    font.family: theme.fontFamily
                }
                Text {
                    visible: exporter.outputSize !== ""
                    width: 280
                    text: "Edited file " + exporter.outputSize + (exporter.discordSize !== "" ? "  ·  Discord " + exporter.discordSize : "")
                    color: theme.textMuted
                    wrapMode: Text.WordWrap
                    font.pixelSize: theme.smallSize
                    font.family: theme.fontFamily
                }
                Text {
                    visible: exporter.error !== ""
                    width: 280
                    text: exporter.error
                    color: theme.danger
                    wrapMode: Text.WordWrap
                    font.pixelSize: theme.labelSize
                    font.family: theme.fontFamily
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 10
        visible: !editor.ready
        Text {
            text: "EDITOR"
            color: theme.text
            font.pixelSize: 28
            font.weight: Font.Light
            font.letterSpacing: 3
            font.family: theme.displayFamily
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: "Open a clip from the library. The original file is left in place."
            color: theme.textMuted
            font.pixelSize: 13
            font.family: theme.fontFamily
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    MediaPlayer {
        id: picture
        audioOutput: AudioOutput { muted: true; volume: 0 }
        source: editor.mediaUrl
        videoOutput: videoOut
        onPositionChanged: {
            const end = (editor.trimOut > 0 ? editor.trimOut : editor.duration) * 1000
            const at = picture.position
            if (picture.playbackState === MediaPlayer.PlayingState && end > 0 && at >= end)
                seekAll(editor.trimIn * 1000)
            else if (!page.scrubbing && !page.seekBusy && page.pinFrame && picture.playbackState !== MediaPlayer.PlayingState && Math.abs(at - page.latestMs) > 120)
                picture.position = page.latestMs
        }
        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia) {
                videoOutput = videoOut
                if (!page.placedFrame && !page.scrubbing && !page.pinFrame && playbackState !== MediaPlayer.PlayingState) {
                    page.placedFrame = true
                    position = editor.trimIn * 1000
                }
            }
        }
        onErrorOccurred: (error, text) => page.playbackError = text
    }

    Repeater {
        id: voices
        model: editor.tracks
        delegate: Item {
            id: voice
            required property int index
            required property bool enabled
            required property real volume
            property alias position: player.position
            function play() { player.play() }
            function pause() { player.pause() }
            MediaPlayer {
                id: player
                audioOutput: AudioOutput {
                    muted: !enabled
                    volume: Math.max(0, Math.min(1, volume))
                }
                source: editor.mediaUrl
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.LoadedMedia || mediaStatus === MediaPlayer.BufferedMedia)
                        activeAudioTrack = index
                }
            }
        }
    }

    property string playbackError: ""
    property bool scrubbing: false
    property bool fillPreview: false
    property bool pinFrame: false
    property real latestMs: 0
    property real pendingMs: 0
    property real issuedMs: -1
    property real issuedAt: 0
    property bool seekBusy: false

    Timer {
        id: seekWatch
        interval: 110
        onTriggered: finishSeek()
    }
    Timer {
        id: finalSeek
        interval: 80
        onTriggered: {
            showFrame(page.latestMs)
            for (let i = 0; i < voices.count; ++i) {
                const voice = voices.itemAt(i)
                if (voice)
                    voice.position = page.latestMs
            }
        }
    }
    Timer {
        id: pinTimer
        interval: 280
        onTriggered: page.pinFrame = false
    }

    function shownSeconds() {
        return page.scrubbing || page.pinFrame ? page.latestMs / 1000 : picture.position / 1000
    }

    function requestPreview(ms) {
        const target = Math.max(0, ms)
        page.latestMs = target
        page.pendingMs = target
        const stale = page.seekBusy && Date.now() - page.issuedAt >= 50 && Math.abs(target - page.issuedMs) > 40
        if (!page.seekBusy || stale)
            issueSeek()
    }

    function issueSeek() {
        const target = page.pendingMs
        page.seekBusy = true
        page.issuedMs = target
        page.latestMs = target
        page.issuedAt = Date.now()
        if (picture.playbackState === MediaPlayer.PlayingState)
            picture.pause()
        picture.position = target
        seekWatch.interval = 50
        seekWatch.restart()
    }

    function finishSeek() {
        if (!page.seekBusy)
            return
        const arrived = Math.abs(picture.position - page.issuedMs) <= 40
        if (!arrived && Date.now() - page.issuedAt < 70) {
            seekWatch.interval = 20
            seekWatch.restart()
            return
        }
        page.seekBusy = false
        if (Math.abs(page.pendingMs - page.issuedMs) > 30)
            issueSeek()
    }

    function showFrame(ms) {
        requestPreview(ms)
    }

    function previewAt(seconds) {
        page.latestMs = Math.max(0, seconds * 1000)
        page.userStarted = false
        showFrame(page.latestMs)
    }

    function setPlaying(play) {
        if (play) {
            picture.play()
            for (let i = 0; i < voices.count; ++i) {
                const voice = voices.itemAt(i)
                if (!voice)
                    continue
                voice.position = picture.position
                voice.play()
            }
        } else {
            picture.pause()
            for (let j = 0; j < voices.count; ++j) {
                const voice = voices.itemAt(j)
                if (voice)
                    voice.pause()
            }
        }
    }

    function seekAll(ms) {
        picture.position = ms
        for (let i = 0; i < voices.count; ++i) {
            const voice = voices.itemAt(i)
            if (voice)
                voice.position = ms
        }
    }

    function step(direction) {
        const frame = 1000 / Math.max(1, editor.fps)
        const next = Math.max(0, Math.min(picture.duration || editor.duration * 1000, picture.position + direction * frame))
        seekAll(next)
    }

    function clock(seconds) {
        const total = Math.max(0, Math.round(seconds))
        const m = Math.floor(total / 60)
        const s = total % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    onVisibleChanged: {
        if (visible)
            picture.videoOutput = videoOut
        else
            setPlaying(false)
    }

    Connections {
        target: editor
        function onSourceChanged() {
            page.playbackError = ""
            page.placedFrame = false
            picture.stop()
            picture.videoOutput = videoOut
            picture.source = editor.mediaUrl
            picture.play()
            frameHold.restart()
        }
    }

    Timer {
        id: frameHold
        interval: 400
        onTriggered: {
            if (picture.playbackState === MediaPlayer.PlayingState && !page.userStarted)
                setPlaying(false)
        }
    }

    property bool userStarted: false
    property bool placedFrame: false
}
