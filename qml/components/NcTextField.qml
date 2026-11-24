import QtQuick

Rectangle {
    id: root
    property alias text: input.text
    property string placeholder: ""
    property bool numeric: false
    property bool dismissOnEscape: false
    signal editingFinished()
    signal finished()
    signal cancelled()
    function edit() {
        input.forceActiveFocus()
        input.selectAll()
    }
    implicitHeight: 32
    implicitWidth: 160
    radius: 0
    color: "transparent"
    border.width: 0
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: input.activeFocus ? 2 : 1
        color: input.activeFocus ? theme.accent : theme.cardBorder
    }

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        color: theme.text
        font.pixelSize: theme.bodySize
        font.family: theme.fontFamily
        selectByMouse: true
        selectionColor: theme.accent
        selectedTextColor: theme.onAccent
        clip: true
        inputMethodHints: root.numeric ? Qt.ImhDigitsOnly : Qt.ImhNone
        validator: root.numeric ? intValidator : null
        property bool edited: false
        onTextEdited: edited = true
        onEditingFinished: {
            root.finished()
            if (!edited)
                return
            edited = false
            root.editingFinished()
        }
        Keys.onEscapePressed: event => {
            edited = false
            root.cancelled()
            event.accepted = true
        }
        IntValidator { id: intValidator; bottom: 0; top: 1000000 }
    }

    Text {
        z: -1
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: Text.AlignVCenter
        text: root.placeholder
        color: theme.textMuted
        font.pixelSize: theme.bodySize
        font.family: theme.fontFamily
        visible: input.text.length === 0
        elide: Text.ElideRight
    }
}
