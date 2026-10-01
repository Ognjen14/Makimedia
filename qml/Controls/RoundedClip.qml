import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property real radius: 0
    property bool clipping: true

    layer.enabled: root.clipping
    layer.effect: OpacityMask {
        maskSource: Rectangle {
            width: root.width
            height: root.height
            radius: root.radius
        }
    }
}
