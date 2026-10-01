pragma ComponentBehavior: Bound

import QtQuick
import Qt5Compat.GraphicalEffects
import "../Singletons"

Item {
    id: root

    property url source

    readonly property bool hasArtwork: _frame.ready

    readonly property int frameWidth:
        Math.min(width, Math.round(height / AppTheme.backdropAspectRatio))

    readonly property bool letterboxed: frameWidth < width

    clip: true

    Item {
        id: _blurSource

        anchors.fill: parent
        visible: false

        Image {
            anchors.centerIn: parent
            width: Math.round(parent.width * 1.25)
            height: Math.round(parent.height * 1.25)
            source: root.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
        }
    }

    FastBlur {
        anchors.fill: parent
        visible: root.letterboxed && root.hasArtwork
        source: _blurSource
        radius: Math.round(root.height * 0.10)
    }

    Rectangle {
        anchors.fill: parent
        visible: root.letterboxed && root.hasArtwork
        color: AppTheme.background
        opacity: 0.45
    }

    Item {
        id: _frameBox

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.frameWidth

        layer.enabled: root.letterboxed
        layer.effect: OpacityMask {
            maskSource: LinearGradient {
                width: _frameBox.width
                height: _frameBox.height
                start: Qt.point(0, 0)
                end: Qt.point(Math.round(_frameBox.width * 0.4), 0)

                gradient: Gradient {
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: "white" }
                }
            }
        }

        HeldImage {
            id: _frame

            anchors.fill: parent
            source: root.source
        }
    }
}
