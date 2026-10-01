pragma ComponentBehavior: Bound

import QtQuick
import Qt5Compat.GraphicalEffects
import "../Singletons" as S
import "../Controls" as Ctrl

Item {
    id: root

    signal chooseFolderRequested()
    signal dismissed()

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
            text: qsTr("Makimedia")
            color: S.AppTheme.textPrimary
            font.pixelSize: S.AppTheme.fs28
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            text: qsTr("Plays what is already on your device. No account, no ads, nothing sent anywhere.")
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
            id: _choose

            width: parent.width
            text: qsTr("Choose a folder")
            variant: Ctrl.AppButton.Filled
            focus: true
            KeyNavigation.down: _later
            onClicked: root.chooseFolderRequested()
        }

        Ctrl.AppButton {
            id: _later

            width: parent.width
            text: qsTr("Not now")
            variant: Ctrl.AppButton.Plain
            size: Ctrl.AppButton.Medium
            KeyNavigation.up: _choose
            onClicked: root.dismissed()
        }

        Text {
            width: parent.width
            text: qsTr("Makimedia only reads the folders you pick. It never asks for full storage access.")
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs11
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    Component.onCompleted: _choose.forceActiveFocus()
}
