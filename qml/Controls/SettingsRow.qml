pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property bool highlighted: false

    enum Trailing {
        None,
        Switch,
        Chevron,
        Value
    }

    property int trailing: SettingsRow.None
    property string title
    property string subtitle
    property string valueText
    property bool switchChecked: false
    property url iconSource

    readonly property bool hasIcon: iconSource.toString().length > 0

    signal switchToggled(bool checked)

    implicitHeight: Math.max(AppTheme.controlHeightLarge,
                             _text.implicitHeight + 2 * AppTheme.spacing8)
    implicitWidth: 320

    Accessible.role: trailing === SettingsRow.Switch
                     ? Accessible.CheckBox
                     : Accessible.Button
    Accessible.name: title
    Accessible.description: subtitle

    onClicked: {
        if (trailing === SettingsRow.Switch) {
            switchChecked = !switchChecked
            switchToggled(switchChecked)
        }
    }

    background: Rectangle {
        radius: AppTheme.radiusSmall
        color: root.down
               ? AppTheme.pressed
               : (root.hovered ? AppTheme.hover : "transparent")

    }

    contentItem: Item {
        FocusRing {
            active: root.visualFocus
                    || (root.highlighted && AppTheme.remoteNavigation)
            ringRadius: AppTheme.radiusSmall
            z: 30
        }

        Rectangle {
            id: _icon

            anchors.left: parent.left
            anchors.leftMargin: AppTheme.rowContentPadding
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            height: 44
            visible: root.hasIcon
            radius: AppTheme.radiusPill
            color: AppTheme.primary

            ThemedIcon {
                anchors.centerIn: parent
                width: 20
                height: 20
                source: root.iconSource
                tintColor: AppTheme.onPrimaryStrong
                showPlaceholder: false
            }
        }

        Column {
            id: _text

            anchors.left: parent.left
            anchors.leftMargin: AppTheme.rowContentPadding
                                + (root.hasIcon ? 44 + AppTheme.spacing14 : 0)
            anchors.right: _trailingLoader.left
            anchors.rightMargin: AppTheme.spacing14
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: root.title
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fs14
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs11
                elide: Text.ElideRight
            }
        }

        Item {
            id: _trailingLoader

            anchors.right: parent.right
            anchors.rightMargin: AppTheme.rowContentPadding
            anchors.verticalCenter: parent.verticalCenter
            width: {
                switch (root.trailing) {
                case SettingsRow.Switch:
                    return 48
                case SettingsRow.Chevron:
                    return 18
                case SettingsRow.Value:
                    return _value.implicitWidth
                default:
                    return 0
                }
            }
            height: 28

            ToggleSwitch {
                anchors.centerIn: parent
                visible: root.trailing === SettingsRow.Switch
                checked: root.switchChecked
                onCheckedChanged: {
                    if (checked !== root.switchChecked) {
                        root.switchChecked = checked
                        root.switchToggled(checked)
                    }
                }
            }

            ThemedIcon {
                anchors.centerIn: parent
                width: 18
                height: 18
                visible: root.trailing === SettingsRow.Chevron
                source: Icons.chevronRight
                tintColor: AppTheme.textSecondary
                showPlaceholder: false
            }

            Text {
                id: _value

                anchors.centerIn: parent
                visible: root.trailing === SettingsRow.Value
                text: root.valueText
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs13
            }
        }
    }
}
