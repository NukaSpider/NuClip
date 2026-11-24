import QtQuick
import NuClip

Item {
    id: root

    CaptureCard {
        id: card
        width: implicitWidth
        height: implicitHeight
        opacity: overlay.fade * overlay.panelOpacity
        title: overlay.title
        detail: overlay.detail
        kind: overlay.kind
        compact: overlay.compact
        large: overlay.large
        accent: overlay.accent
        x: {
            const rest = overlay.anchorX === 1 ? (root.width - width) / 2
                : (overlay.anchorX === 2 ? root.width - width - 12 : 12)
            return rest + overlay.travel * overlay.shiftX
        }
        y: {
            const rest = overlay.anchorY === 2 ? root.height - height - 12
                : (overlay.anchorY === 1 ? (root.height - height) / 2 : 12)
            return rest + overlay.travel * overlay.shiftY
        }
    }
}
