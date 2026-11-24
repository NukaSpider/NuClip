import QtQuick

Rectangle {
    id: root
    property real level: 0
    implicitHeight: 6
    implicitWidth: 120
    radius: 3
    color: theme.control
    Rectangle {
        width: Math.max(0, Math.min(1, root.level)) * parent.width
        height: parent.height
        radius: 3
        color: root.level > 0.9 ? theme.danger : theme.accent
    }
}
