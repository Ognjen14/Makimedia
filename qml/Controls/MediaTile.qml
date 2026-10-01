pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../Singletons"

RemoteButton {
    id: root

    enum Shape {
        Poster,
        Wide
    }

    enum Badge {
        None,
        Neutral,
        Positive,
        Accent,
        Negative
    }

    property int shape: MediaTile.Poster
    property url artSource
    property bool artPortrait: false
    property string title
    property string meta
    property string badgeText
    property url badgeIcon
    property color badgeIconBackground: badgeBackground
    property color badgeIconForeground: badgeForeground
    property int badge: MediaTile.None

    readonly property bool hasBadgeIcon: badgeIcon.toString().length > 0
    property real progress: 0
    property bool unmatched: false
    property string placeholderText
    property bool highlighted: false

    readonly property bool hasArt: artSource.toString().length > 0
    readonly property bool showPlaceholderArt: unmatched && !hasArt

    readonly property real artAspect: shape === MediaTile.Wide
                                      ? AppTheme.backdropAspectRatio
                                      : AppTheme.posterAspectRatio

    readonly property bool topCropArt: artPortrait && artAspect < 1.0
    readonly property int defaultWidth: shape === MediaTile.Wide
                                        ? AppTheme.wideTileWidth
                                        : AppTheme.posterTileWidth

    readonly property color badgeBackground: {
        switch (badge) {
        case MediaTile.Positive:
            return AppTheme.successContainer
        case MediaTile.Accent:
            return AppTheme.primary
        case MediaTile.Negative:
            return AppTheme.errorContainer
        default:
            return AppTheme.surfaceRaised
        }
    }

    readonly property color badgeForeground: {
        switch (badge) {
        case MediaTile.Positive:
            return AppTheme.success
        case MediaTile.Accent:
            return AppTheme.onPrimaryStrong
        case MediaTile.Negative:
            return AppTheme.error
        default:
            return AppTheme.textSecondary
        }
    }

    readonly property bool focused:
        visualFocus || (highlighted && AppTheme.remoteNavigation)

    z: focused ? 1 : 0

    signal contextRequested()

    onPressAndHold: root.contextRequested()

    TapHandler {
        acceptedDevices: PointerDevice.Mouse
        acceptedButtons: Qt.RightButton
        onTapped: root.contextRequested()
    }

    implicitWidth: defaultWidth
    implicitHeight: AppTheme.focusHeadroom
                    + _art.height + AppTheme.spacing8
                    + _title.implicitHeight
                    + (meta.length > 0 ? 2 + _meta.implicitHeight : 0)

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: meta

    background: Item {}

    contentItem: Item {
        Item {
            id: _art

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: AppTheme.focusHeadroom
            height: width * root.artAspect

            transformOrigin: Item.Bottom
            scale: root.focused ? AppTheme.focusScale : 1.0

            Behavior on scale {
                NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
            }

            Rectangle {
                id: _artSurface

                anchors.fill: parent
                radius: AppTheme.radiusMedium
                color: root.showPlaceholderArt ? "transparent" : AppTheme.surfaceVariant

                RoundedClip {
                    anchors.fill: parent
                    radius: _artSurface.radius
                    clipping: root.hasArt

                    Image {
                        id: _artImage

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: root.topCropArt
                                ? width * AppTheme.posterAspectRatio
                                : parent.height
                        source: root.artSource
                        sourceSize.width: Math.ceil(width * Screen.devicePixelRatio / 64) * 64
                        sourceSize.height: Math.ceil(height * Screen.devicePixelRatio / 64) * 64
                        fillMode: root.topCropArt ? Image.PreserveAspectFit
                                                  : Image.PreserveAspectCrop
                        asynchronous: true
                        visible: status === Image.Ready
                    }
                }

                ThemedIcon {
                    anchors.centerIn: parent
                    width: root.shape === MediaTile.Wide ? 30 : 26
                    height: width
                    visible: root.showPlaceholderArt
                    source: Icons.movies
                    tintColor: AppTheme.textDisabled
                    showPlaceholder: false
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 2 * AppTheme.spacing6
                    visible: !root.unmatched && !root.hasArt
                    text: root.placeholderText.length > 0
                          ? root.placeholderText
                          : (root.shape === MediaTile.Wide
                             ? qsTr("backdrop") : qsTr("poster"))
                    color: AppTheme.textDisabled
                    font.pixelSize: AppTheme.fs10
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: AppTheme.spacing6
                    anchors.rightMargin: AppTheme.spacing6
                    anchors.bottomMargin: AppTheme.spacing6
                    height: AppTheme.seekBarHeight
                    visible: root.progress > 0
                    radius: AppTheme.radiusPill
                    color: Qt.rgba(0, 0, 0, 0.55)

                    Rectangle {
                        width: parent.width * Math.min(1, root.progress)
                        height: parent.height
                        radius: AppTheme.radiusPill
                        color: AppTheme.primary
                    }
                }
            }

            Loader {
                anchors.fill: parent
                z: -1
                active: root.showPlaceholderArt
                asynchronous: true

                sourceComponent: Canvas {
                    id: _hatch

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.fillStyle = AppTheme.surface
                        ctx.fillRect(0, 0, width, height)
                        ctx.strokeStyle = AppTheme.surfaceVariant
                        ctx.lineWidth = 8
                        for (let i = -height; i < width + height; i += 16) {
                            ctx.beginPath()
                            ctx.moveTo(i, 0)
                            ctx.lineTo(i + height, height)
                            ctx.stroke()
                        }
                    }

                    Connections {
                        target: AppTheme
                        function onDarkModeChanged() { _hatch.requestPaint() }
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: root.showPlaceholderArt
                radius: AppTheme.radiusMedium
                color: "transparent"
                border.width: 1
                border.color: AppTheme.outlineStrong
            }

            Rectangle {
                id: _badge

                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: AppTheme.spacing6
                visible: root.badge !== MediaTile.None
                         && (root.hasBadgeIcon || root.badgeText.length > 0)
                width: root.hasBadgeIcon ? 22 : _badgeLabel.implicitWidth + 14
                height: root.hasBadgeIcon ? 22 : _badgeLabel.implicitHeight + 4
                radius: AppTheme.radiusPill
                color: root.hasBadgeIcon ? root.badgeIconBackground : root.badgeBackground

                Text {
                    id: _badgeLabel

                    anchors.centerIn: parent
                    visible: !root.hasBadgeIcon
                    text: root.badgeText
                    color: root.badgeForeground
                    font.pixelSize: AppTheme.fs9
                    font.weight: Font.Medium
                }

                ThemedIcon {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    visible: root.hasBadgeIcon
                    source: root.badgeIcon
                    tintColor: root.badgeIconForeground
                    showPlaceholder: false
                }
            }

            FocusRing {
                active: root.visualFocus
                        || (root.highlighted && AppTheme.remoteNavigation)
                ringRadius: _artSurface.radius
            }

            StateLayer {
                control: root
                radius: AppTheme.radiusMedium
            }
        }

        Text {
            id: _title

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _art.bottom
            anchors.topMargin: AppTheme.spacing8
            text: root.title
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fs13
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text {
            id: _meta

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _title.bottom
            anchors.topMargin: 2
            visible: root.meta.length > 0
            text: root.meta
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fs11
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
