import QtQuick

Item {
    id: root
    property real from: 0
    property real to: 1
    property real value: 0
    signal moved(real value)
    implicitHeight: 28
    implicitWidth: 180
    Accessible.role: Accessible.Slider
    Accessible.onIncreaseAction: root.moved(Math.min(root.to, root.value + (root.to - root.from) * 0.1))
    Accessible.onDecreaseAction: root.moved(Math.max(root.from, root.value - (root.to - root.from) * 0.1))

    function fraction() {
        return Math.max(0, Math.min(1, (dragValue - from) / Math.max(0.0001, to - from)))
    }

    property real dragValue: value
    onValueChanged: if (!area.pressed) dragValue = value

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        radius: 2
        color: theme.control
        Rectangle {
            width: root.fraction() * parent.width
            height: parent.height
            radius: 2
            color: theme.accent
        }
    }

    Rectangle {
        width: 16
        height: 16
        radius: 8
        y: (parent.height - height) / 2
        x: root.fraction() * (parent.width - width)
        color: theme.card
        border.width: 2
        border.color: theme.accent
        antialiasing: true
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onPressed: event => setValue(event.x)
        onPositionChanged: event => { if (pressed) setValue(event.x) }
        function setValue(x) {
            const t = Math.max(0, Math.min(1, (x - 8) / Math.max(1, root.width - 16)))
            root.dragValue = root.from + t * (root.to - root.from)
            root.moved(root.dragValue)
        }
    }
}
