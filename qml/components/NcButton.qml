import QtQuick

Item {
    id: root
    property string text: ""
    property string icon: ""
    property string kind: "secondary"
    property string hint: ""
    signal clicked()
    implicitHeight: 32
    implicitWidth: Math.max(32, label.implicitWidth + (icon !== "" ? 22 : 0) + 24)
    opacity: enabled ? 1 : 0.4
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: {
            if (!root.enabled)
                return root.kind === "primary" ? theme.accent : "transparent"
            if (root.kind === "primary")
                return area.containsMouse ? Qt.lighter(theme.accent, 1.08) : theme.accent
            if (root.kind === "danger")
                return area.containsMouse ? Qt.rgba(theme.danger.r, theme.danger.g, theme.danger.b, 0.12) : "transparent"
            if (root.kind === "ghost")
                return area.containsMouse ? theme.controlHover : "transparent"
            return area.containsMouse ? theme.controlHover : "transparent"
        }
        border.width: root.kind === "primary" || root.kind === "ghost" ? 0 : 1
        border.color: root.kind === "danger" ? theme.danger : theme.cardBorder

        Row {
            anchors.centerIn: parent
            spacing: 6
            NcIcon {
                name: root.icon
                size: 14
                iconColor: label.color
                visible: root.icon !== ""
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: label
                text: root.text
                visible: root.text !== ""
                color: {
                    if (root.kind === "primary")
                        return theme.onAccent
                    if (root.kind === "danger")
                        return theme.danger
                    return theme.text
                }
                font.pixelSize: theme.labelSize
                font.weight: Font.DemiBold
                font.family: theme.fontFamily
            }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Text {
        visible: root.hint !== "" && area.containsMouse
        text: root.hint
        y: parent.height + 6
        x: 0
        color: theme.text
        font.pixelSize: theme.smallSize
        font.family: theme.fontFamily
        Rectangle {
            anchors.fill: parent
            anchors.margins: -6
            radius: 4
            color: theme.elevated
            border.color: theme.cardBorder
            z: -1
        }
    }
}
