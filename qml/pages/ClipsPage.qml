import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia

Item {
    id: page

    Column {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        Item {
            width: page.viewWidth
            height: 32
            Text {
                text: "LIBRARY"
                color: theme.text
                font.pixelSize: 12
                font.letterSpacing: 2
                font.weight: Font.DemiBold
                font.family: theme.fontFamily
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
            Rectangle {
                id: libraryList
                width: listLabel.implicitWidth + 22
                height: 32
                x: parent.width - width
                y: 0
                color: page.layoutMode === "list" ? theme.accent : "transparent"
                Text {
                    id: listLabel
                    anchors.centerIn: parent
                    text: "List"
                    color: page.layoutMode === "list" ? theme.onAccent : theme.text
                    font.pixelSize: theme.labelSize
                    font.weight: Font.DemiBold
                    font.family: theme.fontFamily
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.layoutMode = "list" }
            }
            Rectangle {
                id: libraryGrid
                width: gridLabel.implicitWidth + 22
                height: 32
                x: libraryList.x - width - 8
                y: 0
                color: page.layoutMode === "grid" ? theme.accent : "transparent"
                Text {
                    id: gridLabel
                    anchors.centerIn: parent
                    text: "Grid"
                    color: page.layoutMode === "grid" ? theme.onAccent : theme.text
                    font.pixelSize: theme.labelSize
                    font.weight: Font.DemiBold
                    font.family: theme.fontFamily
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.layoutMode = "grid" }
            }
            Rectangle {
                id: libraryDelete
                visible: page.pickCount > 0
                width: libraryDeleteLabel.implicitWidth + 22
                height: 32
                x: librarySelect.x - width - 8
                y: 0
                color: "transparent"
                Text {
                    id: libraryDeleteLabel
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
                    onClicked: {
                        page.pendingDelete = Object.keys(page.picked)
                        confirmDelete.open()
                    }
                }
            }
            Rectangle {
                id: librarySelect
                width: librarySelectLabel.implicitWidth + 22
                height: 32
                x: libraryGrid.x - width - 8
                y: 0
                color: page.selecting ? theme.accent : "transparent"
                Text {
                    id: librarySelectLabel
                    anchors.centerIn: parent
                    text: page.selecting ? "Done" : "Select"
                    color: page.selecting ? theme.onAccent : theme.text
                    font.pixelSize: theme.labelSize
                    font.weight: Font.DemiBold
                    font.family: theme.fontFamily
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        page.selecting = !page.selecting
                        if (!page.selecting)
                            page.picked = ({})
                    }
                }
            }
            Text {
                id: libraryCount
                text: clipFilter.count + (clipFilter.count === 1 ? " clip" : " clips")
                color: theme.textMuted
                font.pixelSize: 13
                font.family: theme.fontFamily
                x: (page.pickCount > 0 ? libraryDelete.x : librarySelect.x) - width - 12
                y: (parent.height - height) / 2
            }
        }

        Row {
            width: parent.width
            spacing: 8
            NcTextField {
                id: search
                width: 240
                placeholder: "Search clips"
                onTextChanged: clipFilter.search = text
            }
            NcCombo {
                width: 180
                options: gameOptions()
                value: clipFilter.game
                onPicked: id => clipFilter.game = id
            }
            NcCombo {
                width: 150
                options: [
                    { id: "all", label: "All time" },
                    { id: "today", label: "Today" },
                    { id: "7", label: "Last 7 days" },
                    { id: "30", label: "Last 30 days" }
                ]
                value: clipFilter.dateRange
                onPicked: id => clipFilter.dateRange = id
            }
            NcCombo {
                width: 150
                options: [
                    { id: "newest", label: "Newest" },
                    { id: "oldest", label: "Oldest" },
                    { id: "name", label: "Name" },
                    { id: "largest", label: "Largest" },
                    { id: "longest", label: "Longest" },
                    { id: "game", label: "Game" }
                ]
                value: sortMode
                onPicked: id => { sortMode = id; clipFilter.setSortMode(id) }
            }
        }

        Item {
            width: parent.width
            height: parent.height - y
            GridView {
                id: grid
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width - (page.selectedPath !== "" ? 360 : 0)
                cellWidth: page.layoutMode === "list" ? width : Math.max(240, Math.floor(width / Math.max(1, Math.floor(width / 280))))
                cellHeight: page.layoutMode === "list" ? 84 : cellWidth * 0.56 + 64
                model: clipFilter
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Text {
                    visible: clipFilter.count === 0
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "No clips match this view. Recordings you save appear here."
                    color: theme.textSecondary
                    font.pixelSize: 14
                    font.family: theme.fontFamily
                }

                delegate: Item {
                    width: grid.cellWidth - 12
                    height: grid.cellHeight - 12
                    Rectangle {
                        anchors.fill: parent
                        radius: 0
                        color: "transparent"
                        border.color: "transparent"
                        border.width: 0
                        Item {
                            id: cardBody
                            anchors.fill: parent
                            anchors.margins: 8
                            Rectangle {
                                id: thumb
                                width: page.layoutMode === "list" ? 120 : parent.width
                                height: page.layoutMode === "list" ? parent.height : parent.height - 46
                                radius: 0
                                color: theme.control
                                border.width: page.isPicked(model.id) ? 2 : (page.selectedPath === model.path ? 1 : (hover.containsMouse ? 1 : 0))
                                border.color: page.isPicked(model.id) || page.selectedPath === model.path ? theme.accent : theme.text
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    source: model.thumbnailUrl || ""
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: model.thumbnailUrl !== ""
                                }
                                Text {
                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 8
                                    text: model.durationText
                                    color: "white"
                                    font.pixelSize: 11
                                    font.family: theme.fontFamily
                                    Rectangle {
                                        anchors.fill: parent
                                        anchors.margins: -4
                                        radius: 4
                                        color: "#99000000"
                                        z: -1
                                    }
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
                            Column {
                                anchors.left: page.layoutMode === "list" ? thumb.right : parent.left
                                anchors.leftMargin: page.layoutMode === "list" ? 12 : 0
                                anchors.right: parent.right
                                anchors.top: page.layoutMode === "list" ? undefined : thumb.bottom
                                anchors.topMargin: page.layoutMode === "list" ? 0 : 8
                                anchors.verticalCenter: page.layoutMode === "list" ? parent.verticalCenter : undefined
                                spacing: 4
                                Text {
                                    width: parent.width
                                    text: model.name
                                    color: theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    font.family: theme.fontFamily
                                }
                                Text {
                                    width: parent.width
                                    text: model.game + "  ·  " + model.date + "  ·  " + model.resolution
                                    color: theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    font.family: theme.fontFamily
                                }
                            }
                        }
                        MouseArea {
                            id: hover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: mouse => {
                                if (mouse.button === Qt.RightButton) {
                                    if (!page.selecting)
                                        page.select(model.path, model.id, model.name)
                                    clipMenu.openFor(model.id, model.name, model.path)
                                    return
                                }
                                if (page.selecting) {
                                    page.togglePick(model.id)
                                    return
                                }
                                page.select(model.path, model.id, model.name)
                            }
                            onDoubleClicked: {
                                if (page.selecting)
                                    return
                                editor.openPath(model.path)
                                ApplicationWindow.window.go(2, "")
                            }
                            Row {
                                visible: hover.containsMouse
                                x: cardBody.x + thumb.x + thumb.width - implicitWidth - 6
                                y: cardBody.y + thumb.y + 6
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
                                        onClicked: {
                                            page.selectedId = model.id
                                            page.selectedName = model.name
                                            renameField.text = model.name
                                            renamePopup.open()
                                        }
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
                                        onClicked: {
                                            page.pendingDelete = [model.id]
                                            confirmDelete.open()
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Component.onCompleted: library.requestThumbnail(model.id)
                }
            }

            Rectangle {
                visible: page.selectedPath !== ""
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 344
                radius: 0
                color: "transparent"
                border.width: 0
                Rectangle { width: 1; height: parent.height; color: theme.cardBorder }
                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    VideoOutput {
                        id: previewOut
                        width: parent.width
                        height: width * 9 / 16
                        fillMode: VideoOutput.PreserveAspectFit
                    }
                    Text {
                        width: parent.width
                        text: page.selectedName
                        color: theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        font.family: theme.fontFamily
                    }
                    Row {
                        spacing: 8
                        NcButton {
                            text: preview.playbackState === MediaPlayer.PlayingState ? "Pause" : "Play"
                            icon: preview.playbackState === MediaPlayer.PlayingState ? "pause" : "play"
                            kind: "primary"
                            onClicked: preview.playbackState === MediaPlayer.PlayingState ? preview.pause() : preview.play()
                        }
                        NcButton {
                            text: "Editor"
                            onClicked: {
                                editor.openPath(page.selectedPath)
                                ApplicationWindow.window.go(2, "")
                            }
                        }
                    }
                    NcButton { text: "Show in folder"; onClicked: library.reveal(page.selectedId) }
                    NcButton {
                        text: "Rename"
                        onClicked: {
                            renameField.text = page.selectedName
                            renamePopup.open()
                        }
                    }
                    NcButton {
                        text: "Move to Recycle Bin"
                        kind: "danger"
                        onClicked: {
                            page.pendingDelete = [page.selectedId]
                            confirmDelete.open()
                        }
                    }
                }
            }
        }
    }

    MediaPlayer {
        id: preview
        videoOutput: previewOut
        audioOutput: AudioOutput { volume: 1 }
        source: page.selectedPath !== "" ? page.fileUrl(page.selectedPath) : ""
    }

    property string selectedPath: ""
    property string selectedId: ""
    property string selectedName: ""
    property string sortMode: "newest"
    property string layoutMode: "grid"
    property bool selecting: false
    property var picked: ({})
    readonly property int pickCount: Object.keys(picked).length
    property var pendingDelete: []

    function isPicked(id) {
        return picked[id] === true
    }

    readonly property real viewWidth: Math.max(240, page.width / Screen.devicePixelRatio - 72)

    function togglePick(id) {
        const next = Object.assign({}, picked)
        if (next[id])
            delete next[id]
        else
            next[id] = true
        picked = next
    }

    ClipMenu {
        id: clipMenu
        onRenameRequested: (id, name) => {
            page.selectedId = id
            page.selectedName = name
            renameField.text = name
            renamePopup.open()
        }
        onDeleteRequested: id => {
            page.pendingDelete = [id]
            confirmDelete.open()
        }
    }

    function select(path, id, name) {
        selectedPath = path
        selectedId = id
        selectedName = name
        preview.stop()
        preview.source = fileUrl(path)
        preview.play()
    }

    function fileUrl(path) {
        return "file:///" + path.replace(/\\/g, "/")
    }

    function gameOptions() {
        let options = [{ id: "All games", label: "All games" }]
        const games = library.games
        for (let i = 0; i < games.length; ++i)
            options.push({ id: games[i], label: games[i] })
        return options
    }

    Popup {
        id: renamePopup
        modal: true
        anchors.centerIn: parent
        width: 360
        padding: 16
        background: Rectangle { color: theme.card; radius: 12; border.color: theme.cardBorder }
        Column {
            width: parent.width
            spacing: 12
            Text { text: "Rename clip"; color: theme.text; font.pixelSize: 16; font.weight: Font.DemiBold; font.family: theme.displayFamily }
            NcTextField { id: renameField; width: parent.width; text: page.selectedName }
            Row {
                spacing: 8
                NcButton {
                    text: "Save"
                    kind: "primary"
                    onClicked: {
                        library.renameClip(page.selectedId, renameField.text)
                        page.selectedName = renameField.text
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
        width: 360
        padding: 16
        background: Rectangle { color: theme.card; radius: 12; border.color: theme.cardBorder }
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
                        const ids = page.pendingDelete.length > 0 ? page.pendingDelete : [page.selectedId]
                        const next = Object.assign({}, page.picked)
                        for (let i = 0; i < ids.length; ++i) {
                            library.deleteClip(ids[i])
                            delete next[ids[i]]
                            if (ids[i] === page.selectedId) {
                                page.selectedPath = ""
                                page.selectedId = ""
                                preview.stop()
                            }
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

    onVisibleChanged: if (!visible) preview.pause()
}
