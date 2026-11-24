import QtQuick

Rectangle {
    id: root
    default property alias content: column.data
    property int pad: 16
    radius: 8
    color: theme.card
    border.color: theme.cardBorder
    border.width: 1
    implicitHeight: column.implicitHeight + pad * 2

    Column {
        id: column
        x: root.pad
        y: root.pad
        width: root.width - root.pad * 2
        spacing: 8
    }
}
