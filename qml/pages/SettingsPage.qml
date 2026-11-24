import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: page

    readonly property var shell: ApplicationWindow.window
    readonly property real shown: {
        const dpr = Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
        return Math.max(560, Math.min(width - 72, width / dpr - 36))
    }

    component SettingLine: Item {
        id: line
        property string label: ""
        property bool checked: false
        signal apply(bool value)
        width: parent ? parent.width : 480
        height: 44
        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: 1
            color: rowMouse.containsMouse || toggle.activeFocus ? theme.controlHover : "transparent"
            Behavior on color { ColorAnimation { duration: 140; easing.type: Easing.OutCubic } }
        }
        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: line.apply(!line.checked)
        }
        Text {
            anchors.left: parent.left
            anchors.right: toggle.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: line.label
            color: theme.text
            elide: Text.ElideRight
            font.pixelSize: 13
            font.family: theme.fontFamily
        }
        NcToggle {
            id: toggle
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            checked: line.checked
            accessibleName: line.label
            rowHovered: rowMouse.containsMouse
            onToggled: value => line.apply(value)
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: theme.cardBorder
        }
    }

    component TextAction: Text {
        property bool active: true
        color: active ? theme.text : theme.textMuted
        font.pixelSize: 12
        font.family: theme.fontFamily
        MouseArea {
            anchors.fill: parent
            anchors.margins: -6
            enabled: parent.active
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: parent.clicked()
        }
        signal clicked()
    }

    component HotkeyRow: Item {
        id: row
        property string label: ""
        property string action: ""
        property string sequence: ""
        readonly property bool listening: hotkeys.listening && hotkeys.listeningAction === row.action
        readonly property bool conflict: row.listening && hotkeys.pendingConflict
        width: parent ? parent.width : 520
        height: row.errorText !== "" ? 64 : 46

        readonly property string errorText: {
            const map = hotkeys.errors
            return map && map[row.action] ? String(map[row.action]) : ""
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: theme.cardBorder
        }
        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: row.errorText !== "" ? -8 : 0
            text: row.label
            color: theme.text
            font.pixelSize: 13
            font.family: theme.fontFamily
        }
        TextAction {
            id: clearAction
            anchors.right: parent.right
            anchors.verticalCenter: keyFace.verticalCenter
            text: row.listening ? "Cancel" : "Clear"
            active: row.listening || row.sequence !== ""
            onClicked: row.listening ? hotkeys.cancelListen() : hotkeys.clearAction(row.action)
        }
        TextAction {
            id: extraAction
            anchors.right: clearAction.left
            anchors.rightMargin: 16
            anchors.verticalCenter: keyFace.verticalCenter
            visible: row.listening && (row.conflict || hotkeys.listeningText.indexOf("…") >= 0)
            text: row.conflict ? "Replace" : "Assign"
            color: theme.accent
            onClicked: row.conflict ? hotkeys.commitPending(true) : hotkeys.commitModifiers()
        }
        TextAction {
            id: changeAction
            anchors.right: extraAction.visible ? extraAction.left : clearAction.left
            anchors.rightMargin: 16
            anchors.verticalCenter: keyFace.verticalCenter
            text: row.listening ? "Listening" : "Change"
            color: row.listening ? theme.accent : theme.text
            onClicked: hotkeys.beginListen(row.action)
        }
        Text {
            id: keyFace
            anchors.right: changeAction.left
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: row.errorText !== "" ? -8 : 0
            text: row.listening ? hotkeys.listeningText : (row.sequence === "" ? "None" : hotkeys.display(row.sequence))
            color: row.listening ? theme.accent : theme.textSecondary
            font.pixelSize: 12
            font.family: theme.monoFamily
        }
        Text {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
            visible: row.errorText !== ""
            text: row.errorText
            color: theme.danger
            font.pixelSize: 11
            font.family: theme.fontFamily
        }
    }

    component Choice: Item {
        id: choice
        property string label: ""
        property bool selected: false
        signal picked()
        width: choiceLabel.implicitWidth + 4
        height: 28
        Accessible.role: Accessible.Button
        Accessible.name: label
        Accessible.onPressAction: choice.picked()
        Text {
            id: choiceLabel
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: choice.label
            color: choice.selected ? theme.text : theme.textSecondary
            font.pixelSize: 13
            font.weight: choice.selected ? Font.Medium : Font.Normal
            font.family: theme.fontFamily
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: choice.selected ? choiceLabel.implicitWidth : 0
            height: 2
            color: theme.accent
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: choice.picked()
        }
    }

    Row {
        x: 36
        y: 28
        width: page.shown
        height: parent.height - 48
        spacing: 36

        Column {
            width: 128
            spacing: 0
            Repeater {
                model: [
                    { id: "general", label: "General" },
                    { id: "recording", label: "Recording" },
                    { id: "audio", label: "Audio" },
                    { id: "hotkeys", label: "Hotkeys" },
                    { id: "appearance", label: "Appearance" },
                    { id: "notify", label: "Notify" },
                    { id: "storage", label: "Storage" },
                    { id: "export", label: "Export" },
                    { id: "integrations", label: "Integrations" }
                ]
                delegate: Item {
                    required property var modelData
                    width: 128
                    height: 34
                    readonly property bool selected: shell.settingsCategory === modelData.id
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label.toUpperCase()
                        color: selected ? theme.text : (catArea.containsMouse ? theme.textSecondary : theme.textMuted)
                        font.pixelSize: 11
                        font.letterSpacing: 1.2
                        font.family: theme.fontFamily
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: selected ? 18 : 0
                        height: 1
                        color: theme.accent
                    }
                    MouseArea {
                        id: catArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: shell.settingsCategory = modelData.id
                    }
                }
            }
        }

        Flickable {
            id: settingsFlick
            width: parent.width - 164
            height: parent.height
            contentHeight: Math.max(height, (stack.currentItem ? stack.currentItem.implicitHeight : 0) + 24)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Connections {
                target: shell
                function onSettingsCategoryChanged() {
                    settingsFlick.contentY = 0
                }
            }

            StackLayout {
                id: stack
                width: parent.width
                currentIndex: shell ? categoryIndex(shell.settingsCategory) : 0

                Column {
                    width: stack.width
                    spacing: 10
                    Text { text: "General"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.letterSpacing: 0.4; font.family: theme.displayFamily }
                    Text {
                        width: parent.width
                        text: "Startup and the tray. Sounds and the overlay live under Notify."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item { width: 1; height: 8 }
                    SettingLine { label: "Launch on Windows startup"; checked: settings.launchOnStartup; onApply: value => settings.launchOnStartup = value }
                    SettingLine { label: "Start replay when NuClip launches"; checked: settings.autoStartReplay; onApply: value => settings.autoStartReplay = value }
                    Text {
                        width: parent.width
                        text: "Closing the window leaves NuClip running in the tray. Quit from the tray icon to exit."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                }

                Column {
                    width: stack.width
                    spacing: 12
                    Text { text: "Recording"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.family: theme.displayFamily }
                    Text {
                        width: parent.width
                        text: "Replay length, quality, frame rate, and the encoder are on the Recording page."
                        wrapMode: Text.WordWrap
                        color: theme.textSecondary
                        font.pixelSize: 13
                        font.family: theme.fontFamily
                    }
                    NcButton { text: "Open recording"; kind: "primary"; onClicked: shell.go(3, "") }
                    Text {
                        width: parent.width
                        text: "Automatic mode uses the monitor showing the foreground app when a recording starts. NuClip does not hook the game."
                        wrapMode: Text.WordWrap
                        color: theme.textMuted
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                }

                Column {
                    width: stack.width
                    spacing: 14
                    Text { text: "Audio"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.family: theme.displayFamily }
                    Text {
                        text: "SYSTEM"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    SettingLine { label: "Record system audio"; checked: settings.systemAudio; onApply: value => settings.systemAudio = value }
                    NcCombo {
                        width: Math.min(420, parent.width)
                        options: devices.outputs
                        value: settings.outputDeviceId
                        onPicked: id => settings.outputDeviceId = id
                    }
                    Row {
                        spacing: 12
                        NcSlider { width: 220; value: settings.systemVolume; onMoved: value => settings.systemVolume = value }
                        Text { text: Math.round(settings.systemVolume * 100) + "%"; color: theme.textSecondary; font.family: theme.monoFamily; anchors.verticalCenter: parent.verticalCenter }
                    }
                    LevelMeter { width: 220; level: recording.systemLevel }
                    Item { width: 1; height: 6 }
                    Text {
                        text: "MICROPHONE"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    SettingLine { label: "Record microphone"; checked: settings.micEnabled; onApply: value => settings.micEnabled = value }
                    NcCombo {
                        width: Math.min(420, parent.width)
                        options: devices.microphones
                        value: settings.micDeviceId
                        onPicked: id => settings.micDeviceId = id
                    }
                    Row {
                        spacing: 12
                        NcSlider { width: 220; value: settings.micVolume; onMoved: value => settings.micVolume = value }
                        Text { text: Math.round(settings.micVolume * 100) + "%"; color: theme.textSecondary; font.family: theme.monoFamily; anchors.verticalCenter: parent.verticalCenter }
                    }
                    LevelMeter { width: 220; level: recording.micLevel }
                    Row {
                        spacing: 8
                        NcTextField {
                            width: 72
                            numeric: true
                            text: String(settings.audioBitrateKbps)
                            onEditingFinished: settings.audioBitrateKbps = Number(text)
                        }
                        Text { text: "kbps"; color: theme.textMuted; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 12; font.family: theme.fontFamily }
                    }
                    Text {
                        width: parent.width
                        text: "Meters move while a recording is running. System audio and the microphone are separate tracks when both are on."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    TextAction { text: "Refresh devices"; onClicked: devices.refresh() }
                }

                Column {
                    width: stack.width
                    spacing: 4
                    Text { text: "Hotkeys"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.family: theme.displayFamily }
                    Text {
                        width: parent.width
                        text: "Shortcuts work while NuClip is in the background. A letter, number, or function key can stand alone. Hold Ctrl, Alt, Shift, or Win, then press the key. Esc cancels."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item { width: 1; height: 8 }
                    HotkeyRow { label: "Save clip"; action: "clip"; sequence: settings.hotkeyClip }
                    HotkeyRow { label: "Save 15 seconds"; action: "clip15"; sequence: settings.hotkeyClip15 }
                    HotkeyRow { label: "Save 30 seconds"; action: "clip30"; sequence: settings.hotkeyClip30 }
                    HotkeyRow { label: "Save 60 seconds"; action: "clip60"; sequence: settings.hotkeyClip60 }
                    HotkeyRow { label: "Save 120 seconds"; action: "clip120"; sequence: settings.hotkeyClip120 }
                    HotkeyRow { label: "Screenshot"; action: "screenshot"; sequence: settings.hotkeyScreenshot }
                    HotkeyRow { label: "Start or stop replay"; action: "toggle"; sequence: settings.hotkeyToggle }
                    HotkeyRow { label: "Pause or resume"; action: "pause"; sequence: settings.hotkeyPause }
                }

                Column {
                    width: stack.width
                    spacing: 14
                    Text { text: "Appearance"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.family: theme.displayFamily }
                    Text {
                        text: "THEME"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    Row {
                        spacing: 18
                        Choice { label: "Dark"; selected: settings.themeMode === "dark"; onPicked: settings.themeMode = "dark" }
                        Choice { label: "Light"; selected: settings.themeMode === "light"; onPicked: settings.themeMode = "light" }
                        Choice { label: "System"; selected: settings.themeMode === "system"; onPicked: settings.themeMode = "system" }
                    }
                    Text {
                        text: "ACCENT"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    Flow {
                        width: parent.width
                        spacing: 10
                        Repeater {
                            model: [
                                { id: "green", color: "#3DDC97" },
                                { id: "blue", color: "#5B9DFF" },
                                { id: "purple", color: "#A78BFA" },
                                { id: "red", color: "#F07178" },
                                { id: "orange", color: "#F5A524" },
                                { id: "pink", color: "#F472B6" },
                                { id: "cyan", color: "#22D3EE" }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                width: 22
                                height: 22
                                radius: 11
                                color: modelData.color
                                border.width: settings.accent === modelData.id ? 2 : 0
                                border.color: theme.text
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: settings.accent = modelData.id
                                }
                            }
                        }
                    }
                    Row {
                        spacing: 12
                        Rectangle {
                            width: 22
                            height: 22
                            radius: 11
                            color: settings.customAccent
                            border.width: settings.accent === "custom" ? 2 : 0
                            border.color: theme.text
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        NcTextField {
                            width: 110
                            text: settings.customAccent
                            onEditingFinished: {
                                settings.customAccent = text
                                settings.accent = "custom"
                            }
                        }
                    }
                    NcSlider {
                        width: Math.min(280, parent.width)
                        from: 0
                        to: 1
                        value: hueOf(settings.customAccent)
                        onMoved: value => {
                            settings.customAccent = hexFromHue(value)
                            settings.accent = "custom"
                        }
                    }
                    Text {
                        text: "Drag the slider or type a hex color."
                        color: theme.textMuted
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                }

                Column {
                    width: stack.width
                    spacing: 10
                    Text { text: "Notifications"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.letterSpacing: 0.4; font.family: theme.displayFamily }
                    Text {
                        width: parent.width
                        text: "Sound and the overlay are separate. Either one can be on without the other."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item { width: 1; height: 8 }
                    Text { text: "SOUND EFFECTS"; color: theme.textMuted; font.pixelSize: 11; font.letterSpacing: 1.2; font.family: theme.fontFamily }
                    SettingLine { label: "Sound effects"; checked: settings.sfxEnabled; onApply: value => settings.sfxEnabled = value }
                    Text {
                        width: parent.width
                        text: "Play audio feedback when NuClip performs capture actions."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item {
                        width: parent.width
                        clip: true
                        height: settings.sfxEnabled ? sfxBody.implicitHeight : 0
                        Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                        Column {
                            id: sfxBody
                            width: parent.width
                            spacing: 10
                            Item { width: 1; height: 4 }
                            Row {
                                width: Math.min(360, parent.width)
                                spacing: 16
                                Text {
                                    width: 92
                                    text: "Volume"
                                    color: theme.text
                                    font.pixelSize: 13
                                    font.family: theme.fontFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                NcSlider {
                                    width: parent.width - 150
                                    from: 0
                                    to: 1
                                    value: settings.sfxVolume
                                    onMoved: value => settings.sfxVolume = Math.round(value * 100) / 100
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    width: 36
                                    text: Math.round(settings.sfxVolume * 100) + "%"
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                    font.family: theme.monoFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            Text { text: "Sound"; color: theme.text; font.pixelSize: 13; font.family: theme.fontFamily }
                            Row {
                                spacing: 12
                                NcCombo {
                                    width: 180
                                    options: tray.sounds
                                    value: settings.sfxSound
                                    onPicked: id => settings.sfxSound = id
                                }
                                NcButton {
                                    text: "Test sound"
                                    kind: "secondary"
                                    onClicked: tray.previewSound()
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            SettingLine { label: "Clip saved"; checked: settings.sfxClip; onApply: value => settings.sfxClip = value }
                            SettingLine { label: "Screenshot captured"; checked: settings.sfxScreenshot; onApply: value => settings.sfxScreenshot = value }
                            SettingLine { label: "Recording started"; checked: settings.sfxRecordStart; onApply: value => settings.sfxRecordStart = value }
                            SettingLine { label: "Recording stopped"; checked: settings.sfxRecordStop; onApply: value => settings.sfxRecordStop = value }
                            SettingLine { label: "Capture error"; checked: settings.sfxError; onApply: value => settings.sfxError = value }
                        }
                    }
                    Item { width: 1; height: 14 }
                    Text { text: "VISUAL OVERLAY"; color: theme.textMuted; font.pixelSize: 11; font.letterSpacing: 1.2; font.family: theme.fontFamily }
                    SettingLine { label: "Visual overlay"; checked: settings.overlayEnabled; onApply: value => settings.overlayEnabled = value }
                    Text {
                        width: parent.width
                        text: "A small card on the display being recorded. It stays above the game, ignores the mouse, and is left out of the capture."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item {
                        width: parent.width
                        clip: true
                        height: settings.overlayEnabled ? overlayBody.implicitHeight : 0
                        Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                        Column {
                            id: overlayBody
                            width: parent.width
                            spacing: 10
                            Item { width: 1; height: 4 }
                            Text { text: "Location"; color: theme.text; font.pixelSize: 13; font.family: theme.fontFamily }
                            Grid {
                                columns: 3
                                columnSpacing: 6
                                rowSpacing: 6
                                Repeater {
                                    model: [
                                        { id: "topLeft", code: "TL" },
                                        { id: "topCenter", code: "TC" },
                                        { id: "topRight", code: "TR" },
                                        { id: "centerLeft", code: "CL" },
                                        { id: "", code: "" },
                                        { id: "centerRight", code: "CR" },
                                        { id: "bottomLeft", code: "BL" },
                                        { id: "bottomCenter", code: "BC" },
                                        { id: "bottomRight", code: "BR" }
                                    ]
                                    delegate: Item {
                                        required property var modelData
                                        readonly property bool chosen: modelData.id !== "" && settings.overlayPosition === modelData.id
                                        width: 36
                                        height: 28
                                        Accessible.role: modelData.id !== "" ? Accessible.Button : Accessible.StaticText
                                        Accessible.name: modelData.code
                                        Accessible.onPressAction: if (modelData.id !== "") settings.overlayPosition = modelData.id
                                        Rectangle {
                                            anchors.fill: parent
                                            visible: modelData.id !== ""
                                            radius: 3
                                            color: chosen ? theme.accent : (spot.containsMouse ? theme.controlHover : theme.control)
                                            border.width: 1
                                            border.color: chosen ? theme.accent : theme.cardBorder
                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.code
                                                color: chosen ? theme.onAccent : theme.textSecondary
                                                font.pixelSize: 9
                                                font.letterSpacing: 0.6
                                                font.family: theme.monoFamily
                                            }
                                            MouseArea {
                                                id: spot
                                                anchors.fill: parent
                                                enabled: modelData.id !== ""
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: settings.overlayPosition = modelData.id
                                            }
                                        }
                                    }
                                }
                            }
                            Text {
                                text: positionName(settings.overlayPosition)
                                color: theme.textSecondary
                                font.pixelSize: 12
                                font.family: theme.fontFamily
                            }
                            Text { text: "Size"; color: theme.text; font.pixelSize: 13; font.family: theme.fontFamily }
                            Row {
                                spacing: 18
                                Choice { label: "Compact"; selected: settings.overlaySize === "compact"; onPicked: settings.overlaySize = "compact" }
                                Choice { label: "Normal"; selected: settings.overlaySize === "normal"; onPicked: settings.overlaySize = "normal" }
                                Choice { label: "Large"; selected: settings.overlaySize === "large"; onPicked: settings.overlaySize = "large" }
                            }
                            Text { text: "Animation"; color: theme.text; font.pixelSize: 13; font.family: theme.fontFamily }
                            Row {
                                spacing: 18
                                Choice { label: "Slide + Fade"; selected: settings.overlayAnimation === "slide"; onPicked: settings.overlayAnimation = "slide" }
                                Choice { label: "Fade"; selected: settings.overlayAnimation === "fade"; onPicked: settings.overlayAnimation = "fade" }
                                Choice { label: "None"; selected: settings.overlayAnimation === "none"; onPicked: settings.overlayAnimation = "none" }
                            }
                            Row {
                                width: Math.min(360, parent.width)
                                spacing: 16
                                Text {
                                    width: 92
                                    text: "Duration"
                                    color: theme.text
                                    font.pixelSize: 13
                                    font.family: theme.fontFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                NcSlider {
                                    width: parent.width - 150
                                    from: 1000
                                    to: 10000
                                    value: settings.overlayDurationMs
                                    onMoved: value => settings.overlayDurationMs = Math.round(value / 100) * 100
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    width: 40
                                    text: (settings.overlayDurationMs / 1000).toFixed(1) + "s"
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                    font.family: theme.monoFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            Row {
                                width: Math.min(360, parent.width)
                                spacing: 16
                                Text {
                                    width: 92
                                    text: "Opacity"
                                    color: theme.text
                                    font.pixelSize: 13
                                    font.family: theme.fontFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                NcSlider {
                                    width: parent.width - 150
                                    from: 0.1
                                    to: 1
                                    value: settings.overlayOpacity
                                    onMoved: value => settings.overlayOpacity = Math.round(value * 100) / 100
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    width: 40
                                    text: Math.round(settings.overlayOpacity * 100) + "%"
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                    font.family: theme.monoFamily
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            SettingLine { label: "Use the accent color"; checked: settings.overlayUseAccent; onApply: value => settings.overlayUseAccent = value }
                            SettingLine { label: "Clip saved"; checked: settings.overlayClip; onApply: value => settings.overlayClip = value }
                            SettingLine { label: "Screenshot captured"; checked: settings.overlayScreenshot; onApply: value => settings.overlayScreenshot = value }
                            SettingLine { label: "Recording started"; checked: settings.overlayRecordStart; onApply: value => settings.overlayRecordStart = value }
                            SettingLine { label: "Recording stopped"; checked: settings.overlayRecordStop; onApply: value => settings.overlayRecordStop = value }
                            SettingLine { label: "Capture error"; checked: settings.overlayError; onApply: value => settings.overlayError = value }
                            Item { width: 1; height: 6 }
                            Text { text: "PREVIEW"; color: theme.textMuted; font.pixelSize: 11; font.letterSpacing: 1.2; font.family: theme.fontFamily }
                            Item {
                                id: stage
                                width: parent.width
                                height: Math.max(120, previewCard.height + 28)
                                property real shown: 1
                                property real travel: 0
                                property string motionWatch: settings.overlayAnimation + settings.overlayPosition
                                function replay() {
                                    if (settings.overlayAnimation === "none") {
                                        shown = 1
                                        travel = 0
                                        return
                                    }
                                    shown = 0
                                    travel = settings.overlayAnimation === "slide" ? 1 : 0
                                    fadeIn.restart()
                                    if (settings.overlayAnimation === "slide")
                                        slideIn.restart()
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 4
                                    color: "#080808"
                                    border.width: 1
                                    border.color: theme.cardBorder
                                }
                                CaptureCard {
                                    id: previewCard
                                    width: implicitWidth
                                    height: implicitHeight
                                    compact: settings.overlaySize === "compact"
                                    large: settings.overlaySize === "large"
                                    accent: settings.overlayUseAccent ? theme.accent : "#3DDC97"
                                    title: "Clip saved"
                                    detail: detector.name + " · 30 seconds"
                                    opacity: stage.shown * settings.overlayOpacity
                                    x: previewX(settings.overlayPosition, width, stage.width) + stage.travel * previewShiftX(settings.overlayPosition)
                                    y: previewY(settings.overlayPosition, height, stage.height) + stage.travel * previewShiftY(settings.overlayPosition)
                                }
                                NumberAnimation {
                                    id: fadeIn
                                    target: stage
                                    property: "shown"
                                    to: 1
                                    duration: 220
                                    easing.type: Easing.OutCubic
                                }
                                NumberAnimation {
                                    id: slideIn
                                    target: stage
                                    property: "travel"
                                    to: 0
                                    duration: 220
                                    easing.type: Easing.OutCubic
                                }
                                property bool motionReady: false
                                Component.onCompleted: motionReady = true
                                onMotionWatchChanged: if (motionReady) replay()
                            }
                            NcButton {
                                text: "Preview Overlay"
                                kind: "secondary"
                                onClicked: overlay.preview()
                            }
                        }
                    }
                }

                Column {
                    width: stack.width
                    spacing: 12
                    Text { text: "Storage"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.family: theme.displayFamily }
                    Text {
                        width: parent.width
                        text: settings.clipsDirectory
                        color: theme.textSecondary
                        wrapMode: Text.Wrap
                        font.pixelSize: 12
                        font.family: theme.monoFamily
                    }
                    Text {
                        text: storage.usedText + " used    ·    " + storage.freeText + " free"
                        color: theme.textMuted
                        font.pixelSize: 12
                        font.family: theme.monoFamily
                    }
                    Row {
                        spacing: 16
                        NcButton { text: "Change folder"; kind: "primary"; onClicked: folderDialog.open() }
                        TextAction { text: "Use Videos\\NuClip\\Clips"; onClicked: settings.saveDirectory = "" }
                    }
                    SettingLine { label: "Organize clips into game folders"; checked: settings.organizeByGame; onApply: value => settings.organizeByGame = value }
                    Row {
                        spacing: 8
                        NcTextField {
                            width: 72
                            numeric: true
                            text: String(settings.storageLimitGb)
                            onEditingFinished: settings.storageLimitGb = Number(text)
                        }
                        Text { text: "GB limit. 0 means no limit."; color: theme.textMuted; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 12; font.family: theme.fontFamily }
                    }
                    SettingLine { label: "Move the oldest clips to the Recycle Bin when over the limit"; checked: settings.autoCleanup; onApply: value => settings.autoCleanup = value }
                }

                Column {
                    width: stack.width
                    spacing: 14
                    Text { text: "Export"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.family: theme.displayFamily }
                    Text {
                        text: "FORMAT"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    NcCombo {
                        width: Math.min(320, parent.width)
                        options: [
                            { id: "mp4", label: "MP4" },
                            { id: "mkv", label: "MKV" },
                            { id: "webm", label: "WebM (VP9, software)" }
                        ]
                        value: settings.exportFormat
                        onPicked: id => settings.exportFormat = id
                    }
                    Text {
                        text: "QUALITY"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    Row {
                        spacing: 18
                        Choice { label: "Low"; selected: settings.exportQuality === "low"; onPicked: settings.exportQuality = "low" }
                        Choice { label: "Medium"; selected: settings.exportQuality === "medium"; onPicked: settings.exportQuality = "medium" }
                        Choice { label: "High"; selected: settings.exportQuality === "high"; onPicked: settings.exportQuality = "high" }
                        Choice { label: "Ultra"; selected: settings.exportQuality === "ultra"; onPicked: settings.exportQuality = "ultra" }
                    }
                    Text {
                        text: "SIZE"
                        color: theme.textMuted
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.family: theme.fontFamily
                    }
                    NcCombo {
                        width: Math.min(320, parent.width)
                        options: [
                            { id: "0", label: "Source resolution" },
                            { id: "720", label: "720p" },
                            { id: "1080", label: "1080p" },
                            { id: "1440", label: "1440p" }
                        ]
                        value: String(settings.exportHeight)
                        onPicked: id => settings.exportHeight = Number(id)
                    }
                    SettingLine { label: "Also compress a Discord copy"; checked: settings.discordCompress; onApply: value => settings.discordCompress = value }
                    Row {
                        spacing: 8
                        NcTextField {
                            width: 72
                            numeric: true
                            text: String(settings.discordTargetMb)
                            onEditingFinished: settings.discordTargetMb = Number(text)
                        }
                        Text { text: "MB target for the Discord copy"; color: theme.textMuted; anchors.verticalCenter: parent.verticalCenter; font.pixelSize: 12; font.family: theme.fontFamily }
                    }
                    Text {
                        width: parent.width
                        text: "The edited export keeps the quality above. The Discord file is a second encode sized to the target. If it is still over after three attempts, NuClip keeps the smallest copy."
                        wrapMode: Text.WordWrap
                        color: theme.textMuted
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                }

                Column {
                    width: stack.width
                    spacing: 10
                    Text { text: "Integrations"; color: theme.text; font.pixelSize: 28; font.weight: Font.Light; font.letterSpacing: 0.4; font.family: theme.displayFamily }
                    Text {
                        width: parent.width
                        text: "Optional connections. Nothing here is sent unless you turn it on."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item { width: 1; height: 8 }
                    Row {
                        spacing: 8
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 2
                            color: discord.connected ? "#5865F2" : (discord.status === "Looking for Discord" || discord.status === "Connecting" || discord.status === "Reconnecting" ? theme.warning : theme.textMuted)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "DISCORD"
                            color: "#5865F2"
                            font.pixelSize: 11
                            font.letterSpacing: 1.4
                            font.family: theme.fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: discord.status
                            color: theme.textSecondary
                            font.pixelSize: 12
                            font.family: theme.fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    SettingLine { label: "Discord Rich Presence"; checked: settings.discordEnabled; onApply: value => settings.discordEnabled = value }
                    Text {
                        width: parent.width
                        text: "Show what NuClip is doing on your Discord profile. File names, paths, and recording contents stay on this PC."
                        color: theme.textMuted
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                    }
                    Item {
                        width: parent.width
                        clip: true
                        height: settings.discordEnabled ? discordBody.implicitHeight : 0
                        Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                        Column {
                            id: discordBody
                            width: parent.width
                            spacing: 10
                            Item { width: 1; height: 4 }
                            SettingLine { label: "Connect when NuClip launches"; checked: settings.discordAutoConnect; onApply: value => settings.discordAutoConnect = value }
                            SettingLine { label: "Show the detected game"; checked: settings.discordShowGame; onApply: value => settings.discordShowGame = value }
                            SettingLine { label: "Show recording status"; checked: settings.discordShowRecording; onApply: value => settings.discordShowRecording = value }
                            SettingLine { label: "Show replay buffer information"; checked: settings.discordShowBuffer; onApply: value => settings.discordShowBuffer = value }
                            SettingLine { label: "Show editing and exporting"; checked: settings.discordShowEditing; onApply: value => settings.discordShowEditing = value }
                            Row {
                                spacing: 12
                                NcButton {
                                    text: discord.connected ? "Disconnect" : "Connect"
                                    kind: "secondary"
                                    onClicked: discord.connected ? discord.disconnectNow() : discord.connectNow()
                                }
                            }
                            Item { width: 1; height: 6 }
                            Text { text: "PREVIEW"; color: theme.textMuted; font.pixelSize: 11; font.letterSpacing: 1.2; font.family: theme.fontFamily }
                            Rectangle {
                                width: Math.min(320, parent.width)
                                height: previewColumn.implicitHeight + 28
                                radius: 8
                                color: "#0A0A0A"
                                border.width: 1
                                border.color: theme.cardBorder
                                Row {
                                    id: previewColumn
                                    x: 14
                                    y: 14
                                    spacing: 12
                                    width: parent.width - 28
                                    Image {
                                        width: 48
                                        height: 48
                                        source: "qrc:/assets/resources/nuclip.svg"
                                        sourceSize.width: 96
                                        sourceSize.height: 96
                                        fillMode: Image.PreserveAspectFit
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                    Column {
                                        width: parent.width - 60
                                        spacing: 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        Text {
                                            width: parent.width
                                            text: "NuClip"
                                            color: "#F2F2F2"
                                            font.pixelSize: 14
                                            font.weight: Font.Medium
                                            font.family: theme.fontFamily
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            width: parent.width
                                            text: discord.previewDetails
                                            color: "#B5BAC1"
                                            font.pixelSize: 13
                                            font.family: theme.fontFamily
                                            wrapMode: Text.WordWrap
                                        }
                                        Text {
                                            width: parent.width
                                            visible: discord.previewState !== ""
                                            text: discord.previewState
                                            color: "#80848E"
                                            font.pixelSize: 12
                                            font.family: theme.fontFamily
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a folder for clips"
        currentFolder: "file:///" + settings.clipsDirectory.replace(/\\/g, "/")
        onAccepted: settings.saveDirectory = folderToPath(selectedFolder)
    }

    function categoryIndex(id) {
        const ids = ["general", "recording", "audio", "hotkeys", "appearance", "notify", "storage", "export", "integrations"]
        const index = ids.indexOf(id)
        return index < 0 ? 0 : index
    }

    function previewX(id, cardWidth, stageWidth) {
        if (String(id).endsWith("Left"))
            return 16
        if (String(id).endsWith("Right"))
            return Math.max(16, stageWidth - cardWidth - 16)
        return Math.max(8, (stageWidth - cardWidth) / 2)
    }

    function previewY(id, cardHeight, stageHeight) {
        const name = String(id)
        if (name.startsWith("top"))
            return 14
        if (name.startsWith("bottom"))
            return Math.max(14, stageHeight - cardHeight - 14)
        return Math.max(8, (stageHeight - cardHeight) / 2)
    }

    function previewShiftX(id) {
        const name = String(id)
        if (name.endsWith("Left"))
            return -10
        if (name.endsWith("Right"))
            return 10
        return 0
    }

    function previewShiftY(id) {
        const name = String(id)
        if (name.startsWith("top"))
            return -8
        if (name.startsWith("bottom"))
            return 8
        return 0
    }

    function positionName(id) {
        const names = {
            "topLeft": "Top left",
            "topCenter": "Top center",
            "topRight": "Top right",
            "centerLeft": "Center left",
            "centerRight": "Center right",
            "bottomLeft": "Bottom left",
            "bottomCenter": "Bottom center",
            "bottomRight": "Bottom right"
        }
        return names[id] ? names[id] : "Top right"
    }

    function hueOf(hex) {
        const color = Qt.color(hex)
        return color.hsvHue < 0 ? 0.38 : color.hsvHue
    }

    function hexByte(channel) {
        const n = Math.round(channel * 255)
        const text = n.toString(16).toUpperCase()
        return n < 16 ? "0" + text : text
    }

    function hexFromHue(hue) {
        const color = Qt.hsva(hue, 0.62, 0.9, 1)
        return "#" + hexByte(color.r) + hexByte(color.g) + hexByte(color.b)
    }

    function folderToPath(url) {
        let text = String(url)
        if (text.startsWith("file:///"))
            text = text.substring(8)
        else if (text.startsWith("file://"))
            text = text.substring(7)
        return decodeURIComponent(text).replace(/\//g, "\\")
    }
}
