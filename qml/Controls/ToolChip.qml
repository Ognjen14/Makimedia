pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    property bool active: false

    property real maxTextWidth: 0

    leftPadding: AppTheme.spacing12
    rightPadding: AppTheme.spacing12

    implicitHeight: AppTheme.controlHeightSmall
    implicitWidth: leftPadding + rightPadding
                   + (root.maxTextWidth > 0
                      ? Math.min(_label.implicitWidth, root.maxTextWidth)
                      : _label.implicitWidth)

    Accessible.role: Accessible.Button
    Accessible.name: text

    background: Rectangle {
        radius: AppTheme.radiusPill
        color: root.active ? AppTheme.primary : Qt.rgba(1, 1, 1, 0.1)
        border.width: root.activeFocus ? AppTheme.focusThickness : 0
        border.color: AppTheme.focus

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: root.down
                   ? Qt.rgba(0, 0, 0, 0.18)
                   : (root.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
        }
    }

    contentItem: Text {
        id: _label

        text: root.text
        color: root.active ? AppTheme.onPrimaryStrong : "#FFFFFF"
        opacity: root.enabled ? 1.0 : 0.45
        font.pixelSize: AppTheme.fs12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
