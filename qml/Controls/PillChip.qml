pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

AbstractButton {
    id: root

    property bool selected: false
    property bool highlighted: false

    hoverEnabled: true
    focusPolicy: Qt.NoFocus

    leftPadding: AppTheme.spacing16
    rightPadding: AppTheme.spacing16
    implicitHeight: AppTheme.controlHeightSmall
    implicitWidth: leftPadding + _label.implicitWidth + rightPadding

    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: selected

    contentItem: Text {
        id: _label

        text: root.text
        color: root.selected ? AppTheme.onPrimaryStrong : AppTheme.textPrimary
        font.pixelSize: AppTheme.fs13
        font.weight: root.selected ? Font.Medium : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: AppTheme.radiusPill
        color: root.selected ? AppTheme.primary : "transparent"
        border.width: root.highlighted ? 2 : 1
        border.color: root.highlighted ? AppTheme.focus
                                       : (root.selected ? AppTheme.primary : AppTheme.outlineStrong)

        StateLayer { control: root }
    }
}
