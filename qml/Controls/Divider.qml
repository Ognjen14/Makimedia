import QtQuick
import "../Singletons"

Item {
    id: root

    property real inset: 0

    implicitHeight: 1
    height: 1

    Rectangle {
        x: root.inset
        width: Math.max(0, root.width - root.inset)
        height: 1
        color: AppTheme.outline
    }
}
