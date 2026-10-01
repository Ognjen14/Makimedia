pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property url iconSource
    property bool selected: false
    property bool muted: false
    property bool rail: false
    property bool iconOnly: false
    property string countText
    property bool highlighted: false

    readonly property color contentColor: {
        if (selected)
            return AppTheme.onPrimaryStrong
        return muted ? AppTheme.textDisabled : AppTheme.textSecondary
    }

    implicitHeight: rail
                    ? (AppTheme.spacing8 * 2 + 21
                       + (iconOnly ? 0 : AppTheme.spacing4 + _railLabel.implicitHeight))
                    : AppTheme.controlHeightLarge
    implicitWidth: rail ? 64 : 200

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.checked: selected

    background: Rectangle {
        radius: root.rail ? AppTheme.radiusLarge : AppTheme.radiusPill
        color: root.selected ? AppTheme.primary : "transparent"

        StateLayer { control: root }

        Behavior on color {
            ColorAnimation { duration: 140 }
        }
    }

    contentItem: Item {
        FocusRing {
            active: root.visualFocus
                    || (root.highlighted && AppTheme.remoteNavigation)
            ringRadius: root.rail ? AppTheme.radiusLarge : AppTheme.radiusPill
            innerColor: (root.selected && AppTheme.focus === AppTheme.primary)
                        ? AppTheme.onPrimaryStrong
                        : AppTheme.focus
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: AppTheme.spacing16
            anchors.rightMargin: AppTheme.spacing16
            spacing: AppTheme.spacing14
            visible: !root.rail

            ThemedIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: 21
                height: 21
                source: root.iconSource
                tintColor: root.contentColor
                showPlaceholder: false
            }

            Text {
                id: _label

                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 21 - AppTheme.spacing14
                       - (root.countText.length > 0 ? _count.implicitWidth + AppTheme.spacing8 : 0)
                text: root.text
                color: root.contentColor
                font.pixelSize: AppTheme.fs14
                font.weight: root.selected ? Font.Medium : Font.Normal
                elide: Text.ElideRight
            }

            Text {
                id: _count

                anchors.verticalCenter: parent.verticalCenter
                visible: root.countText.length > 0
                text: root.countText
                color: root.selected ? AppTheme.onPrimaryStrong : AppTheme.textDisabled
                opacity: root.selected ? 0.72 : 1.0
                font.pixelSize: AppTheme.fs12
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: AppTheme.spacing4
            visible: root.rail

            ThemedIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 21
                height: 21
                source: root.iconSource
                tintColor: root.contentColor
                showPlaceholder: false
            }

            Text {
                id: _railLabel

                anchors.horizontalCenter: parent.horizontalCenter
                visible: !root.iconOnly
                text: root.text
                color: root.contentColor
                font.pixelSize: AppTheme.fs10
                font.weight: root.selected ? Font.Medium : Font.Normal
            }
        }
    }
}
