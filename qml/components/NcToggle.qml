import QtQuick

Item {
    id: root
    property bool checked: false
    property string accessibleName: ""
    property bool rowHovered: false
    signal toggled(bool checked)

    implicitWidth: 20
    implicitHeight: 20
    opacity: enabled ? 1 : 0.4
    activeFocusOnTab: enabled

    readonly property bool hot: enabled && (area.containsMouse || rowHovered)
    readonly property color restBorder: Qt.rgba(theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.72)
    property bool motion: false

    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName !== "" ? accessibleName : "Setting"
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.onPressAction: if (root.enabled) root.toggled(!root.checked)
    Accessible.onToggleAction: if (root.enabled) root.toggled(!root.checked)

    Component.onCompleted: motion = true

    Rectangle {
        anchors.centerIn: face
        width: 28
        height: 28
        radius: 7
        color: "transparent"
        border.width: 1
        border.color: theme.text
        opacity: root.activeFocus ? 1 : 0
        Behavior on opacity {
            enabled: root.motion
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: face
        anchors.centerIn: parent
        width: 20
        height: 20
        radius: 4
        color: root.checked ? theme.accent : "transparent"
        border.width: 1
        border.color: root.checked ? theme.accent : (root.hot ? theme.textSecondary : root.restBorder)
        Behavior on color {
            enabled: root.motion
            ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
        }
        Behavior on border.color {
            enabled: root.motion
            ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        Canvas {
            id: mark
            anchors.fill: parent
            antialiasing: true
            opacity: root.checked ? 1 : 0
            scale: root.checked ? 1 : 0.72
            transformOrigin: Item.Center
            Behavior on opacity {
                enabled: root.motion
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
            Behavior on scale {
                enabled: root.motion
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.strokeStyle = theme.onAccent
                ctx.lineWidth = 1.7
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(5.4, 10.5)
                ctx.lineTo(8.7, 13.8)
                ctx.lineTo(14.8, 7.1)
                ctx.stroke()
            }
            Component.onCompleted: requestPaint()
        }
    }

    Connections {
        target: theme
        function onChanged() { mark.requestPaint() }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        anchors.margins: -8
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }

    Keys.onPressed: (event) => {
        if (!root.enabled)
            return
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.toggled(!root.checked)
            event.accepted = true
        }
    }
}
