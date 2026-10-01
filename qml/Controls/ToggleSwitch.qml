pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    checkable: true

    implicitWidth: 48
    implicitHeight: 28

    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked

    contentItem: Item {}

    background: Rectangle {
        id: _track

        radius: height / 2
        color: root.checked ? AppTheme.primary : AppTheme.outlineStrong

        Behavior on color {
            ColorAnimation { duration: 140 }
        }

        StateLayer { control: root }

        Rectangle {
            id: _thumb

            y: (parent.height - height) / 2
            x: root.checked ? parent.width - width - 4 : 4
            width: 20
            height: 20
            radius: height / 2
            color: root.checked ? AppTheme.onPrimaryStrong : AppTheme.surface

            Behavior on x {
                NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
            }
            Behavior on color {
                ColorAnimation { duration: 140 }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.visualFocus
            color: "transparent"
            border.width: 2
            border.color: AppTheme.focus
        }
    }
}
