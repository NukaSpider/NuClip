import QtQuick

Item {
    id: card
    property string title: "Clip saved"
    property string detail: ""
    property string kind: "clip"
    property bool compact: false
    property bool large: false
    property color accent: "#3DDC97"

    implicitHeight: large ? 72 : (compact ? 52 : 60)

    TextMetrics {
        id: titleMeasure
        text: card.title
        font.pixelSize: card.large ? 15 : (card.compact ? 12 : 13)
        font.weight: Font.Medium
        font.family: "Segoe UI Variable Text"
    }
    TextMetrics {
        id: detailMeasure
        text: card.detail
        font.pixelSize: 11
        font.family: "Segoe UI Variable Text"
    }

    implicitWidth: {
        const lead = 12 + (large ? 22 : (compact ? 16 : 18)) + 10
        const badgeW = (kind === "error" || kind === "record") ? 26 : 0
        const text = Math.max(titleMeasure.advanceWidth, detail === "" ? 0 : detailMeasure.advanceWidth)
        const raw = lead + badgeW + text + 18
        const cap = large ? 248 : (compact ? 188 : 210)
        const floor = large ? 148 : (compact ? 124 : 136)
        return Math.round(Math.max(floor, Math.min(cap, raw)))
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: 2
        anchors.leftMargin: -1
        anchors.rightMargin: -1
        radius: 9
        color: Qt.rgba(0, 0, 0, 0.42)
    }

    Rectangle {
        anchors.fill: parent
        radius: 8
        color: "#0A0A0A"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
        antialiasing: true

        Image {
            id: mark
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: card.large ? 22 : (card.compact ? 16 : 18)
            height: width
            source: "qrc:/assets/resources/nuclip.svg"
            sourceSize.width: 36
            sourceSize.height: 36
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Rectangle {
            id: badge
            visible: card.kind === "error" || card.kind === "record"
            anchors.left: mark.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            radius: 4
            color: card.kind === "error" ? "#F07178" : card.accent

            Rectangle {
                visible: card.kind === "record"
                width: 6
                height: 6
                radius: 1
                anchors.centerIn: parent
                color: "#07140F"
            }

            Text {
                visible: card.kind === "error"
                anchors.centerIn: parent
                text: "!"
                color: "#1A0909"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                font.family: "Segoe UI"
            }
        }

        Column {
            anchors.left: badge.visible ? badge.right : mark.right
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                width: parent.width
                text: card.title
                color: "#F3F3F3"
                elide: Text.ElideRight
                font.pixelSize: card.large ? 15 : (card.compact ? 12 : 13)
                font.weight: Font.Medium
                font.family: "Segoe UI Variable Text"
            }
            Text {
                width: parent.width
                visible: card.detail !== ""
                text: card.detail
                color: "#8C8C8C"
                elide: Text.ElideRight
                font.pixelSize: 11
                font.family: "Segoe UI Variable Text"
            }
        }
    }
}
