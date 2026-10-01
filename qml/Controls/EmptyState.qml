pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

Item {
    id: root

    property url iconSource
    property string title
    property string message
    property string actionText
    property bool busy: false

    signal actionTriggered()

    implicitWidth: 320
    implicitHeight: _column.implicitHeight + 2 * AppTheme.spacing32

    Accessible.role: Accessible.StaticText
    Accessible.name: title
    Accessible.description: message

    Column {
        id: _column

        anchors.centerIn: parent
        width: parent.width - 2 * AppTheme.spacing20
        spacing: AppTheme.spacing12

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 88
            height: 88
            radius: AppTheme.radiusPill
            color: AppTheme.primary

            ThemedIcon {
                anchors.centerIn: parent
                width: 38
                height: 38
                visible: !root.busy
                source: root.iconSource
                tintColor: AppTheme.onPrimaryStrong
                showPlaceholder: false
            }

            BusyIndicator {
                anchors.centerIn: parent
                width: 38
                height: 38
                running: root.busy
                visible: running
            }
        }

        Item {
            width: 1
            height: AppTheme.spacing4
        }

        Text {
            width: parent.width
            text: root.title
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fs18
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width
            visible: text.length > 0
            text: root.message
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fs13
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        AppButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.actionText.length > 0
            text: root.actionText
            variant: AppButton.Filled
            onClicked: root.actionTriggered()
        }
    }
}
