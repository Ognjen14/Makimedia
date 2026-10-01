pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

Rectangle {
    id: root

    property color swatchColor: "transparent"
    property string label
    property bool selected: false

    signal clicked()

    width: AppTheme.touchTargetMinimum
    height: AppTheme.touchTargetMinimum
    radius: width / 2
    color: "transparent"
    border.width: selected ? 2 : 1
    border.color: selected ? AppTheme.primary : AppTheme.outlineStrong

    Accessible.role: Accessible.RadioButton
    Accessible.name: root.label
    Accessible.checked: root.selected

    Rectangle {
        anchors.centerIn: parent
        width: parent.width - 12
        height: width
        radius: width / 2
        color: AppTheme.surfaceVariant

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: root.swatchColor
        }
    }

    AbstractButton {
        id: _button

        anchors.fill: parent
        focusPolicy: AppTheme.remoteNavigation ? Qt.StrongFocus : Qt.NoFocus

        onClicked: root.clicked()

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Select
                    || event.key === Qt.Key_Return
                    || event.key === Qt.Key_Enter) {
                root.clicked()
                event.accepted = true
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width + AppTheme.spacing8
            height: width
            radius: width / 2
            color: "transparent"
            visible: _button.activeFocus
            border.width: AppTheme.focusThickness
            border.color: AppTheme.focus
        }
    }
}
