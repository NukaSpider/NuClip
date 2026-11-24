import QtQuick
import QtQuick.Controls

Menu {
    id: menu
    padding: 4
    width: 196

    property string clipId: ""
    property string clipName: ""
    property string clipPath: ""

    signal renameRequested(string id, string name)
    signal deleteRequested(string id)

    background: Rectangle {
        color: theme.elevated
        border.color: theme.cardBorder
        border.width: 1
    }

    component Entry: MenuItem {
        id: entry
        property bool danger: false
        implicitWidth: 188
        implicitHeight: 32
        font.pixelSize: 13
        font.family: theme.fontFamily
        contentItem: Text {
            text: entry.text
            color: entry.danger ? theme.danger : theme.text
            font: entry.font
            leftPadding: 10
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: entry.highlighted ? theme.controlHover : "transparent"
        }
    }

    Entry {
        text: "Rename"
        onTriggered: menu.renameRequested(menu.clipId, menu.clipName)
    }
    Entry {
        text: "Open File Location"
        onTriggered: library.reveal(menu.clipId)
    }
    Entry {
        text: "Edit"
        onTriggered: {
            editor.openPath(menu.clipPath)
            ApplicationWindow.window.go(2, "")
        }
    }
    Entry {
        text: "Delete"
        danger: true
        onTriggered: menu.deleteRequested(menu.clipId)
    }

    function openFor(id, name, path) {
        clipId = id
        clipName = name
        clipPath = path
        popup()
    }
}
