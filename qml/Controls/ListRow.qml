pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../Singletons"

RemoteButton {
    id: root

    enum Leading {
        None,
        Thumbnail,
        Icon
    }

    property bool highlighted: false
    property int leading: ListRow.None
    property url iconSource
    property url thumbnailSource
    property string title
    property string subtitle
    property string trailingText
    property bool monoTitle: false
    property real progress: 0
    property bool dimmed: false
    property bool iconSpinning: false

    readonly property int leadingWidth: {
        switch (leading) {
        case ListRow.Thumbnail:
            return 76
        case ListRow.Icon:
            return 44
        default:
            return 0
        }
    }

    signal contextRequested()

    onPressAndHold: root.contextRequested()

    TapHandler {
        acceptedDevices: PointerDevice.Mouse
        acceptedButtons: Qt.RightButton
        onTapped: root.contextRequested()
    }

    implicitHeight: Math.max(AppTheme.touchTargetMinimum,
                             _text.implicitHeight + 2 * AppTheme.spacing8)
    implicitWidth: 320

    Accessible.role: Accessible.Button
    Accessible.name: title

    background: Rectangle {
        radius: AppTheme.radiusSmall
        color: root.down
               ? AppTheme.pressed
               : (root.hovered ? AppTheme.hover : "transparent")
    }

    contentItem: Item {
        FocusRing {
            active: root.visualFocus
                    || (root.highlighted && AppTheme.remoteNavigation)
            ringRadius: AppTheme.radiusSmall
            z: 30
        }

        Rectangle {
            id: _thumb

            anchors.left: parent.left
            anchors.leftMargin: AppTheme.rowContentPadding
            anchors.verticalCenter: parent.verticalCenter
            width: root.leadingWidth
            height: 44
            visible: root.leading === ListRow.Thumbnail
            radius: AppTheme.radiusSmall
            color: AppTheme.surfaceVariant

            RoundedClip {
                anchors.fill: parent
                radius: _thumb.radius

                Image {
                    anchors.fill: parent
                    source: root.thumbnailSource
                    sourceSize.width: Math.ceil(width * Screen.devicePixelRatio / 64) * 64
                    sourceSize.height: Math.ceil(height * Screen.devicePixelRatio / 64) * 64
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: status === Image.Ready
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: AppTheme.seekBarHeight
                visible: root.progress > 0
                color: Qt.rgba(0, 0, 0, 0.45)

                Rectangle {
                    width: parent.width * Math.min(1, root.progress)
                    height: parent.height
                    color: AppTheme.primary
                }
            }
        }

        Rectangle {
            id: _iconHolder

            anchors.left: parent.left
            anchors.leftMargin: AppTheme.rowContentPadding
            anchors.verticalCenter: parent.verticalCenter
            width: root.leadingWidth
            height: 44
            visible: root.leading === ListRow.Icon
            radius: AppTheme.radiusPill
            color: AppTheme.primary

            ThemedIcon {
                id: _icon

                anchors.centerIn: parent
                width: 20
                height: 20
                source: root.iconSource
                tintColor: AppTheme.onPrimaryStrong
                showPlaceholder: false

                RotationAnimator on rotation {
                    running: root.iconSpinning && _icon.visible && root.visible
                    from: 0
                    to: 360
                    duration: 900
                    loops: Animation.Infinite
                }
            }
        }

        Column {
            id: _text

            anchors.left: parent.left
            anchors.leftMargin: AppTheme.rowContentPadding
                                + (root.leading === ListRow.None
                                   ? 0
                                   : root.leadingWidth + AppTheme.spacing12)
            anchors.right: _trailing.left
            anchors.rightMargin: AppTheme.spacing12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: root.title
                color: root.dimmed ? AppTheme.textDisabled : AppTheme.textPrimary
                font.pixelSize: root.monoTitle ? AppTheme.fs12 : AppTheme.fs14
                font.family: root.monoTitle ? AppTheme.monoFontFamily : AppTheme.fontFamily
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Text {
                width: parent.width
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs11
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }

        Text {
            id: _trailing

            anchors.right: parent.right
            anchors.rightMargin: AppTheme.rowContentPadding
            anchors.verticalCenter: parent.verticalCenter
            text: root.trailingText
            color: AppTheme.textDisabled
            font.pixelSize: AppTheme.fs12
        }
    }
}
