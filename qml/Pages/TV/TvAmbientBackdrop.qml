pragma ComponentBehavior: Bound

import QtQuick
import Qt5Compat.GraphicalEffects
import "../../Singletons" as S

Item {
    id: root

    property url source

    property bool topScrim: false

    readonly property bool hasArtwork: _image.status === Image.Ready

    anchors.fill: parent
    visible: hasArtwork

    Image {
        id: _image

        anchors.fill: parent
        source: root.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: false
    }

    Item {
        id: _blurSource

        anchors.fill: parent
        visible: false

        Image {
            anchors.centerIn: parent
            width: parent.width * 1.25
            height: parent.height * 1.25
            source: root.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
        }
    }

    FastBlur {
        anchors.fill: parent
        source: _blurSource
        radius: Math.round(root.height * 0.064)
        visible: root.hasArtwork
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
        opacity: 0.34
    }

    Rectangle {
        anchors.fill: parent
        visible: root.topScrim
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                               S.AppTheme.background.b, 0.78)
            }
            GradientStop {
                position: 0.30
                color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                               S.AppTheme.background.b, 0.55)
            }
            GradientStop {
                position: 0.52
                color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                               S.AppTheme.background.b, 0.0)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                               S.AppTheme.background.b, 0.0)
            }
            GradientStop {
                position: 0.55
                color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                               S.AppTheme.background.b, 0.55)
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                               S.AppTheme.background.b, 0.95)
            }
        }
    }
}
