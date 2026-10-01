pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property url artSource
    property string title
    property string meta
    property bool stacked: false
    property int newCount: 0
    property bool watched: false
    property real progress: 0
    property bool highlighted: false

    readonly property int stackStep: 4
    readonly property int tileWidth: width - 2 * stackStep
    readonly property real artHeight: tileWidth * AppTheme.posterAspectRatio

    signal clicked()
    signal contextRequested()

    implicitWidth: AppTheme.posterTileWidth
    implicitHeight: _tile.implicitHeight + 2 * stackStep
    z: _tile.focused ? 1 : 0

    Repeater {
        model: root.stacked ? 2 : 0

        delegate: Rectangle {
            required property int index

            readonly property int depth: 2 - index

            x: depth * root.stackStep
            y: AppTheme.focusHeadroom + (2 - depth) * root.stackStep
            width: root.tileWidth
            height: root.artHeight
            radius: AppTheme.radiusMedium
            color: depth === 2 ? AppTheme.surface : AppTheme.surfaceRaised
            border.width: 1
            border.color: AppTheme.outline
            opacity: _tile.focused ? 0 : 1
        }
    }

    MediaTile {
        id: _tile

        y: 2 * root.stackStep
        width: root.tileWidth
        shape: MediaTile.Poster
        artSource: root.artSource
        title: root.title
        meta: root.meta
        progress: root.progress
        highlighted: root.highlighted
        badge: root.newCount > 0 ? MediaTile.Accent
                                 : (root.watched ? MediaTile.Positive : MediaTile.None)
        badgeText: root.newCount > 0 ? qsTr("%n new", "", root.newCount)
                                     : (root.watched ? qsTr("Seen") : "")

        onClicked: root.clicked()
        onContextRequested: root.contextRequested()
    }
}
