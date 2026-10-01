pragma ComponentBehavior: Bound

import QtQuick
import Qt5Compat.GraphicalEffects
import "../../Singletons" as S
import "../../Controls" as Ctrl
import ".." as Pages

Item {
    id: root

    property url source
    property real aspect: 16 / 9

    readonly property bool hasArtwork: _image.ready

    implicitHeight: Math.round(width / Math.max(0.1, aspect))

    Rectangle {
        id: _frame

        anchors.fill: parent
        radius: S.AppTheme.radiusLarge

        layer.enabled: true
        layer.effect: DropShadow {
            transparentBorder: true
            horizontalOffset: 0
            verticalOffset: Math.min(20, Math.round(root.height * 0.045))
            radius: Math.min(32, Math.round(root.height * 0.10))
            samples: 25
            color: Qt.rgba(0, 0, 0, 0.62)
        }

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: S.AppTheme.surfaceVariant }
            GradientStop { position: 1.0; color: S.AppTheme.surface }
        }

        Ctrl.RoundedClip {
            anchors.fill: parent
            radius: _frame.radius

            Pages.HeldImage {
                id: _image

                anchors.fill: parent
                source: root.source
                visible: root.hasArtwork
            }
        }
    }

    Rectangle {
        anchors.fill: _frame
        radius: _frame.radius
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.10)
    }
}
