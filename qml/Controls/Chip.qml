pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property bool selected: false
    property bool compact: false

    readonly property int chipHeight: compact ? 26 : AppTheme.controlHeightSmall
    readonly property int labelSize: compact ? AppTheme.fs11 : AppTheme.fs12

    leftPadding: compact ? AppTheme.spacing10 : AppTheme.spacing14
    rightPadding: leftPadding

    implicitHeight: chipHeight
    implicitWidth: leftPadding + _label.implicitWidth + rightPadding

    checkable: true
    checked: selected

    Accessible.role: Accessible.RadioButton
    Accessible.name: text
    Accessible.checked: selected

    contentItem: Text {
        id: _label

        text: root.text
        color: root.selected ? AppTheme.onPrimaryStrong : AppTheme.textSecondary
        font.pixelSize: root.labelSize
        font.weight: root.selected ? Font.Medium : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    background: Rectangle {
        radius: AppTheme.radiusSmall
        color: root.selected ? AppTheme.primary : AppTheme.surfaceVariant
        border.width: root.visualFocus ? 2 : 0
        border.color: AppTheme.focus

        StateLayer { control: root }

        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
