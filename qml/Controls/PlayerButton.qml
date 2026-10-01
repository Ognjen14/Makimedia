pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property url iconSource
    property bool primary: false
    property string accessibleName: text

    readonly property int diameter: primary ? 72 : 56
    readonly property int iconSize: primary ? 30 : 24

    implicitWidth: diameter
    implicitHeight: diameter

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName

    background: Rectangle {
        radius: AppTheme.radiusPill
        color: root.primary ? AppTheme.primary : Qt.rgba(1, 1, 1, 0.1)
        border.width: root.activeFocus ? AppTheme.focusThickness + 1 : 0
        border.color: AppTheme.focus

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: root.down
                   ? Qt.rgba(0, 0, 0, 0.18)
                   : (root.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
        }
    }

    contentItem: Item {
        ThemedIcon {
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize
            source: root.iconSource
            tintColor: root.primary ? AppTheme.onPrimaryStrong : "#FFFFFF"
            showPlaceholder: false
        }
    }
}
