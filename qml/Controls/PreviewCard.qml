pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"
import com.topicdev.makimedia 1.0

Item {
    id: root

    property int playLimitMs: 30000
    property int soundVolume: 50

    Component.onCompleted: PreviewPlayer.volume = root.soundVolume

    Binding {
        target: PreviewPlayer
        property: "muted"
        value: !AppSettings.previewSound
    }

    property var info: ({})
    property bool open: false
    property bool videoShown: false
    property int _ticks: 0
    property rect anchorRect: Qt.rect(0, 0, 0, 0)

    function artOf(tile) {
        return tile && tile.artItem ? tile.artItem : tile
    }

    function measureAnchor() {
        const art = root.artOf(HoverPreview.anchor)
        if (!art)
            return Qt.rect(0, 0, 0, 0)
        const p = art.mapToItem(root, 0, 0)
        return Qt.rect(p.x, p.y, art.width, art.height)
    }

    function start() {
        _limit.stop()
        root.videoShown = false
        root._ticks = 0
        if (!HoverPreview.anchor || !HoverPreview.mediaId) {
            root.finish()
            return
        }
        const found = Library.previewFor(HoverPreview.mediaId)
        if (!found.url) {
            HoverPreview.close()
            return
        }
        root.info = found
        root.anchorRect = root.measureAnchor()
        root.open = true
        PreviewPlayer.setStartForNextOpen(found.start)
        PreviewPlayer.openPath(found.url)
    }

    function finish() {
        _limit.stop()
        _watch.stop()
        if (root.open)
            PreviewPlayer.stop()
        root.open = false
        root.videoShown = false
    }

    Connections {
        target: HoverPreview

        function onMediaIdChanged() {
            if (HoverPreview.mediaId)
                root.start()
            else
                root.finish()
        }

        function onAnchorChanged() {
            if (!HoverPreview.anchor)
                root.finish()
        }
    }

    Connections {
        target: PreviewPlayer
        enabled: root.open && !root.videoShown

        function onPositionChanged() {
            if (!PreviewPlayer.fileLoaded)
                return
            root._ticks += 1
            if (root._ticks >= 2) {
                root.videoShown = true
                _limit.restart()
            }
        }
    }

    Timer {
        id: _limit

        interval: root.playLimitMs
        onTriggered: {
            root.videoShown = false
            PreviewPlayer.stop()
        }
    }

    Timer {
        id: _watch

        interval: 250
        repeat: true
        running: root.open
        onTriggered: {
            const art = root.artOf(HoverPreview.anchor)
            if (!art || !art.visible) {
                HoverPreview.close()
                return
            }
            const now = root.measureAnchor()
            if (Math.abs(now.x - root.anchorRect.x) > 2 || Math.abs(now.y - root.anchorRect.y) > 2)
                HoverPreview.close()
        }
    }

    Item {
        id: _card

        readonly property real videoHeight: Math.round(width * 9 / 16)

        width: Math.round(Math.min(941, Math.max(627, root.anchorRect.width * 4.7),
                                   root.width - 2 * AppTheme.spacing12,
                                   (root.height - 2 * AppTheme.spacing12 - 64) * 16 / 9))
        height: videoHeight + _caption.height
        x: Math.round(Math.max(AppTheme.spacing12,
                               Math.min(root.width - width - AppTheme.spacing12,
                                        root.anchorRect.x + root.anchorRect.width / 2
                                        - width / 2)))
        y: Math.round(Math.max(AppTheme.spacing12,
                               Math.min(root.height - height - AppTheme.spacing12,
                                        root.anchorRect.y + root.anchorRect.height / 2
                                        - height / 2)))

        visible: opacity > 0
        opacity: root.open ? 1 : 0
        scale: root.open ? 1 : 0.94

        Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -1
            radius: AppTheme.radiusMedium + 1
            color: AppTheme.outline
        }

        Rectangle {
            id: _surface

            anchors.fill: parent
            radius: AppTheme.radiusMedium
            color: AppTheme.surfaceRaised
            clip: true

            Item {
                id: _video

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: _card.videoHeight
                clip: true

                Rectangle {
                    anchors.fill: parent
                    color: "black"
                }

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: true
                    source: root.info.backdropPath
                            ? Metadata.backdropUrl(root.info.backdropPath, 780) : ""
                }

                Loader {
                    anchors.fill: parent
                    active: _card.visible

                    sourceComponent: MpvRenderItem {
                        controller: PreviewPlayer
                        opacity: root.videoShown ? 1 : 0

                        Behavior on opacity { NumberAnimation { duration: 220 } }
                    }
                }

                IconButton {
                    id: _sound

                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: AppTheme.spacing8
                    compact: true
                    scrim: true
                    focusPolicy: Qt.NoFocus
                    iconSource: AppSettings.previewSound ? Icons.volume : Icons.volumeOff
                    tintColor: "white"
                    accessibleName: AppSettings.previewSound ? qsTr("Mute preview")
                                                             : qsTr("Play preview sound")
                    onClicked: AppSettings.previewSound = !AppSettings.previewSound
                }
            }

            Column {
                id: _caption

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: _video.bottom
                topPadding: AppTheme.spacing8
                bottomPadding: AppTheme.spacing10
                leftPadding: AppTheme.spacing12
                rightPadding: AppTheme.spacing12
                spacing: 2

                Text {
                    width: parent.width - parent.leftPadding - parent.rightPadding
                    text: root.info.title || ""
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fs15
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width - parent.leftPadding - parent.rightPadding
                    text: {
                        const parts = []
                        if (root.info.year > 0)
                            parts.push(String(root.info.year))
                        if (root.info.episode)
                            parts.push(root.info.episode)
                        return parts.join("  ·  ")
                    }
                    visible: text.length > 0
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fs12
                    elide: Text.ElideRight
                }
            }
        }

        HoverHandler {
            enabled: root.open
            onHoveredChanged: HoverPreview.setCardHovered(hovered)
        }

        TapHandler {
            enabled: root.open
            acceptedButtons: Qt.LeftButton
            onTapped: {
                if (_sound.hovered)
                    return
                const tile = HoverPreview.anchor
                HoverPreview.close()
                if (tile)
                    tile.clicked()
            }
        }

        TapHandler {
            enabled: root.open
            acceptedButtons: Qt.RightButton
            onTapped: {
                const tile = HoverPreview.anchor
                HoverPreview.close()
                if (tile)
                    tile.contextRequested()
            }
        }

        WheelHandler {
            enabled: root.open
            onWheel: HoverPreview.close()
        }
    }
}
