import QtQuick

Rectangle {
    id: root
    property string text: ""
    property string kind: "info"
    radius: 0
    color: "transparent"
    border.color: kind === "warning" ? theme.warning : theme.cardBorder
    border.width: 0
    Rectangle {
        width: 2
        height: parent.height
        color: root.kind === "warning" ? theme.warning : theme.textMuted
    }
    implicitHeight: label.implicitHeight + 20
    implicitWidth: 200

    Text {
        id: label
        anchors.fill: parent
        anchors.margins: 12
        text: root.text
        wrapMode: Text.WordWrap
        color: theme.textSecondary
        font.pixelSize: 12
        font.family: theme.fontFamily
        lineHeight: 1.35
    }
}
