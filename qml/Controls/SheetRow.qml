pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property string title
    property string subtitle
    property bool selected: false
    property bool showRadio: true
    property url iconSource
    property bool destructive: false

    readonly property bool hasIcon: iconSource.toString().length > 0

    implicitHeight: Math.max(AppTheme.touchTargetMinimum,
                             _text.implicitHeight + 2 * AppTheme.spacing6)
    implicitWidth: 260

    Accessible.role: showRadio ? Accessible.RadioButton : Accessible.Button
    Accessible.name: title
    Accessible.checked: selected

    background: Rectangle {
        radius: AppTheme.radiusSmall
        color: root.down
               ? AppTheme.pressed
               : (root.hovered ? AppTheme.hover : "transparent")
        border.width: root.activeFocus ? AppTheme.focusThickness : 0
        border.color: AppTheme.focus
    }

    contentItem: Item {
        RadioIndicator {
            id: _radio

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showRadio
            selected: root.selected
        }

        ThemedIcon {
            id: _icon

            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            visible: root.hasIcon && !root.showRadio
            source: root.iconSource
            tintColor: root.destructive ? AppTheme.error : AppTheme.textSecondary
            showPlaceholder: false
        }

        Column {
            id: _text

            anchors.left: parent.left
            anchors.leftMargin: (root.showRadio || root.hasIcon)
                                ? 20 + AppTheme.spacing14 : 0
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: root.title
                color: root.destructive ? AppTheme.error : AppTheme.textPrimary
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
    }
}
