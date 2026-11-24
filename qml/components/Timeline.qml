import QtQuick

Item {
    id: root

    property real duration: 1
    property real position: 0
    property real trimIn: 0
    property real trimOut: 1
    property real fps: 30
    property string waveform: ""

    signal seek(real seconds)
    signal trimInMoved(real seconds)
    signal trimOutMoved(real seconds)
    signal dragActive(bool active)

    implicitHeight: 128

    function span() {
        return Math.max(0.05, duration)
    }

    function xFor(seconds) {
        return Math.max(0, Math.min(width, seconds / span() * width))
    }

    function secondsFor(x) {
        return Math.max(0, Math.min(span(), x / Math.max(1, width) * span()))
    }

    function tickStep() {
        const px = width / span()
        const steps = [1, 2, 5, 10, 15, 30, 60, 120, 300, 600]
        for (let i = 0; i < steps.length; ++i) {
            if (steps[i] * px >= 80)
                return steps[i]
        }
        return 600
    }

    function pad(value) {
        return value < 10 ? "0" + value : "" + value
    }

    function tc(seconds) {
        const rate = Math.max(1, Math.round(fps))
        const whole = Math.max(0, Math.floor(seconds))
        const frame = Math.max(0, Math.min(rate - 1, Math.floor((seconds - whole) * rate)))
        const minutes = Math.floor(whole / 60)
        return minutes + ":" + pad(whole % 60) + "." + pad(frame)
    }

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        text: "VIDEO"
        color: theme.textMuted
        font.pixelSize: 10
        font.letterSpacing: 1.2
        font.family: theme.fontFamily
    }
    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        text: tc(trimIn) + "  –  " + tc(trimOut)
        color: theme.textSecondary
        font.pixelSize: 11
        font.family: theme.fontFamily
    }

    Item {
        id: ruler
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 18
        height: 18

        Repeater {
            model: Math.min(40, Math.floor(root.span() / root.tickStep()) + 1)
            delegate: Item {
                required property int index
                x: root.xFor(index * root.tickStep())
                width: 48
                height: ruler.height
                Rectangle {
                    width: 1
                    height: index % 2 === 0 ? 8 : 4
                    anchors.bottom: parent.bottom
                    color: theme.cardBorder
                }
                Text {
                    visible: index % 2 === 0
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    text: root.tc(index * root.tickStep())
                    color: theme.textMuted
                    font.pixelSize: 10
                    font.family: theme.fontFamily
                }
            }
        }
    }

    Rectangle {
        id: lane
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: ruler.bottom
        anchors.topMargin: 6
        height: 64
        radius: 0
        color: theme.elevated
        clip: true

        Image {
            anchors.fill: parent
            anchors.margins: 8
            source: root.waveform
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            visible: root.waveform !== ""
            opacity: 0.9
            cache: true
        }

        Rectangle {
            x: root.xFor(root.trimIn)
            width: Math.max(0, root.xFor(root.trimOut) - root.xFor(root.trimIn))
            height: parent.height
            color: theme.accent
            opacity: 0.16
        }
        Rectangle {
            x: root.xFor(root.trimIn)
            width: Math.max(0, root.xFor(root.trimOut) - root.xFor(root.trimIn))
            height: 2
            color: theme.accent
        }
    }

    Rectangle {
        id: inHandle
        x: root.xFor(root.trimIn) - 5
        y: lane.y - 2
        width: 10
        height: lane.height + 4
        radius: 2
        color: theme.accent
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height - 16
            color: theme.onAccent
            opacity: 0.7
        }
    }
    Rectangle {
        id: outHandle
        x: root.xFor(root.trimOut) - 5
        y: lane.y - 2
        width: 10
        height: lane.height + 4
        radius: 2
        color: theme.accent
        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: parent.height - 16
            color: theme.onAccent
            opacity: 0.7
        }
    }

    Item {
        id: playhead
        x: root.xFor(root.position) - 6
        y: ruler.y
        width: 12
        height: lane.y + lane.height - ruler.y
        z: 2
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 2
            height: parent.height
            color: theme.text
        }
        Canvas {
            width: 11
            height: 8
            anchors.horizontalCenter: parent.horizontalCenter
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.fillStyle = theme.text
                ctx.beginPath()
                ctx.moveTo(0, 0)
                ctx.lineTo(width, 0)
                ctx.lineTo(width / 2, height)
                ctx.closePath()
                ctx.fill()
            }
            onWidthChanged: requestPaint()
        }
    }

    Text {
        visible: hoverLine.visible
        x: Math.max(0, Math.min(width - 64, hoverLine.x - 28))
        y: lane.y + lane.height + 4
        text: root.tc(root.secondsFor(hoverLine.x))
        color: theme.textSecondary
        font.pixelSize: 10
        font.family: theme.fontFamily
    }

    Rectangle {
        id: hoverLine
        visible: surface.containsMouse && surface.drag === ""
        x: surface.hoverX
        y: lane.y
        width: 1
        height: lane.height
        color: theme.textMuted
        opacity: 0.7
    }

    MouseArea {
        id: surface
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        cursorShape: Qt.SizeHorCursor
        property string drag: ""
        property real hoverX: 0

        function pick(x) {
            const inX = root.xFor(root.trimIn)
            const outX = root.xFor(root.trimOut)
            const playX = root.xFor(root.position)
            if (Math.abs(x - inX) <= 14)
                return "in"
            if (Math.abs(x - outX) <= 14)
                return "out"
            if (Math.abs(x - playX) <= 10)
                return "play"
            return "seek"
        }

        function apply(x) {
            let seconds = root.secondsFor(x)
            if (drag === "in") {
                seconds = Math.min(seconds, root.trimOut - 0.05)
                root.trimInMoved(seconds)
            } else if (drag === "out") {
                seconds = Math.max(seconds, root.trimIn + 0.05)
                root.trimOutMoved(seconds)
            } else {
                root.seek(seconds)
            }
        }

        onPressed: mouse => {
            drag = pick(mouse.x)
            root.dragActive(true)
            apply(mouse.x)
        }
        onPositionChanged: mouse => {
            hoverX = mouse.x
            if (pressed)
                apply(mouse.x)
        }
        onReleased: mouse => {
            if (drag !== "")
                apply(mouse.x)
            drag = ""
            root.dragActive(false)
        }
        onExited: hoverX = -1
    }
}
