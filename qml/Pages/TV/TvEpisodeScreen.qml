pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Singletons" as S
import "../../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    property string handle: ""
    property var info: ({})
    property var metadata: ({})
    property var nextEpisode: ({})
    property var attachedSubtitles: []

    signal backRequested()
    signal playRequested(string handle)
    signal playFromStartRequested(string handle)
    signal detailsRequested(string handle)
    signal showRequested(var mediaId)
    signal drawerRequested()

    readonly property bool acceptsFocus: true

    function takeFocus() {
        _play.forceActiveFocus(Qt.TabFocusReason)
    }

    readonly property int safeX: Math.round(width * 0.045)
    readonly property int safeY: Math.round(height * 0.055)
    readonly property int contentWidth: width - 2 * safeX

    readonly property int titleSize: Math.round(height * 0.062)
    readonly property int actionHeight: Math.round(height * 0.066)

    readonly property int cardWidth: Math.round(contentWidth * 0.52)
    readonly property int dockWidth: Math.round(contentWidth * 0.19)

    readonly property int textWidth:
        Math.round(contentWidth - dockWidth - S.AppTheme.spacing32)
    readonly property int overviewHeight: Math.round(S.AppTheme.fs15 * 1.35 * 2)

    readonly property bool matched: (metadata.title || "").length > 0

    readonly property string shownTitle: {
        if (!matched)
            return info.displayName || ""
        return (metadata.episodeTitle || "").length > 0
               ? metadata.episodeTitle
               : qsTr("Episode %1").arg(metadata.episode || 0)
    }

    readonly property string kickerText:
        matched ? qsTr("%1  ·  Season %2  ·  Episode %3")
                  .arg(metadata.title || "")
                  .arg(metadata.season || 0)
                  .arg(metadata.episode || 0)
                : ""

    readonly property bool indexed: info.indexed === true
    readonly property bool finished: info.finished === true
    readonly property real positionSeconds:
        info.positionSeconds === undefined ? 0 : info.positionSeconds
    readonly property real progress:
        info.progress === undefined ? 0 : Math.max(0, Math.min(1, info.progress))
    readonly property real resumeSeconds:
        info.resumeSeconds === undefined ? 0 : info.resumeSeconds
    readonly property bool partiallyWatched: !finished && resumeSeconds > 0

    readonly property bool hasNextEpisode: (nextEpisode.handle || "").length > 0

    readonly property url cardSource: {
        void Metadata.artworkRevision
        if ((metadata.episodeStillPath || "").length > 0)
            return Metadata.backdropUrl(metadata.episodeStillPath, 1280)
        if ((metadata.backdropPath || "").length > 0)
            return Metadata.backdropUrl(metadata.backdropPath, 1280)
        return ""
    }

    readonly property url ambientSource: {
        void Metadata.artworkRevision
        if ((metadata.episodeStillPath || "").length > 0)
            return Metadata.backdropUrl(metadata.episodeStillPath, 780)
        if ((metadata.backdropPath || "").length > 0)
            return Metadata.backdropUrl(metadata.backdropPath, 780)
        return ""
    }

    function value(key) {
        return info[key] === undefined ? "" : info[key]
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    TvAmbientBackdrop {
        topScrim: true
        source: root.ambientSource
    }

    TvCastStrip {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: root.safeX
        anchors.topMargin: root.safeY

        faceSize: Math.round(root.height * 0.118)
        crewNameWidth: 130
        cast: root.metadata.cast || []
        directors: root.metadata.directors || ""
        writers: root.metadata.writers || ""
    }

    TvArtworkCard {
        id: _card

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: root.safeX
        anchors.topMargin: root.safeY

        width: root.cardWidth
        height: implicitHeight

        source: root.cardSource
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.safeX
        anchors.rightMargin: root.safeX
        anchors.topMargin: root.safeY
        anchors.bottomMargin: root.safeY
        spacing: S.AppTheme.spacing16

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: _card.height
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: S.AppTheme.spacing32

            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: root.textWidth
                Layout.alignment: Qt.AlignBottom
                spacing: S.AppTheme.spacing8

                Text {
                    Layout.maximumWidth: root.textWidth
                    text: root.kickerText
                    visible: text.length > 0
                    color: S.AppTheme.primary
                    font.pixelSize: S.AppTheme.fs16
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    Layout.maximumWidth: root.textWidth
                    text: root.shownTitle
                    color: S.AppTheme.textPrimary
                    font.pixelSize: root.titleSize
                    font.weight: Font.Bold
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Math.round(root.titleSize * 0.5)
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    Layout.maximumWidth: root.textWidth
                    text: {
                        const bits = []
                        if ((metadata.episodeRuntimeMinutes || 0) > 0)
                            bits.push(S.Format.runtime(metadata.episodeRuntimeMinutes))
                        else if (root.value("durationText").length > 0)
                            bits.push(root.value("durationText"))
                        if ((metadata.episodeAirDate || "").length > 0)
                            bits.push(metadata.episodeAirDate)

                        const scan = S.Format.scanLine(info.width || 0, info.height || 0)
                        const codec = root.value("videoCodec")
                        if (scan.length > 0 && codec.length > 0)
                            bits.push(scan + " " + codec.split("/")[0].trim())

                        return bits.join("  ·  ")
                    }
                    visible: text.length > 0
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs18
                    elide: Text.ElideRight
                }

                Text {
                    Layout.preferredWidth: root.textWidth
                    Layout.maximumWidth: root.textWidth
                    Layout.preferredHeight: root.overviewHeight

                    text: metadata.episodeOverview || metadata.overview || ""
                    visible: text.length > 0
                    color: S.AppTheme.textSecondary

                    fontSizeMode: Text.Fit
                    font.pixelSize: S.AppTheme.fs15
                    minimumPixelSize: S.AppTheme.fs11

                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    lineHeight: 1.35
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: Math.round(root.textWidth * 0.6)
                    spacing: S.AppTheme.spacing10
                    visible: root.partiallyWatched

                    Text {
                        text: S.Format.clock(root.positionSeconds)
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                    }

                    Ctrl.LinearProgress {
                        Layout.fillWidth: true
                        value: root.progress
                    }

                    Text {
                        text: "-" + S.Format.clock(
                            Math.max(0, (root.info.durationSeconds || 0)
                                        - root.positionSeconds))
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                    }
                }

                Item {
                    Layout.topMargin: S.AppTheme.spacing8
                    Layout.preferredWidth: root.textWidth
                    Layout.preferredHeight: _actions.height

                    Row {
                        id: _actions

                        spacing: S.AppTheme.spacing12

                        transformOrigin: Item.Left
                        scale: implicitWidth > root.textWidth
                               ? root.textWidth / implicitWidth
                               : 1

                    Ctrl.AppButton {
                        id: _play

                        height: root.actionHeight
                        text: root.finished
                              ? qsTr("Play again")
                              : (root.partiallyWatched ? qsTr("Resume")
                                                       : qsTr("Play"))
                        variant: Ctrl.AppButton.Filled
                        iconSource: S.Icons.play
                        focus: true

                        KeyNavigation.right: root.partiallyWatched
                                             ? _fromStart : _watched
                        Keys.onLeftPressed: root.drawerRequested()
                        onClicked: root.playRequested(root.handle)
                    }

                    Ctrl.AppButton {
                        id: _fromStart

                        height: root.actionHeight
                        visible: root.partiallyWatched
                        text: qsTr("From start")
                        variant: Ctrl.AppButton.Outlined

                        KeyNavigation.left: _play
                        KeyNavigation.right: _watched
                        onClicked: root.playFromStartRequested(root.handle)
                    }

                    Ctrl.AppButton {
                        id: _watched

                        height: root.actionHeight
                        text: root.finished ? qsTr("Unwatched") : qsTr("Watched")
                        variant: Ctrl.AppButton.Outlined
                        iconSource: root.finished ? "" : S.Icons.check
                        enabled: root.indexed

                        KeyNavigation.left: root.partiallyWatched ? _fromStart : _play
                        KeyNavigation.right: _subtitles.visible ? _subtitles : _allEpisodes
                        onClicked: {
                            Library.setWatched(root.info.fileId, !root.finished)
                            root.refresh()
                        }
                    }

                    Ctrl.AppButton {
                        id: _subtitles

                        height: root.actionHeight
                        text: qsTr("Subtitles")
                        variant: Ctrl.AppButton.Outlined
                        visible: !Streaming.connected

                        KeyNavigation.left: _watched
                        KeyNavigation.right: _allEpisodes
                        onClicked: _subtitleSheet.open()
                    }

                    Ctrl.AppButton {
                        id: _allEpisodes

                        height: root.actionHeight
                        text: qsTr("All episodes")
                        variant: Ctrl.AppButton.Outlined
                        enabled: (root.info.mediaId || 0) > 0

                        KeyNavigation.left: _subtitles.visible ? _subtitles : _watched
                        KeyNavigation.right: root.hasNextEpisode ? _nextDock : null
                        onClicked: root.showRequested(root.info.mediaId)
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: root.dockWidth
                Layout.minimumWidth: root.dockWidth
                Layout.alignment: Qt.AlignBottom
                spacing: S.AppTheme.spacing8
                visible: root.hasNextEpisode

                Ctrl.SectionLabel {
                    text: qsTr("Next episode")
                    topPadding: 0
                }

                Item {
                    id: _nextDock

                    Layout.fillWidth: true
                    Layout.preferredHeight: _nextColumn.implicitHeight

                    activeFocusOnTab: true

                    Keys.onPressed: (event) => {
                        if (event.key === Qt.Key_Select
                                || event.key === Qt.Key_Return
                                || event.key === Qt.Key_Enter) {
                            root.detailsRequested(root.nextEpisode.handle)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Left) {
                            _allEpisodes.forceActiveFocus(Qt.TabFocusReason)
                            event.accepted = true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.detailsRequested(root.nextEpisode.handle)
                    }

                    Column {
                        id: _nextColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        spacing: S.AppTheme.spacing6

                        Item {
                            width: parent.width
                            height: Math.round(width * 9 / 16)

                            Rectangle {
                                id: _nextSurface

                                anchors.fill: parent
                                radius: S.AppTheme.radiusMedium
                                color: S.AppTheme.surfaceVariant
                            }

                            Ctrl.RoundedClip {
                                anchors.fill: parent
                                radius: _nextSurface.radius

                                Image {
                                    anchors.fill: parent
                                    source: {
                                        void Metadata.artworkRevision
                                        return Metadata.backdropUrl(
                                            root.nextEpisode.stillPath || "", 780)
                                    }
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    visible: (root.nextEpisode.stillPath || "").length > 0
                                }
                            }

                            Ctrl.FocusRing {
                                active: _nextDock.activeFocus
                                ringRadius: _nextSurface.radius
                            }
                        }

                        Text {
                            width: parent.width
                            text: qsTr("%1. %2")
                                  .arg(root.nextEpisode.episode || 0)
                                  .arg(root.nextEpisode.title || "")
                            color: S.AppTheme.textPrimary
                            font.pixelSize: S.AppTheme.fs14
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            lineHeight: 1.15
                        }

                        Text {
                            width: parent.width
                            text: {
                                const bits = []
                                if ((root.nextEpisode.runtimeMinutes || 0) > 0)
                                    bits.push(S.Format.runtime(root.nextEpisode.runtimeMinutes))
                                if ((root.nextEpisode.airDate || "").length > 0)
                                    bits.push(root.nextEpisode.airDate)
                                return bits.join("  ·  ")
                            }
                            color: S.AppTheme.textSecondary
                            font.pixelSize: S.AppTheme.fs12
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    TvSubtitleSheet {
        id: _subtitleSheet

        z: 50
        handle: root.handle

        onClosed: _subtitles.forceActiveFocus(Qt.TabFocusReason)
    }

    function refresh() {
        if (handle.length > 0)
            info = Library.fileInfo(handle)
    }

    function refreshNextEpisode() {
        nextEpisode = Metadata.nextEpisodeFor(root.handle)
    }

    function refreshAttachedSubtitle() {
        attachedSubtitles = Library.attachedSubtitles(root.handle)
    }

    onHandleChanged: {
        refresh()
        refreshNextEpisode()
        refreshAttachedSubtitle()
        takeFocus()
    }

    Component.onCompleted: {
        refresh()
        refreshNextEpisode()
        refreshAttachedSubtitle()
    }

    Connections {
        target: Library

        function onFileCountChanged() { root.refresh() }

        function onSubtitleAttached(fileHandle, displayName) {
            if (fileHandle === root.handle)
                root.refreshAttachedSubtitle()
        }
    }

    Connections {
        target: Metadata

        function onMatchesChangedFor(fileHandles, mediaIds) {
            if (fileHandles.indexOf(root.handle) >= 0
                    || (root.metadata.mediaId !== undefined
                        && mediaIds.indexOf(root.metadata.mediaId) >= 0))
                root.refreshNextEpisode()
        }
    }

}
