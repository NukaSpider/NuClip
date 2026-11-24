import QtQuick
import QtQuick.Controls

Item {
    id: root
    property var options: []
    property string value: ""
    property string placeholder: "Select"
    signal picked(string id)
    implicitHeight: 36
    implicitWidth: 220

    function optionId(option) {
        if (option === undefined || option === null)
            return ""
        if (typeof option === "string")
            return option
        return String(option.id)
    }
    function optionLabel(option) {
        if (option === undefined || option === null)
            return ""
        if (typeof option === "string")
            return option
        return String(option.label)
    }
    function currentLabel() {
        const list = options ? options : []
        for (let i = 0; i < list.length; ++i) {
            if (optionId(list[i]) === value)
                return optionLabel(list[i])
        }
        return value === "" ? placeholder : value
    }
    function openMenu() {
        const count = options ? options.length : 0
        popup.width = width
        popup.height = Math.min(292, count * 34 + 12)
        const overlay = Overlay.overlay
        if (overlay) {
            popup.parent = overlay
            const pos = mapToItem(overlay, 0, height + 4)
            popup.x = pos.x
            popup.y = pos.y
        } else {
            popup.x = 0
            popup.y = height + 4
        }
        popup.open()
    }

    Rectangle {
        anchors.fill: parent
        radius: 0
        color: area.containsMouse || popup.opened ? theme.controlHover : theme.control
        border.color: popup.opened || area.containsMouse ? theme.accent : theme.cardBorder
        border.width: 1
        antialiasing: false
        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 8
            Text {
                width: parent.width - 18
                anchors.verticalCenter: parent.verticalCenter
                text: root.currentLabel()
                color: theme.text
                font.pixelSize: 13
                elide: Text.ElideRight
                font.family: theme.fontFamily
            }
            NcIcon {
                name: "chevron"
                size: 12
                iconColor: theme.textMuted
                rotation: popup.opened ? -90 : 90
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            enabled: root.enabled
            onClicked: popup.opened ? popup.close() : Qt.callLater(root.openMenu)
        }
    }

    Popup {
        id: popup
        padding: 4
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: theme.elevated
            border.color: theme.cardBorder
            border.width: 1
            radius: 0
            antialiasing: false
        }
        contentItem: ListView {
            clip: true
            model: root.options
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 34
                color: optionArea.containsMouse ? theme.controlHover : "transparent"
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    text: root.optionLabel(modelData)
                    color: root.optionId(modelData) === root.value ? theme.accent : theme.text
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    font.family: theme.fontFamily
                }
                MouseArea {
                    id: optionArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.picked(root.optionId(modelData))
                        popup.close()
                    }
                }
            }
        }
    }
}
