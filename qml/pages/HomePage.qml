import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    clip: true

    property bool selecting: false
    property var picked: ({})
    readonly property int pickCount: Object.keys(picked).length
    property var pendingDelete: []

    function nativeSize() {
        const monitors = devices.monitors
        const index = settings.captureMode === "auto" && detector.live ? detector.monitorIndex : settings.monitorIndex
        for (let i = 0; i < monitors.length; ++i) {
            if (monitors[i].index === index && monitors[i].width > 0)
                return monitors[i].width + "×" + monitors[i].height
        }
        return "Native"
    }
    function isPicked(id) {
        return picked[id] === true
    }

    function togglePick(id) {
        const next = Object.assign({}, picked)
        if (next[id])
            delete next[id]
        else
            next[id] = true
        picked = next
    }

    function closeAppRename(save) {
        if (!appNameEdit.visible)
            return
        const next = appNameEdit.text.trim()
        appNameEdit.visible = false
        if (save && next.length > 0 && next !== detector.name)
            detector.setCustomName(next)
    }

    function askRename(id, name) {
        renamePopup.renameId = id
        renameField.text = name
        renamePopup.open()
    }

    function askDelete(ids) {
        pendingDelete = ids
        confirmDelete.open()
    }

    function statusLabel() {
        const state = recording.stateText
        if (recording.booting && state === "Idle")
            return "Initializing"
        if (state === "Starting")
            return "Starting replay"
        if (state === "Error")
            return "Replay unavailable"
        if (state === "Paused")
            return "Replay paused"
        if (state === "Stopping")
            return "Stopping"
        if (state === "Recording" || state === "Saving clip")
            return recording.mode === "session" ? "Session" : "Replay"
        return "Standby"
    }

    function live() {
        return recording.stateText === "Recording" || recording.stateText === "Paused" || recording.stateText === "Saving clip"
    }

    Column {
        id: column
        anchors.fill: parent
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 28
        anchors.bottomMargin: 22
        spacing: 0

        Item {
            width: parent.width
            height: 148

            Rectangle { x: 0; y: 0; width: 16; height: 1; color: theme.textMuted }
            Rectangle { x: 0; y: 0; width: 1; height: 16; color: theme.textMuted }
            Rectangle { anchors.right: parent.right; y: 0; width: 16; height: 1; color: theme.textMuted }
            Rectangle { anchors.right: parent.right; y: 0; width: 1; height: 16; color: theme.textMuted }
            Rectangle { x: 0; anchors.bottom: parent.bottom; width: 16; height: 1; color: theme.textMuted }
            Rectangle { x: 0; anchors.bottom: parent.bottom; width: 1; height: 16; color: theme.textMuted }
            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 16
                height: 1
                color: page.live() ? theme.accent : theme.textMuted
                SequentialAnimation on opacity {
                    running: page.live()
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 0.3; duration: 800 }
                    NumberAnimation { from: 0.3; to: 1; duration: 800 }
                }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 1
                height: 16
                color: page.live() ? theme.accent : theme.textMuted
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 10

                RowLayout {
                    width: parent.width
                    spacing: 18
                    Text {
                        text: page.statusLabel().toUpperCase()
                        color: theme.text
                        font.pixelSize: 34
                        font.weight: Font.Light
                        font.letterSpacing: 3
                        font.family: theme.displayFamily
                    }
                    Text {
                        text: page.live() ? "ACTIVE" : (recording.stateText === "Error" ? "CHECK" : "")
                        color: theme.accent
                        font.pixelSize: 12
                        font.letterSpacing: 2
                        font.weight: Font.DemiBold
                        font.family: theme.fontFamily
                        Layout.alignment: Qt.AlignBottom
                        Layout.bottomMargin: 8
                    }
                    Item { Layout.fillWidth: true }
                    Column {
                        spacing: 6
                        Layout.alignment: Qt.AlignVCenter
                        Text {
                            anchors.right: parent.right
                            text: recording.bufferSeconds + "s buffer"
                            color: theme.textSecondary
                            font.pixelSize: 12
                            font.family: theme.monoFamily
                        }
                        Rectangle {
                            width: 120
                            height: 2
                            color: theme.cardBorder
                            Rectangle {
                                width: parent.width * (page.live() ? Math.max(0, Math.min(1, recording.elapsed / Math.max(1, recording.bufferSeconds))) : 0)
                                height: 2
                                color: theme.accent
                            }
                        }
                    }
                }

                Row {
                    spacing: 8
                    width: parent.width
                    Image {
                        source: detector.icon
                        width: 16
                        height: 16
                        visible: detector.icon !== ""
                        fillMode: Image.PreserveAspectFit
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    NcIcon {
                        visible: detector.icon === ""
                        name: detector.name === "Desktop" ? "monitor" : "game"
                        size: 14
                        iconColor: theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        visible: !appNameEdit.visible
                        text: detector.name
                        color: theme.text
                        font.pixelSize: 13
                        font.family: theme.fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        visible: detector.name !== "Desktop" && !appNameEdit.visible
                        text: "Rename"
                        color: theme.textMuted
                        font.pixelSize: 12
                        font.family: theme.fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                appNameEdit.text = detector.name
                                appNameEdit.visible = true
                                appNameEdit.edit()
                            }
                        }
                    }
                    NcTextField {
                        id: appNameEdit
                        visible: false
                        width: 180
                        dismissOnEscape: true
                        onFinished: page.closeAppRename(true)
                        onCancelled: page.closeAppRename(false)
                    }
                }
                Row {
                    spacing: 14
                    width: parent.width
                    Text {
                        text: recording.activeMonitor !== "" ? recording.activeMonitor : (settings.captureMode === "auto" ? "Foreground display" : devices.monitorLabel(settings.monitorIndex))
                        color: theme.textSecondary
                        font.pixelSize: 12
                        font.family: theme.monoFamily
                        width: Math.min(implicitWidth, 280)
                        elide: Text.ElideRight
                    }
                    Text { text: settings.qualityLabel; color: theme.textSecondary; font.pixelSize: 12; font.family: theme.monoFamily }
                    Text { text: nativeSize(); color: theme.textSecondary; font.pixelSize: 12; font.family: theme.monoFamily }
                    Text { text: settings.fps + " fps"; color: theme.textSecondary; font.pixelSize: 12; font.family: theme.monoFamily }
                    Text {
                        text: devices.probing ? "Checking encoders" : devices.encoderLabel(recording.activeEncoder !== "" ? recording.activeEncoder : devices.resolveEncoder(settings.encoder, settings.codec))
                        color: theme.textSecondary
                        font.pixelSize: 12
                        font.family: theme.monoFamily
                    }
                }
                Text {
                    visible: recording.error !== ""
                    width: parent.width
                    text: recording.error
                    color: theme.danger
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    font.family: theme.fontFamily
                }
            }
        }

        Item { width: 1; height: 22 }

        Row {
            spacing: 18
            NcButton {
                text: "Save clip"
                kind: "primary"
                enabled: page.live()
                onClicked: recording.saveClip(0)
            }
            Text {
                text: hotkeys.display(settings.hotkeyClip)
                color: theme.textMuted
                font.pixelSize: 11
                font.family: theme.monoFamily
                anchors.verticalCenter: parent.verticalCenter
            }
            NcButton {
                text: "Screenshot"
                kind: "ghost"
                enabled: page.live()
                onClicked: recording.screenshot()
            }
            Text {
                text: hotkeys.display(settings.hotkeyScreenshot)
                color: theme.textMuted
                font.pixelSize: 11
                font.family: theme.monoFamily
                anchors.verticalCenter: parent.verticalCenter
            }
            NcButton {
                text: recording.stateText === "Error" ? "Retry" : "Recording"
                kind: "ghost"
                onClicked: {
                    if (recording.stateText === "Error" || (!settings.autoStartReplay && recording.stateText === "Idle"))
                        recording.startReplay()
                    else
                        ApplicationWindow.window.go(3, "")
                }
            }
        }

        Item { width: 1; height: 28 }

        Item {
            id: recentBar
            width: parent.width
            height: 32
            Text {
                text: "RECENT"
                color: theme.textMuted
                font.pixelSize: 10
                font.letterSpacing: 1.8
                font.family: theme.fontFamily
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: recentCount
                text: recentClips.count === 0 ? "No clips yet" : recentClips.count + " saved"
                color: theme.textMuted
                font.pixelSize: 11
                font.family: theme.monoFamily
                x: parent.width - width
                y: (parent.height - height) / 2
            }
            Rectangle {
                id: recentSelect
                width: selectLabel.implicitWidth + 22
                height: 32
                x: recentCount.x - width - 14
                y: 0
                color: page.selecting ? theme.accent : "transparent"
                Text {
                    id: selectLabel
                    anchors.centerIn: parent
                    text: page.selecting ? "Done" : "Select"
                    color: page.selecting ? theme.onAccent : theme.text
                    font.pixelSize: theme.labelSize
                    font.weight: Font.DemiBold
                    font.family: theme.fontFamily
                }
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        page.selecting = !page.selecting
                        if (!page.selecting)
                            page.picked = ({})
                    }
                }
            }
            Rectangle {
                id: recentDelete
                visible: page.pickCount > 0
                width: deleteLabel.implicitWidth + 22
                height: 32
                x: recentSelect.x - width - 8
                y: 0
                color: "transparent"
                Text {
                    id: deleteLabel
                    anchors.centerIn: parent
                    text: "Delete"
                    color: theme.danger
                    font.pixelSize: theme.labelSize
                    font.weight: Font.DemiBold
                    font.family: theme.fontFamily
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.askDelete(Object.keys(page.picked))
                }
            }
        }

        Item { width: 1; height: 12 }

        Item {
            id: recentSlot
            width: parent.width
            height: Math.max(180, parent.height - y - 36)
            clip: true

            Text {
                visible: recentClips.count === 0
                width: 420
                anchors.verticalCenter: parent.verticalCenter
                text: "The buffer is holding the last " + settings.replaySeconds + " seconds. Save a clip and it will land here."
                wrapMode: Text.WordWrap
                color: theme.textSecondary
                font.pixelSize: 14
                font.weight: Font.Light
                font.family: theme.fontFamily
            }

            GridView {
                id: recentGrid
                anchors.fill: parent
                visible: recentClips.count > 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                header: Item { width: 1; height: 6 }
                model: recentClips
                readonly property int cardWidth: 280
                cellWidth: Math.min(cardWidth, Math.max(1, width))
                cellHeight: Math.floor((cellWidth - 12) * 9 / 16 + 70)
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    width: recentGrid.cellWidth
                    height: recentGrid.cellHeight

                    MouseArea {
                        anchors.fill: parent
                        z: 6
                        acceptedButtons: Qt.RightButton
                        onClicked: clipMenu.openFor(model.id, model.name, model.path)
                    }

                    Column {
                        x: 6
                        y: 6
                        width: parent.width - 12
                        spacing: 8
                        Item {
                            id: thumb
                            width: parent.width
                            height: width * 9 / 16
                            Item {
                                anchors.fill: parent
                                anchors.margins: 1
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    source: model.thumbnailUrl || ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: model.thumbnailUrl !== ""
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    color: theme.control
                                    visible: model.thumbnailUrl === ""
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    color: "#66000000"
                                    visible: shot.containsMouse && !page.selecting
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 8
                                    text: model.durationText
                                    color: "white"
                                    font.pixelSize: 11
                                    font.family: theme.monoFamily
                                }
                                Text {
                                    visible: shot.containsMouse && !page.selecting
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 8
                                    text: "Open"
                                    color: "white"
                                    font.pixelSize: 11
                                    font.letterSpacing: 1
                                    font.family: theme.fontFamily
                                }
                                Rectangle {
                                    visible: page.selecting
                                    x: 8
                                    y: 8
                                    width: 18
                                    height: 18
                                    color: page.isPicked(model.id) ? theme.accent : "#66000000"
                                    border.width: 1
                                    border.color: "white"
                                    NcIcon {
                                        visible: page.isPicked(model.id)
                                        anchors.centerIn: parent
                                        name: "check"
                                        size: 12
                                        iconColor: theme.onAccent
                                    }
                                }
                            }
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                antialiasing: false
                                border.width: page.isPicked(model.id) ? 2 : (shot.containsMouse ? 1 : 0)
                                border.color: page.isPicked(model.id) ? theme.accent : theme.text
                                z: 3
                            }
                            MouseArea {
                                id: shot
                                anchors.fill: parent
                                hoverEnabled: true
                                z: 4
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (page.selecting)
                                        page.togglePick(model.id)
                                    else {
                                        editor.openPath(model.path)
                                        ApplicationWindow.window.go(2, "")
                                    }
                                }
                                Row {
                                    visible: shot.containsMouse
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 6
                                    spacing: 4
                                    z: 2
                                    Rectangle {
                                        width: 26
                                        height: 26
                                        color: "#CC101010"
                                        NcIcon { anchors.centerIn: parent; name: "edit"; size: 13; iconColor: "white" }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: page.askRename(model.id, model.name)
                                        }
                                    }
                                    Rectangle {
                                        width: 26
                                        height: 26
                                        color: "#CC101010"
                                        NcIcon { anchors.centerIn: parent; name: "trash"; size: 13; iconColor: "white" }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: page.askDelete([model.id])
                                        }
                                    }
                                }
                            }
                        }
                        Text {
                            width: parent.width
                            text: model.name
                            color: theme.text
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            font.family: theme.fontFamily
                        }
                        Text {
                            width: parent.width
                            text: model.game + "   " + model.date
                            color: theme.textMuted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            font.family: theme.fontFamily
                        }
                    }
                    Component.onCompleted: library.requestThumbnail(model.id)
                }
            }
        }

        RowLayout {
            width: parent.width
            spacing: 14
            Text {
                text: storage.usedText + " in clips"
                color: theme.textMuted
                font.pixelSize: 11
                font.family: theme.monoFamily
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                Layout.maximumWidth: 220
                color: theme.cardBorder
                Rectangle {
                    width: Math.max(storage.usedRatio > 0 ? 3 : 0, parent.width * storage.usedRatio)
                    height: 2
                    color: theme.textSecondary
                }
            }
            Text {
                text: storage.freeText + " free"
                color: theme.textMuted
                font.pixelSize: 11
                font.family: theme.monoFamily
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "Storage"
                color: theme.textSecondary
                font.pixelSize: 11
                font.family: theme.fontFamily
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    cursorShape: Qt.PointingHandCursor
                    onClicked: ApplicationWindow.window.go(4, "storage")
                }
            }
        }
    }

    MouseArea {
        id: appRenameScrim
        anchors.fill: parent
        z: 20
        enabled: appNameEdit.visible
        onPressed: mouse => {
            const pos = appNameEdit.mapToItem(appRenameScrim, 0, 0)
            const inside = mouse.x >= pos.x && mouse.y >= pos.y
                    && mouse.x <= pos.x + appNameEdit.width
                    && mouse.y <= pos.y + appNameEdit.height
            if (inside) {
                mouse.accepted = false
                return
            }
            page.closeAppRename(true)
        }
    }

    ClipMenu {
        id: clipMenu
        onRenameRequested: (id, name) => page.askRename(id, name)
        onDeleteRequested: id => page.askDelete([id])
    }

    Popup {
        id: renamePopup
        modal: true
        anchors.centerIn: parent
        width: 360
        padding: 16
        background: Rectangle { color: theme.card; border.color: theme.cardBorder; border.width: 1 }
        property string renameId: ""
        Column {
            width: parent.width
            spacing: 12
            Text { text: "Rename clip"; color: theme.text; font.pixelSize: 16; font.weight: Font.Medium; font.family: theme.displayFamily }
            NcTextField { id: renameField; width: parent.width }
            Row {
                spacing: 8
                NcButton {
                    text: "Save"
                    kind: "primary"
                    onClicked: {
                        library.renameClip(renamePopup.renameId, renameField.text)
                        renamePopup.close()
                    }
                }
                NcButton { text: "Cancel"; onClicked: renamePopup.close() }
            }
        }
    }

    Popup {
        id: confirmDelete
        modal: true
        anchors.centerIn: parent
        width: 380
        padding: 16
        background: Rectangle { color: theme.card; border.color: theme.cardBorder; border.width: 1 }
        Column {
            width: parent.width
            spacing: 12
            Text {
                width: parent.width
                text: page.pendingDelete.length > 1
                      ? "Move " + page.pendingDelete.length + " clips to the Recycle Bin?"
                      : "Move this clip to the Recycle Bin?"
                color: theme.text
                wrapMode: Text.WordWrap
                font.pixelSize: 15
                font.family: theme.fontFamily
            }
            Row {
                spacing: 8
                NcButton {
                    text: "Move to Recycle Bin"
                    kind: "danger"
                    onClicked: {
                        const ids = page.pendingDelete
                        const next = Object.assign({}, page.picked)
                        for (let i = 0; i < ids.length; ++i) {
                            library.deleteClip(ids[i])
                            delete next[ids[i]]
                        }
                        page.picked = next
                        page.pendingDelete = []
                        confirmDelete.close()
                    }
                }
                NcButton { text: "Cancel"; onClicked: confirmDelete.close() }
            }
        }
    }
}
