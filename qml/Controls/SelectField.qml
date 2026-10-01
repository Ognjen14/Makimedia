pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property string label
    property string value

    implicitHeight: AppTheme.controlHeightMedium
    implicitWidth: AppTheme.spacing16 + _label.implicitWidth + AppTheme.spacing6
                   + _value.implicitWidth + AppTheme.spacing8 + 18 + AppTheme.spacing12

    Accessible.role: Accessible.ComboBox
    Accessible.name: label
    Accessible.description: value

    background: Rectangle {
        radius: AppTheme.radiusPill
        color: AppTheme.surfaceVariant
        border.width: 1
        border.color: AppTheme.outline

        FocusRing {
            active: AppTheme.remoteNavigation ? root.activeFocus : root.visualFocus
            ringRadius: parent.radius
        }

        StateLayer { control: root }
    }

    contentItem: Item {
        Row {
            anchors.left: parent.left
            anchors.leftMargin: AppTheme.spacing16
            anchors.verticalCenter: parent.verticalCenter
            spacing: AppTheme.spacing6

            Text {
                id: _label

                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs13
            }

            Text {
                id: _value

                anchors.verticalCenter: parent.verticalCenter
                text: root.value
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fs14
                font.weight: Font.Medium
            }

            Item {
                width: AppTheme.spacing2
                height: 1
            }

            ThemedIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                source: Icons.chevronDown
                tintColor: AppTheme.textSecondary
                showPlaceholder: false
            }
        }
    }
}
