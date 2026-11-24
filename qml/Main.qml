import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1280
    height: 820
    minimumWidth: 1100
    minimumHeight: 700
    visible: true
    title: "NuClip"
    color: theme.background
    font.family: theme.fontFamily
    font.pixelSize: 13

    property int page: 0
    property string settingsCategory: "general"
    property string toastTitle: ""
    property string toastDetail: ""

    function go(index, category) {
        page = index
        if (category !== undefined && category !== "")
            settingsCategory = category
    }

    function notify(title, detail) {
        toastTitle = title
        toastDetail = detail || ""
        toastTimer.restart()
    }

    function live() {
        return recording.stateText === "Recording" || recording.stateText === "Paused" || recording.stateText === "Saving clip"
    }

    function signalWord() {
        const state = recording.stateText
        if (recording.booting && state === "Idle")
            return "INIT"
        if (state === "Starting")
            return "ARM"
        if (state === "Error")
            return "FAIL"
        if (state === "Paused")
            return "HOLD"
        if (state === "Stopping")
            return "STOP"
        if (state === "Saving clip")
            return "SAVE"
        if (state === "Recording")
            return recording.mode === "session" ? "REC" : "LIVE"
        return "OFF"
    }

    Timer {
        id: toastTimer
        interval: 4200
        onTriggered: {
            window.toastTitle = ""
            window.toastDetail = ""
        }
    }

    Connections {
        target: recording
        function onErrorChanged() {
            if (recording.error.length > 0)
                window.notify("Replay unavailable", "")
        }
    }

    component CaptureMark: Item {
        id: mark
        property bool live: false
        property int tick: 8
        implicitWidth: 22
        implicitHeight: 22

        Rectangle { width: tick; height: 1; color: theme.text }
        Rectangle { width: 1; height: tick; color: theme.text }
        Rectangle { anchors.right: parent.right; width: tick; height: 1; color: theme.text }
        Rectangle { anchors.right: parent.right; width: 1; height: tick; color: theme.text }
        Rectangle { anchors.bottom: parent.bottom; width: tick; height: 1; color: theme.text }
        Rectangle { anchors.bottom: parent.bottom; width: 1; height: tick; color: theme.text }
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: tick
            height: 1
            color: mark.live ? theme.accent : theme.text
            SequentialAnimation on opacity {
                running: mark.live
                loops: Animation.Infinite
                NumberAnimation { from: 1; to: 0.35; duration: 700 }
                NumberAnimation { from: 0.35; to: 1; duration: 700 }
            }
        }
        Rectangle {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 1
            height: tick
            color: mark.live ? theme.accent : theme.text
        }
    }

    Row {
        anchors.fill: parent

        Rectangle {
            id: rail
            width: 76
            height: parent.height
            color: theme.sidebar

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 18
                spacing: 2

                Item {
                    width: parent.width
                    height: 48
                    CaptureMark {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 2
                        live: window.live()
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        text: "NUCLIP"
                        color: theme.text
                        font.pixelSize: 8
                        font.letterSpacing: 1.4
                        font.weight: Font.DemiBold
                        font.family: theme.fontFamily
                    }
                }

                Item { width: 1; height: 14 }

                Repeater {
                    model: [
                        { icon: "home", label: "Home", index: 0 },
                        { icon: "clips", label: "Clips", index: 1 },
                        { icon: "edit", label: "Edit", index: 2 },
                        { icon: "record", label: "Rec", index: 3 },
                        { icon: "settings", label: "Prefs", index: 4 }
                    ]
                    delegate: Item {
                        required property var modelData
                        width: rail.width
                        height: 54
                        Rectangle {
                            anchors.fill: parent
                            color: navHit.containsMouse && window.page !== modelData.index ? theme.controlHover : "transparent"
                        }
                        NcIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: 8
                            name: modelData.icon
                            size: 16
                            iconColor: window.page === modelData.index ? theme.text : theme.textMuted
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            text: modelData.label.toUpperCase()
                            color: window.page === modelData.index ? theme.text : theme.textMuted
                            font.pixelSize: 9
                            font.letterSpacing: 0.8
                            font.family: theme.fontFamily
                        }
                        Rectangle {
                            visible: window.page === modelData.index
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            width: 14
                            height: 1
                            color: theme.accent
                        }
                        MouseArea {
                            id: navHit
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: window.page = modelData.index
                        }
                    }
                }
            }

            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16
                height: 28
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    text: window.signalWord()
                    color: window.live() ? theme.accent : theme.textMuted
                    font.pixelSize: 11
                    font.letterSpacing: 1.6
                    font.weight: Font.DemiBold
                    font.family: theme.monoFamily
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: window.page = 3
                }
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: theme.cardBorder
            }
        }

        Item {
            id: stage
            width: parent.width - rail.width
            height: parent.height
            opacity: 1

            StackLayout {
                anchors.fill: parent
                currentIndex: window.page
                HomePage { }
                ClipsPage { }
                EditorPage { }
                RecordingPage { }
                SettingsPage { }
            }

            Behavior on opacity { NumberAnimation { duration: 120 } }
        }
    }

    onPageChanged: {
        stage.opacity = 0.55
        stage.opacity = 1
        discord.setSurface(page)
    }
    Component.onCompleted: discord.setSurface(page)

    Rectangle {
        visible: updates.available
        anchors.fill: parent
        color: "#C0080808"
        z: 30

        MouseArea { anchors.fill: parent }

        Rectangle {
            id: updateCard
            width: 420
            height: updateBody.implicitHeight + 48
            anchors.centerIn: parent
            color: theme.card
            border.color: theme.cardBorder
            border.width: 1

            Column {
                id: updateBody
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 24
                spacing: 14

                Text {
                    text: "UPDATE"
                    color: theme.textMuted
                    font.pixelSize: 11
                    font.letterSpacing: 1.6
                    font.family: theme.fontFamily
                }
                Text {
                    width: parent.width
                    text: "Version " + updates.version
                    color: theme.text
                    font.pixelSize: 28
                    font.weight: Font.Light
                    font.family: theme.displayFamily
                }
                Text {
                    width: parent.width
                    visible: updates.notes !== ""
                    text: updates.notes
                    color: theme.textSecondary
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                    font.family: theme.fontFamily
                }
                Row {
                    spacing: 10
                    NcButton {
                        text: "Update"
                        kind: "primary"
                        onClicked: updates.apply()
                    }
                    NcButton {
                        text: "Skip for now"
                        kind: "ghost"
                        onClicked: updates.skip()
                    }
                }
            }
        }
    }

    Rectangle {
        visible: window.toastTitle !== ""
        anchors.left: parent.left
        anchors.leftMargin: 92
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: Math.min(480, toastText.implicitWidth + 28)
        height: toastText.implicitHeight + 16
        radius: 0
        color: theme.elevated
        z: 20
        Rectangle {
            width: 2
            height: parent.height
            color: theme.accent
        }
        Text {
            id: toastText
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            text: window.toastDetail !== "" ? window.toastTitle + "  " + window.toastDetail : window.toastTitle
            color: theme.text
            font.pixelSize: 12
            font.family: theme.fontFamily
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }
    }
}
