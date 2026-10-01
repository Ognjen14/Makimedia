pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Rectangle {
    id: root

    property real posterWidth: AppTheme.posterTileWidth
    property real wideWidth: AppTheme.wideTileWidth

    color: AppTheme.background

    component Bar: Rectangle {
        height: 14
        radius: 4
        color: AppTheme.surfaceVariant
    }

    component Tiles: Row {
        id: _tiles

        property real tileWidth: 0
        property real tileHeight: 0

        spacing: AppTheme.spacing10
        clip: true

        Repeater {
            model: 8

            delegate: Rectangle {
                width: _tiles.tileWidth
                height: _tiles.tileHeight
                radius: AppTheme.radiusMedium
                color: AppTheme.surfaceVariant
            }
        }
    }

    SequentialAnimation {
        running: root.visible
        loops: Animation.Infinite

        NumberAnimation {
            target: _shapes
            property: "opacity"
            from: 0.5
            to: 0.9
            duration: 700
            easing.type: Easing.InOutQuad
        }

        NumberAnimation {
            target: _shapes
            property: "opacity"
            from: 0.9
            to: 0.5
            duration: 700
            easing.type: Easing.InOutQuad
        }
    }

    MouseArea {
        anchors.fill: parent
    }

    Column {
        id: _shapes

        x: AppTheme.spacing16
        y: AppTheme.spacing16
        width: parent.width - 2 * AppTheme.spacing16
        spacing: AppTheme.spacing6

        Bar { width: 150 }

        Tiles {
            width: parent.width
            tileWidth: root.wideWidth
            tileHeight: root.wideWidth * AppTheme.backdropAspectRatio
        }

        Item { width: 1; height: AppTheme.spacing16 }

        Bar { width: 180 }

        Tiles {
            width: parent.width
            tileWidth: root.posterWidth
            tileHeight: root.posterWidth * AppTheme.posterAspectRatio
        }

        Item { width: 1; height: AppTheme.spacing16 }

        Bar { width: 120 }

        Tiles {
            width: parent.width
            tileWidth: root.posterWidth
            tileHeight: root.posterWidth * AppTheme.posterAspectRatio
        }
    }
}
