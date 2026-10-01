import QtQuick
import QtQuick.Controls
import "../Singletons"

Rectangle {
    id: root

    property AbstractButton control

    anchors.fill: parent
    radius: parent && parent.radius !== undefined ? parent.radius : 0
    color: {
        if (!root.control)
            return "transparent"
        if (root.control.down)
            return AppTheme.pressed
        return root.control.hovered ? AppTheme.hover : "transparent"
    }
}
