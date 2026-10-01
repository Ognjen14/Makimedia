pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../../Singletons" as S
import "../../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    property var mediaId: 0
    readonly property var show: Library.showEpisodes
    readonly property var info: show.info
    readonly property int season: show.season

    signal backRequested()
    signal playRequested(string handle)
    signal detailsRequested(string handle)
    signal drawerRequested()

    readonly property bool acceptsFocus: true

    function takeFocus() {
        if (_continue.visible)
            _continue.forceActiveFocus(Qt.TabFocusReason)
        else if (_rail.count > 0)
            _rail.forceActiveFocus(Qt.TabFocusReason)
        else if (_seasons.count > 1)
            _seasons.forceActiveFocus(Qt.TabFocusReason)
    }

    readonly property int safeX: Math.round(width * 0.045)
    readonly property int safeY: Math.round(height * 0.055)
    readonly property int contentWidth: width - 2 * safeX

    readonly property int titleSize: Math.round(height * 0.060)
    readonly property int actionHeight: Math.round(height * 0.062)

    readonly property int cardWidth: Math.round(contentWidth * 0.38)
    readonly property bool hasSeasonPoster:
        (show.seasonPosterPath || "").length > 0
    readonly property int heroTextWidth:
        contentWidth - cardWidth - Math.round(contentWidth * 0.034)

    readonly property int episodeWidth: Math.round(contentWidth * 0.164)
    readonly property int stillHeight: Math.round(episodeWidth * 9 / 16)

    readonly property int episodeWidthFocused: Math.round(episodeWidth * 1.34)
    readonly property int stillHeightFocused:
        Math.round(episodeWidthFocused * 9 / 16)

    readonly property int episodeActionWidth:
        Math.round((episodeWidthFocused - S.AppTheme.spacing8) / 2)

    readonly property int railHeight:
        stillHeightFocused
        + 3 * S.AppTheme.spacing8
        + Math.round(S.AppTheme.fs15 * 1.4)
        + Math.round(S.AppTheme.fs13 * 1.4)
        + actionHeight

    readonly property var seasons: show.seasons

    readonly property var nextUp: show.hasNextUp ? show.nextUp : null

    readonly property bool seasonWatched: show.seasonWatched

    function setSeasonWatched(watched) {
        Library.setAllWatched(show.seasonFileHandles(), watched)
    }

    onMediaIdChanged: {
        show.mediaId = root.mediaId
        Metadata.ensureSeasons(root.mediaId)
    }

    Component.onCompleted: {
        show.mediaId = root.mediaId
        Metadata.ensureSeasons(root.mediaId)
    }

    Connections {
        target: Metadata

        function onCandidatesReady(candidates) {
            if (!_fixSheet.opened)
                return
            root.fixSearching = false
            root.fixCandidates = candidates
        }
    }

    property var fixCandidates: []
    property bool fixSearching: false

    function fileHandles() {
        return show.fileHandles()
    }

    function openFixMatch() {
        const handles = root.fileHandles()
        if (handles.length === 0)
            return
        root.fixCandidates = []
        const guess = Library.parsedNameFor(handles[0])
        _fixSheet.beginWith(guess.title || root.info.title || "", true)
    }

    Ctrl.FixMatchSheet {
        id: _fixSheet

        searchTv: true
        currentTmdbId: root.info.tmdbId || 0
        candidates: root.fixCandidates
        searching: root.fixSearching

        onSearchRequested: (text, tv) => {
            root.fixSearching = true
            Metadata.searchCandidates(text, tv)
        }
        onPicked: (candidate) => {
            const pinned = Metadata.pinMatchForShow(root.fileHandles(), candidate)
            if (pinned > 0 && candidate.tmdbId !== root.info.tmdbId)
                root.backRequested()
        }
        onClosed: Qt.callLater(root.takeFocus)
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    TvAmbientBackdrop {
        source: {
            void Metadata.artworkRevision
            return Metadata.backdropUrl(root.show.seasonBackdropPath
                                        || root.info.backdropPath || "", 780)
        }
    }

    TvArtworkCard {
        id: _card

        anchors.right: parent.right
        anchors.rightMargin: root.safeX

        anchors.top: parent.top
        anchors.topMargin: root.safeY

        readonly property int room:
            _bottom.y + _seasons.height + _bottom.spacing
            - root.safeY - S.AppTheme.spacing16

        aspect: root.hasSeasonPoster ? (1.0 / S.AppTheme.posterAspectRatio)
                                     : (16.0 / 9.0)
        width: root.hasSeasonPoster
               ? Math.min(root.cardWidth,
                          Math.round(room / S.AppTheme.posterAspectRatio))
               : root.cardWidth
        height: implicitHeight

        source: {
            void Metadata.artworkRevision
            if (root.hasSeasonPoster)
                return Metadata.posterUrl(root.show.seasonPosterPath, 500)
            return Metadata.backdropUrl(root.info.backdropPath || "", 1280)
        }
    }

    ColumnLayout {
        id: _hero

        anchors.left: parent.left
        anchors.bottom: _bottom.top
        anchors.leftMargin: root.safeX
        anchors.bottomMargin: S.AppTheme.spacing16
        width: root.heroTextWidth
        spacing: S.AppTheme.spacing10

        Text {
            Layout.fillWidth: true
            text: root.info.title || ""
            color: S.AppTheme.textPrimary
            font.pixelSize: root.titleSize
            font.weight: Font.Bold
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            text: {
                const bits = []
                if ((root.info.year || 0) > 0)
                    bits.push(root.info.year)
                if (root.seasons.length === 1)
                    bits.push(qsTr("1 season"))
                else if (root.seasons.length > 1)
                    bits.push(qsTr("%1 seasons").arg(root.seasons.length))
                if ((root.info.genres || "").length > 0)
                    bits.push(root.info.genres)
                if ((root.info.rating || 0) > 0)
                    bits.push(S.Format.rating(root.info.rating))
                return bits.join("  ·  ")
            }
            visible: text.length > 0
            color: S.AppTheme.textPrimary
            font.pixelSize: S.AppTheme.fs18
            elide: Text.ElideRight
        }

        Text {
            Layout.preferredWidth: root.heroTextWidth
            Layout.maximumWidth: root.heroTextWidth
            Layout.preferredHeight: Math.round(S.AppTheme.fs15 * 1.35 * 3)

            text: root.info.overview || ""
            visible: text.length > 0
            color: S.AppTheme.textSecondary

            fontSizeMode: Text.Fit
            font.pixelSize: S.AppTheme.fs15
            minimumPixelSize: S.AppTheme.fs11

            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            lineHeight: 1.35
        }

        Item {
            Layout.topMargin: S.AppTheme.spacing8
            Layout.preferredWidth: root.heroTextWidth
            Layout.preferredHeight: _heroActions.height

            Row {
                id: _heroActions

                spacing: S.AppTheme.spacing12

                transformOrigin: Item.Left
                scale: implicitWidth > root.heroTextWidth
                       ? root.heroTextWidth / implicitWidth
                       : 1

                Ctrl.AppButton {
                    id: _continue

                    height: root.actionHeight
                    visible: root.nextUp !== null
                    text: {
                        if (root.nextUp === null)
                            return ""
                        return S.Format.nextUpLabel(root.nextUp.season, root.nextUp.episode,
                                                    (root.nextUp.resumeSeconds || 0) > 0)
                    }
                    variant: Ctrl.AppButton.Filled
                    iconSource: S.Icons.play

                    KeyNavigation.right: _markSeason
                    Keys.onLeftPressed: root.drawerRequested()
                    Keys.onDownPressed: root.focusBelowHero()
                    onClicked: root.playRequested(root.nextUp.handle)
                }

                Ctrl.AppButton {
                    id: _markSeason

                    height: root.actionHeight
                    text: root.seasonWatched ? qsTr("Mark season unwatched")
                                             : qsTr("Mark season watched")
                    variant: Ctrl.AppButton.Outlined
                    enabled: root.show.count > 0

                    Keys.onLeftPressed: {
                        if (_continue.visible)
                            _continue.forceActiveFocus(Qt.TabFocusReason)
                        else
                            root.drawerRequested()
                    }
                    KeyNavigation.right: _fixShow.visible ? _fixShow : null
                    Keys.onDownPressed: root.focusBelowHero()
                    onClicked: root.setSeasonWatched(!root.seasonWatched)
                }

                Ctrl.AppButton {
                    id: _fixShow

                    height: root.actionHeight
                    visible: root.info.suggested === true
                    text: qsTr("Fix match")
                    variant: Ctrl.AppButton.Outlined
                    enabled: Metadata.available

                    Keys.onLeftPressed: _markSeason.forceActiveFocus(Qt.TabFocusReason)
                    Keys.onDownPressed: root.focusBelowHero()
                    onClicked: root.openFixMatch()
                }
            }
        }
    }

    function focusBelowHero() {
        if (_seasons.visible)
            _seasons.forceActiveFocus(Qt.TabFocusReason)
        else if (_rail.count > 0)
            _rail.forceActiveFocus(Qt.TabFocusReason)
    }

    function focusHero() {
        if (_continue.visible)
            _continue.forceActiveFocus(Qt.TabFocusReason)
        else
            _markSeason.forceActiveFocus(Qt.TabFocusReason)
    }

    Column {
        id: _bottom

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.safeX
        anchors.rightMargin: root.safeX
        anchors.bottomMargin: root.safeY
        spacing: S.AppTheme.spacing12

        ListView {
            id: _seasons

            width: root.hasSeasonPoster
                   ? parent.width - root.cardWidth : parent.width
            height: visible ? S.AppTheme.controlHeightSmall : 0
            visible: root.seasons.length > 0
            orientation: ListView.Horizontal
            spacing: S.AppTheme.spacing8
            model: root.seasons
            currentIndex: Math.max(0, root.seasons.indexOf(root.season))
            keyNavigationEnabled: true
            keyNavigationWraps: false
            clip: true

            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Down) {
                    if (_rail.count > 0)
                        _rail.forceActiveFocus(Qt.TabFocusReason)
                    event.accepted = true
                } else if (event.key === Qt.Key_Up) {
                    root.focusHero()
                    event.accepted = true
                } else if (event.key === Qt.Key_Left
                           && _seasons.currentIndex === 0) {
                    root.drawerRequested()
                    event.accepted = true
                }
            }

            onCurrentIndexChanged: {
                if (currentIndex >= 0 && currentIndex < root.seasons.length)
                    root.show.season = root.seasons[currentIndex]
            }

            delegate: Ctrl.Chip {
                required property var modelData
                required property int index

                text: modelData === 0 ? qsTr("Specials")
                                      : qsTr("Season %1").arg(modelData)
                selected: index === _seasons.currentIndex

                Ctrl.FocusRing {
                    active: index === _seasons.currentIndex && _seasons.activeFocus
                    ringRadius: S.AppTheme.radiusSmall
                }
            }
        }

        ListView {
            id: _rail

            width: parent.width
            height: visible ? root.railHeight : 0
            visible: count > 0
            orientation: ListView.Horizontal
            spacing: S.AppTheme.spacing12
            model: root.show
            keyNavigationEnabled: true
            keyNavigationWraps: false
            clip: true
            preferredHighlightBegin: 0
            preferredHighlightEnd: width
            highlightRangeMode: ListView.ApplyRange
            highlightMoveDuration: 140

            Keys.onPressed: (event) => {
                const item = root.show.get(_rail.currentIndex)

                if (event.key === Qt.Key_Up) {
                    if (_seasons.visible)
                        _seasons.forceActiveFocus(Qt.TabFocusReason)
                    else
                        root.focusHero()
                    event.accepted = true
                } else if (event.key === Qt.Key_Down) {
                    if (_rail.currentItem && item && item.hasFile === true) {
                        _rail.currentItem.focusPlay()
                        event.accepted = true
                    }
                } else if (event.key === Qt.Key_Left
                           && _rail.currentIndex === 0) {
                    root.drawerRequested()
                    event.accepted = true
                }
            }

            delegate: Item {
                id: _episode

                required property var model
                required property int index

                readonly property bool current: ListView.isCurrentItem
                readonly property bool cursorHere: current && _rail.activeFocus

                function focusPlay() {
                    _episodePlay.forceActiveFocus(Qt.TabFocusReason)
                }

                width: _episode.current ? root.episodeWidthFocused
                                        : root.episodeWidth
                height: _rail.height
                opacity: _episode.model.hasFile === true ? 1 : 0.45

                Behavior on width {
                    NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
                }

                Column {
                    anchors.fill: parent
                    spacing: S.AppTheme.spacing8

                    Item {
                        width: parent.width
                        height: _episode.current ? root.stillHeightFocused
                                                 : root.stillHeight

                        Behavior on height {
                            NumberAnimation {
                                duration: 130
                                easing.type: Easing.OutCubic
                            }
                        }

                        Rectangle {
                            id: _stillSurface

                            anchors.fill: parent
                            radius: S.AppTheme.radiusMedium
                            color: S.AppTheme.surfaceVariant
                        }

                        Ctrl.RoundedClip {
                            anchors.fill: parent
                            radius: _stillSurface.radius

                            Image {
                                anchors.fill: parent
                                source: {
                                    void _episode.model.artworkStamp
                                    return Metadata.backdropUrl(
                                        _episode.model.stillPath || "", 780)
                                }
                                sourceSize.width: Math.ceil(root.episodeWidthFocused
                                                            * Screen.devicePixelRatio)
                                sourceSize.height: Math.ceil(root.stillHeightFocused
                                                             * Screen.devicePixelRatio)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                visible: (_episode.model.stillPath || "").length > 0
                            }

                            Rectangle {
                                anchors.fill: parent
                                color: S.AppTheme.background
                                opacity: 0.62
                                visible: _episode.model.watched === true
                            }

                            Ctrl.LinearProgress {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                visible: (_episode.model.progress || 0) > 0.01
                                         && _episode.model.watched !== true
                                value: Math.min(1, _episode.model.progress || 0)
                            }
                        }

                        Ctrl.ThemedIcon {
                            anchors.centerIn: parent
                            width: Math.round(root.stillHeight * 0.3)
                            height: width
                            source: S.Icons.check
                            tintColor: S.AppTheme.textPrimary
                            visible: _episode.model.watched === true
                        }

                        Ctrl.FocusRing {
                            active: _episode.cursorHere
                            ringRadius: _stillSurface.radius
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("%1. %2")
                              .arg(_episode.model.episode)
                              .arg((_episode.model.title || "").length > 0
                                   ? _episode.model.title
                                   : qsTr("Episode %1").arg(_episode.model.episode))
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs15
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: {
                            const bits = []
                            if ((_episode.model.runtimeMinutes || 0) > 0)
                                bits.push(S.Format.runtime(_episode.model.runtimeMinutes))
                            if ((_episode.model.progress || 0) > 0.01
                                    && _episode.model.watched !== true) {
                                const left = Math.max(0,
                                    (_episode.model.durationSeconds || 0)
                                    - (_episode.model.positionSeconds || 0))
                                bits.push(qsTr("%1m left").arg(Math.round(left / 60)))
                            }
                            if (_episode.model.hasFile !== true)
                                bits.push(qsTr("Not in your library"))
                            return bits.join("  ·  ")
                        }
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                        elide: Text.ElideRight
                    }

                    Row {
                        width: parent.width
                        height: root.actionHeight
                        spacing: S.AppTheme.spacing8
                        visible: _episode.current
                                 && _episode.model.hasFile === true

                        Ctrl.AppButton {
                            id: _episodePlay

                            width: root.episodeActionWidth
                            height: root.actionHeight
                            size: Ctrl.AppButton.Medium
                            leftPadding: S.AppTheme.spacing10
                            text: (_episode.model.resumeSeconds || 0) > 0
                                  ? qsTr("Resume") : qsTr("Play")
                            variant: Ctrl.AppButton.Filled

                            KeyNavigation.right: _episodeDetails
                            Keys.onUpPressed: _rail.forceActiveFocus(Qt.TabFocusReason)
                            onClicked: root.playRequested(_episode.model.handle)
                        }

                        Ctrl.AppButton {
                            id: _episodeDetails

                            width: root.episodeActionWidth
                            height: root.actionHeight
                            size: Ctrl.AppButton.Medium
                            leftPadding: S.AppTheme.spacing10
                            text: qsTr("Details")
                            variant: Ctrl.AppButton.Outlined

                            KeyNavigation.left: _episodePlay
                            Keys.onUpPressed: _rail.forceActiveFocus(Qt.TabFocusReason)
                            onClicked: root.detailsRequested(_episode.model.handle)
                        }
                    }
                }
            }
        }

        Ctrl.EmptyState {
            width: parent.width
            visible: _rail.count === 0
            title: qsTr("Nothing in this season yet")
            message: qsTr("Episodes appear here once their files are matched.")
        }
    }
}
