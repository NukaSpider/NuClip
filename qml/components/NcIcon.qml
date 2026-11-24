import QtQuick

Text {
    id: root
    property string name: ""
    property int size: 18
    property color iconColor: theme.text
    font.family: theme.iconFamily
    font.pixelSize: size
    color: iconColor
    renderType: Text.NativeRendering
    text: {
        switch (name) {
        case "home": return "\uE80F"
        case "clips": return "\uE714"
        case "edit": return "\uE70F"
        case "record": return "\uE7C8"
        case "settings": return "\uE713"
        case "search": return "\uE721"
        case "folder": return "\uE8B7"
        case "trash": return "\uE74D"
        case "play": return "\uE768"
        case "pause": return "\uE769"
        case "mic": return "\uE720"
        case "speaker": return "\uE767"
        case "camera": return "\uE722"
        case "check": return "\uE73E"
        case "chevron": return "\uE76C"
        case "back": return "\uE76B"
        case "info": return "\uE946"
        case "warning": return "\uE7BA"
        case "monitor": return "\uE7F4"
        case "game": return "\uE7FC"
        case "clock": return "\uE823"
        case "save": return "\uE74E"
        default: return ""
        }
    }
}
