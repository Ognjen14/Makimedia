pragma ComponentBehavior: Bound

import QtQuick
import Qt5Compat.GraphicalEffects
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal grantRequested()
    signal settingsRequested()
    signal skipRequested()

    readonly property bool blocked: System.videoPermissionBlocked
    readonly property bool partial: System.videoAccessPartial

    readonly property bool canSkip: true

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 380)
        spacing: S.AppTheme.spacing12

        Item {
            id: _appIconBox

            anchors.horizontalCenter: parent.horizontalCenter
            width: 88
            height: 88

            Image {
                id: _appIconImage

                anchors.fill: parent
                source: S.Icons.appIcon
                sourceSize.width: 176
                sourceSize.height: 176
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: false
            }

            Rectangle {
                id: _appIconMask

                anchors.fill: parent
                radius: S.AppTheme.radiusLarge
                color: S.AppTheme.textPrimary
                visible: false
            }

            OpacityMask {
                anchors.fill: parent
                source: _appIconImage
                maskSource: _appIconMask
            }
        }

        Item {
            width: 1
            height: S.AppTheme.spacing4
        }

        Text {
            width: parent.width
            text: qsTr("Makimedia needs access to your videos")
            color: S.AppTheme.textPrimary
            font.pixelSize: S.AppTheme.fs20
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width
            text: {
                if (root.partial)
                    return qsTr("Only some videos were allowed. Makimedia builds its library from every video on this device, so it needs access to all of them.")
                if (root.blocked)
                    return qsTr("Android will not show the permission dialog again, because it was declined more than once. It can still be turned on from this app's settings page.")
                return qsTr("Makimedia only reads video files from this device. Nothing is uploaded and nothing is sent anywhere. If your films live on a PC, you can carry on without this and stream from it instead.")
            }
            color: S.AppTheme.textSecondary
            font.pixelSize: S.AppTheme.fs13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Item {
            width: 1
            height: S.AppTheme.spacing12
        }

        Ctrl.AppButton {
            id: _grant

            width: parent.width
            text: root.blocked
                  ? qsTr("Open app settings")
                  : qsTr("Allow access to videos")
            variant: Ctrl.AppButton.Filled
            focus: true
            onClicked: {
                if (root.blocked)
                    root.settingsRequested()
                else
                    root.grantRequested()
            }
            KeyNavigation.down: root.canSkip ? _skip : null
        }

        Ctrl.AppButton {
            id: _skip

            width: parent.width
            visible: root.canSkip
            text: root.partial
                  ? qsTr("Continue with the allowed videos")
                  : qsTr("Continue without them")
            variant: Ctrl.AppButton.Outlined
            onClicked: root.skipRequested()
            KeyNavigation.up: _grant
        }

        Text {
            width: parent.width
            visible: root.blocked
            text: root.partial
                  ? qsTr("Permissions, then Photos and videos, then Always allow all. Makimedia picks it up when you come back.")
                  : qsTr("Permissions, then Videos, then Allow. Makimedia picks it up when you come back.")
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs11
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    readonly property bool acceptsFocus: true

    function takeFocus() {
        _grant.forceActiveFocus(Qt.TabFocusReason)
        return true
    }

    Connections {
        target: System

        function onVideoPermissionChanged() {
            Qt.callLater(root.takeFocus)
        }
    }

    Connections {
        target: Qt.application

        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive)
                Qt.callLater(root.takeFocus)
        }
    }

    Component.onCompleted: root.takeFocus()
}
