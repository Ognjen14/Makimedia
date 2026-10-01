pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property url iconSource
    property bool compact: false
    property color tintColor: AppTheme.textSecondary
    property string accessibleName: text
    property string tooltipText: accessibleName
    property bool scrim: false

    readonly property int diameter: compact ? 36 : AppTheme.touchTargetMinimum
    readonly property int iconSize: compact ? 18 : 22

    implicitWidth: diameter
    implicitHeight: diameter

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName.length > 0 ? accessibleName : qsTr("Button")

    contentItem: Item {
        ThemedIcon {
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize
            source: root.iconSource
            tintColor: root.enabled ? root.tintColor : AppTheme.textDisabled
            showPlaceholder: true
            placeholderText: ""
            placeholderBorderColor: AppTheme.outlineStrong
        }
    }

    HoldToolTip {
        id: _tooltip

        text: root.tooltipText
    }

    onPressAndHold: _tooltip.hold()

    background: Rectangle {
        radius: AppTheme.radiusPill
        color: root.down
               ? AppTheme.pressed
               : (root.hovered
                  ? AppTheme.hover
                  : (root.scrim ? Qt.rgba(0, 0, 0, 0.45) : "transparent"))

        FocusRing {
            active: AppTheme.remoteNavigation ? root.activeFocus
                                              : root.visualFocus
            ringRadius: parent.radius
        }
    }
}
