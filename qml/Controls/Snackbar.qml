pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Rectangle {
    id: root

    property string message
    property string actionText
    property int dismissAfterMs: 4000

    signal actionTriggered()

    function show(text, action) {
        message = text
        actionText = (action === undefined) ? "" : action
        opacity = 1
        _timer.restart()
    }

    function hide() {
        opacity = 0
        _timer.stop()
    }

    height: Math.max(AppTheme.controlHeightMedium,
                     _label.implicitHeight + 2 * AppTheme.spacing14)
    radius: AppTheme.radiusMedium
    color: AppTheme.surfaceRaised
    opacity: 0
    visible: opacity > 0.01

    Behavior on opacity {
        NumberAnimation { duration: 180 }
    }

    Timer {
        id: _timer

        interval: root.dismissAfterMs
        onTriggered: root.opacity = 0
    }

    Text {
        id: _label

        anchors.left: parent.left
        anchors.right: _action.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: AppTheme.spacing16
        anchors.rightMargin: AppTheme.spacing12
        text: root.message
        color: AppTheme.textPrimary
        font.pixelSize: AppTheme.fs13
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }

    Text {
        id: _action

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: AppTheme.spacing16
        visible: root.actionText.length > 0
        text: root.actionText
        color: AppTheme.primary
        font.pixelSize: AppTheme.fs13
        font.weight: Font.Medium

        MouseArea {
            anchors.fill: parent
            anchors.margins: -AppTheme.spacing8
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.actionTriggered()
                root.hide()
            }
        }
    }
}
