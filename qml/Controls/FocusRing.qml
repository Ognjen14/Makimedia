import QtQuick
import "../Singletons"

Item {
    id: root

    property bool active: false
    property real ringRadius: AppTheme.radiusMedium
    property int thickness: AppTheme.focusThickness
    property color innerColor: AppTheme.focus

    anchors.fill: parent
    visible: active
    z: 30

    Rectangle {
        anchors.fill: parent
        radius: root.ringRadius
        color: "transparent"
        border.width: root.thickness
        border.color: root.innerColor
    }
}
