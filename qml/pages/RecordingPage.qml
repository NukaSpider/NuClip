import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Flickable {
    id: page
    contentWidth: width
    contentHeight: body.implicitHeight + 56
    clip: true

    readonly property real viewWidth: {
        const dpr = Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
        return Math.max(420, Math.min(640, width / dpr - 120))
    }

    Column {
        id: body
        x: 28
        y: 28
        width: parent.width - 72
        spacing: 16

        Text {
            text: "RECORDING"
            color: theme.text
            font.pixelSize: 28
            font.weight: Font.Light
            font.letterSpacing: 3
            font.family: theme.displayFamily
        }
        Text {
            width: page.viewWidth
            text: profile()
            color: theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            font.family: theme.monoFamily
        }

        Row {
            spacing: 18
            NcButton {
                text: recording.stateText === "Recording" || recording.stateText === "Starting" ? "Stop replay" : "Start replay"
                kind: "primary"
                onClicked: recording.stateText === "Recording" || recording.stateText === "Paused" || recording.stateText === "Starting" ? recording.stop() : recording.startReplay()
            }
            NcButton {
                text: "Session"
                kind: "ghost"
                enabled: recording.stateText === "Idle" || recording.stateText === "Error"
                onClicked: recording.startSession()
            }
            NcButton {
                text: recording.stateText === "Paused" ? "Resume" : "Pause"
                kind: "ghost"
                enabled: recording.stateText === "Recording" || recording.stateText === "Paused"
                onClicked: recording.togglePause()
            }
            NcButton {
                text: "Save " + settings.replaySeconds + "s"
                kind: "ghost"
                enabled: recording.stateText === "Recording" || recording.stateText === "Paused"
                onClicked: recording.saveClip(0)
            }
            NcButton {
                text: "Screenshot"
                kind: "ghost"
                onClicked: recording.screenshot()
            }
        }

        Text {
            width: page.viewWidth
            visible: recording.error !== "" || recording.warning !== ""
            text: recording.error !== "" ? recording.error : recording.warning
            color: recording.error !== "" ? theme.danger : theme.warning
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            font.family: theme.fontFamily
        }

        Text {
            text: "CLIP LENGTH"
            color: theme.textMuted
            font.pixelSize: 11
            font.letterSpacing: 1.4
            font.family: theme.fontFamily
        }
        Row {
            spacing: 0
            Repeater {
                model: [15, 30, 45, 60, 90, 120]
                delegate: Item {
                    required property int modelData
                    readonly property bool selected: settings.replaySeconds === modelData
                    width: lengthLabel.implicitWidth + 16
                    height: 32
                    Text {
                        id: lengthLabel
                        anchors.centerIn: parent
                        text: modelData + "s"
                        color: selected ? theme.text : theme.textSecondary
                        font.pixelSize: 13
                        font.weight: selected ? Font.Medium : Font.Normal
                        font.family: theme.fontFamily
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: selected ? parent.width - 12 : 0
                        height: 2
                        color: theme.accent
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settings.replaySeconds = modelData
                    }
                }
            }
        }
        Row {
            spacing: 8
            NcTextField {
                id: customSeconds
                width: 72
                numeric: true
                text: String(settings.replaySeconds)
                onEditingFinished: settings.replaySeconds = Number(text)
            }
            Text {
                text: "seconds"
                color: theme.textMuted
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 12
                font.family: theme.fontFamily
            }
        }

        Text {
            text: "CAPTURE"
            color: theme.textMuted
            font.pixelSize: 11
            font.letterSpacing: 1.4
            font.family: theme.fontFamily
        }
        Row {
            spacing: 22
            Repeater {
                model: [
                    { id: "auto", label: "Automatic" },
                    { id: "monitor", label: "Selected display" }
                ]
                delegate: Item {
                    required property var modelData
                    readonly property bool selected: settings.captureMode === modelData.id
                    width: captureLabel.implicitWidth + 2
                    height: 28
                    Text {
                        id: captureLabel
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: selected ? theme.text : theme.textSecondary
                        font.pixelSize: 13
                        font.weight: selected ? Font.Medium : Font.Normal
                        font.family: theme.fontFamily
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        width: selected ? captureLabel.implicitWidth : 0
                        height: 2
                        color: theme.accent
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settings.captureMode = modelData.id
                    }
                }
            }
        }
        NcCombo {
            width: Math.min(360, page.viewWidth)
            visible: settings.captureMode === "monitor"
            enabled: recording.stateText === "Idle"
            options: monitorOptions()
            value: String(settings.monitorIndex)
            onPicked: id => settings.monitorIndex = Number(id)
        }
        Item {
            id: cursorRow
            width: page.viewWidth
            height: 40
            Rectangle {
                anchors.fill: parent
                color: cursorMouse.containsMouse || cursorToggle.activeFocus ? theme.controlHover : "transparent"
                Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }
            }
            MouseArea {
                id: cursorMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: settings.showCursor = !settings.showCursor
            }
            Text {
                anchors.left: parent.left
                anchors.right: cursorToggle.left
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "Show cursor"
                color: theme.text
                elide: Text.ElideRight
                font.pixelSize: 13
                font.family: theme.fontFamily
            }
            NcToggle {
                id: cursorToggle
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                checked: settings.showCursor
                accessibleName: "Show cursor"
                rowHovered: cursorMouse.containsMouse
                onToggled: value => settings.showCursor = value
            }
        }
        Item {
            width: page.viewWidth
            height: qualityHead.implicitHeight
            Text {
                id: qualityHead
                text: "QUALITY"
                color: theme.textMuted
                font.pixelSize: 11
                font.letterSpacing: 1.4
                font.family: theme.fontFamily
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: qualityHead.verticalCenter
                text: "Custom"
                color: settings.quality === "custom" ? theme.accent : theme.textSecondary
                font.pixelSize: 12
                font.weight: settings.quality === "custom" ? Font.Medium : Font.Normal
                font.family: theme.fontFamily
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.applyQualityPreset("custom")
                }
            }
        }

        Item {
            id: presets
            width: page.viewWidth
            height: 48
            Row {
                anchors.fill: parent
                spacing: 0
                Repeater {
                    model: [
                        { id: "performance", label: "Performance" },
                        { id: "balanced", label: "Balanced" },
                        { id: "high", label: "High" },
                        { id: "ultra", label: "Ultra" }
                    ]
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: presets.width / 4
                        height: presets.height
                        readonly property bool selected: settings.quality === modelData.id
                        Rectangle {
                            anchors.fill: parent
                            color: selected ? theme.elevated : "transparent"
                            Rectangle {
                                visible: index > 0
                                width: 1
                                height: parent.height
                                color: theme.cardBorder
                            }
                        }
                        Rectangle {
                            z: 2
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: selected ? 2 : 0
                            color: theme.accent
                        }
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: selected ? theme.text : theme.textSecondary
                            font.pixelSize: 13
                            font.weight: selected ? Font.Medium : Font.Normal
                            font.family: theme.fontFamily
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                settings.applyQualityPreset(modelData.id)
                                page.syncBitrate()
                            }
                        }
                    }
                }
            }
            Rectangle {
                z: 1
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: theme.cardBorder
            }
        }

        Item { width: 1; height: 6 }

        Text {
            text: "FRAME RATE"
            color: theme.textMuted
            font.pixelSize: 11
            font.letterSpacing: 1.4
            font.family: theme.fontFamily
        }
        Row {
            spacing: 0
            Repeater {
                model: [30, 60, 90, 120, 144, 165, 240]
                delegate: Item {
                    required property int modelData
                    readonly property bool allowed: devices.fpsSupported(modelData, page.captureIndex())
                    readonly property bool selected: settings.fps === modelData
                    width: rateLabel.implicitWidth + 18
                    height: 32
                    Text {
                        id: rateLabel
                        anchors.centerIn: parent
                        text: modelData
                        color: !allowed ? theme.textMuted : (selected ? theme.text : theme.textSecondary)
                        font.pixelSize: 13
                        font.weight: selected ? Font.Medium : Font.Normal
                        font.family: theme.fontFamily
                        opacity: allowed ? 1 : 0.4
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: selected ? parent.width - 14 : 0
                        height: 2
                        color: theme.accent
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: allowed ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (!allowed)
                                return
                            settings.fps = modelData
                            page.syncBitrate()
                        }
                    }
                }
            }
        }
        Text {
            width: page.viewWidth
            visible: devices.supportedFps(settings.fps, page.captureIndex()) !== settings.fps
            text: devices.monitorRefresh(page.captureIndex()) > 0
                  ? ("This display refreshes at " + devices.monitorRefresh(page.captureIndex())
                     + " Hz, so " + settings.fps + " fps is unavailable. Recording uses "
                     + devices.supportedFps(settings.fps, page.captureIndex()) + " fps.")
                  : ("A higher frame rate was not reported for this display. Recording uses "
                     + devices.supportedFps(settings.fps, page.captureIndex()) + " fps.")
            color: theme.textSecondary
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            font.family: theme.fontFamily
        }
        Item { width: 1; height: 6 }

        Text {
            text: "CODEC"
            color: theme.textMuted
            font.pixelSize: 11
            font.letterSpacing: 1.4
            font.family: theme.fontFamily
        }
        Row {
            spacing: 18
            Repeater {
                model: codecChoices()
                delegate: Item {
                    required property var modelData
                    readonly property bool selected: settings.codec === modelData.id
                    width: codecLabel.implicitWidth + 4
                    height: 28
                    Text {
                        id: codecLabel
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: selected ? theme.text : theme.textSecondary
                        font.pixelSize: 13
                        font.weight: selected ? Font.Medium : Font.Normal
                        font.family: theme.fontFamily
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        width: selected ? codecLabel.implicitWidth : 0
                        height: 2
                        color: theme.accent
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            settings.codec = modelData.id
                            page.syncBitrate()
                        }
                    }
                }
            }
        }
        Item { width: 1; height: 6 }

        Text {
            text: "ESTIMATED STORAGE"
            color: theme.textMuted
            font.pixelSize: 11
            font.letterSpacing: 1.4
            font.family: theme.fontFamily
        }
        Text {
            text: storageMinute() + "   ·   " + storageHour()
            color: theme.text
            font.pixelSize: 15
            font.family: theme.displayFamily
        }
        Column {
            width: page.viewWidth
            visible: settings.quality === "custom"
            spacing: 8
            Item { width: 1; height: 4 }
            Rectangle { width: parent.width; height: 1; color: theme.cardBorder }
            Item { width: 1; height: 4 }
            Text {
                text: "BITRATE"
                color: theme.textMuted
                font.pixelSize: 11
                font.letterSpacing: 1.4
                font.family: theme.fontFamily
            }
            Row {
                spacing: 8
                NcTextField {
                    id: bitrateField
                    width: 96
                    text: (settings.videoBitrateKbps / 1000).toFixed(1)
                    onEditingFinished: page.commitBitrate(text)
                }
                Text {
                    text: "Mbps"
                    color: theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: theme.fontFamily
                }
            }
            Text {
                text: "ENCODER"
                color: theme.textMuted
                font.pixelSize: 11
                font.letterSpacing: 1.4
                font.family: theme.fontFamily
            }
            NcCombo {
                width: Math.min(320, page.viewWidth)
                enabled: !devices.probing
                options: devices.encoders
                value: settings.encoder
                placeholder: devices.probing ? "Detecting encoders…" : "Encoder"
                onPicked: id => page.pickEncoder(id)
            }
        }

        Text {
            visible: recording.details !== ""
            text: "ENCODER LOG"
            color: theme.textMuted
            font.pixelSize: 11
            font.letterSpacing: 1.4
            font.family: theme.fontFamily
        }
        Text {
            width: page.viewWidth
            visible: recording.details !== ""
            text: recording.details.slice(-800)
            color: theme.textMuted
            font.pixelSize: 11
            font.family: theme.monoFamily
            wrapMode: Text.Wrap
        }
    }

    Connections {
        target: devices
        function onMonitorsChanged() { page.syncBitrate() }
        function onProbeFinished() {
            if (!devices.codecSupported(settings.encoder, settings.codec))
                settings.codec = "h264"
            page.syncBitrate()
        }
    }
    Connections {
        target: detector
        function onChanged() { page.syncBitrate() }
    }
    Connections {
        target: settings
        function onCaptureModeChanged() { page.syncBitrate() }
        function onMonitorIndexChanged() { page.syncBitrate() }
        function onQualityChanged() { page.syncBitrate() }
    }

    Component.onCompleted: syncBitrate()

    function captureIndex() {
        const count = devices.monitors.length
        if (settings.captureMode === "auto" && detector.live)
            return detector.monitorIndex
        if (count === 0)
            return 0
        return settings.monitorIndex
    }
    function captureMonitor() {
        const monitors = devices.monitors
        const index = captureIndex()
        for (let i = 0; i < monitors.length; ++i) {
            if (monitors[i].index === index)
                return monitors[i]
        }
        return monitors.length > 0 ? monitors[0] : null
    }
    function syncBitrate() {
        const monitor = captureMonitor()
        const width = monitor && monitor.width ? monitor.width : 0
        const height = monitor && monitor.height ? monitor.height : 0
        settings.applyDerivedBitrate(width, height)
    }
    function profile() {
        const monitor = captureMonitor()
        const size = monitor && monitor.width > 0 ? (monitor.width + "×" + monitor.height) : "Native"
        const fps = devices.supportedFps(settings.fps, captureIndex())
        const encoder = devices.probing ? "Detecting encoder…" : devices.encoderLabel(devices.resolveEncoder(settings.encoder, settings.codec))
        const app = detector.name
        return app + " · " + settings.qualityLabel + " · " + size + " · " + fps + " fps · " + encoder
    }
    function codecChoices() {
        const probing = devices.probing
        const encoderCount = devices.encoders.length
        const choices = devices.codecsFor(settings.encoder)
        if (!probing)
            return choices
        const quiet = []
        for (let i = 0; i < choices.length; ++i) {
            const item = choices[i]
            quiet.push({ id: item.id, label: item.label })
        }
        if (quiet.length > 0 || encoderCount < 0)
            return quiet
        return [{ id: "h264", label: "H.264" }]
    }
    function videoBitrateKbps() {
        const monitor = captureMonitor()
        const width = monitor && monitor.width ? monitor.width : 0
        const height = monitor && monitor.height ? monitor.height : 0
        const fps = devices.supportedFps(settings.fps, captureIndex())
        if (settings.quality === "custom")
            return settings.videoBitrateKbps
        return settings.recommendedVideoBitrate(width, height, fps, settings.codec, settings.quality)
    }
    function totalBitrateKbps() {
        const audio = settings.quality === "custom" ? settings.audioBitrateKbps : settings.audioBitrateFor(settings.quality)
        return videoBitrateKbps() + audio
    }
    function formatStorage(bytes) {
        if (bytes >= 1e9) {
            const gb = bytes / 1e9
            return gb.toFixed(1) + " GB"
        }
        const mb = bytes / 1e6
        return (mb >= 10 ? String(Math.round(mb)) : mb.toFixed(1)) + " MB"
    }
    function storageMinute() {
        return formatStorage(totalBitrateKbps() * 1000 / 8 * 60) + " / minute"
    }
    function storageHour() {
        return formatStorage(totalBitrateKbps() * 1000 / 8 * 3600) + " / hour"
    }
    function commitBitrate(raw) {
        const mbps = Number(raw)
        if (!isFinite(mbps))
            return
        const clamped = Math.max(0.5, Math.min(150, mbps))
        settings.editCustomBitrate(Math.round(clamped * 1000))
    }
    function pickEncoder(id) {
        settings.encoder = id
        if (!devices.codecSupported(id, settings.codec))
            settings.codec = "h264"
    }
    function monitorOptions() {
        const list = []
        const monitors = devices.monitors
        for (let i = 0; i < monitors.length; ++i)
            list.push({ id: String(monitors[i].index), label: monitors[i].label })
        return list
    }
}
