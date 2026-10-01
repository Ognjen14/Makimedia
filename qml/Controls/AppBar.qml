pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    enum Leading {
        None,
        Menu,
        Back
    }

    property int leading: AppBar.None
    property string title
    property int titleLeftInset: 0

    default property alias actions: _actions.data

    readonly property bool isEmpty: title.length === 0
                                    && leading === AppBar.None
                                    && _actions.width <= 0

    signal leadingTriggered()

    implicitHeight: Math.max(AppTheme.touchTargetMinimum,
                             _actions.implicitHeight + 2 * AppTheme.spacing4)
    implicitWidth: 320
    clip: true

    IconButton {
        id: _leading

        anchors.left: parent.left
        anchors.leftMargin: AppTheme.spacing12
        anchors.verticalCenter: parent.verticalCenter
        visible: root.leading !== AppBar.None
        iconSource: root.leading === AppBar.Back ? Icons.chevronLeft : Icons.menu
        accessibleName: root.leading === AppBar.Back ? qsTr("Back") : qsTr("Open navigation")
        onClicked: root.leadingTriggered()
    }

    Text {
        anchors.left: _leading.visible ? _leading.right : parent.left
        anchors.leftMargin: _leading.visible
                            ? AppTheme.spacing12
                            : (root.titleLeftInset > 0 ? root.titleLeftInset : AppTheme.spacing12)
        anchors.right: _actions.left
        anchors.rightMargin: AppTheme.spacing12
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        color: AppTheme.textPrimary
        font.pixelSize: AppTheme.fs22
        font.weight: Font.Medium
        elide: Text.ElideRight
    }

    Row {
        id: _actions

        anchors.right: parent.right
        anchors.rightMargin: AppTheme.spacing12
        anchors.verticalCenter: parent.verticalCenter
        spacing: AppTheme.spacing4
    }
}
