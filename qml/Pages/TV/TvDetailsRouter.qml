pragma ComponentBehavior: Bound

import QtQuick
import "../../Singletons" as S
import com.topicdev.makimedia 1.0

Item {
    id: root

    property string handle: ""

    signal backRequested()
    signal playRequested(string handle)
    signal playFromStartRequested(string handle)
    signal detailsRequested(string handle)
    signal showRequested(var mediaId)
    signal drawerRequested()
    signal fixMatchRequested(string handle)

    property var metadata: ({})
    readonly property bool isEpisode: metadata.isEpisode === true

    readonly property bool acceptsFocus:
        _loader.item ? _loader.item.acceptsFocus === true : false

    function takeFocus() {
        if (_loader.item)
            _loader.item.takeFocus()
    }

    function refreshMetadata() {
        metadata = Metadata.metadataForFile(root.handle)
    }

    onHandleChanged: refreshMetadata()
    Component.onCompleted: refreshMetadata()

    Connections {
        target: Metadata

        function onMatchesChangedFor(fileHandles, mediaIds) {
            if (fileHandles.indexOf(root.handle) >= 0
                    || (root.metadata.mediaId !== undefined
                        && mediaIds.indexOf(root.metadata.mediaId) >= 0))
                root.refreshMetadata()
        }
    }

    Loader {
        id: _loader

        anchors.fill: parent
        sourceComponent: root.isEpisode ? _episodePage : _moviePage

        onLoaded: {
            if (S.AppTheme.remoteNavigation)
                root.takeFocus()
        }
    }

    Component {
        id: _moviePage

        TvMovieScreen {
            handle: root.handle
            metadata: root.metadata

            onBackRequested: root.backRequested()
            onPlayRequested: (handle) => root.playRequested(handle)
            onPlayFromStartRequested: (handle) => root.playFromStartRequested(handle)
            onDrawerRequested: root.drawerRequested()
            onFixMatchRequested: (handle) => root.fixMatchRequested(handle)
        }
    }

    Component {
        id: _episodePage

        TvEpisodeScreen {
            handle: root.handle
            metadata: root.metadata

            onBackRequested: root.backRequested()
            onPlayRequested: (handle) => root.playRequested(handle)
            onPlayFromStartRequested: (handle) => root.playFromStartRequested(handle)
            onDetailsRequested: (handle) => root.detailsRequested(handle)
            onShowRequested: (mediaId) => root.showRequested(mediaId)
            onDrawerRequested: root.drawerRequested()
        }
    }
}
