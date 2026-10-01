import QtQuick
import "../Singletons"

Rectangle {
    id: root

    property bool selected: false

    implicitWidth: 20
    implicitHeight: 20
    radius: AppTheme.radiusPill
    color: "transparent"
    border.width: 2
    border.color: selected ? AppTheme.primary : AppTheme.outlineStrong

    Behavior on border.color {
        ColorAnimation { duration: 120 }
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.selected ? 10 : 0
        height: width
        radius: AppTheme.radiusPill
        color: AppTheme.primary

        Behavior on width {
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }
    }
}
