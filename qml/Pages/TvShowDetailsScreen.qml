pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    readonly property bool compact: !System.isTelevision
        && Math.min(Window.width, Window.height) < S.AppTheme.breakpointCompact

    property var mediaId: 0

    signal backRequested()
    signal playRequested(string handle)
    signal detailsRequested(string handle)

    readonly property bool acceptsFocus: true

    function takeFocus() {
        _back.forceActiveFocus(Qt.TabFocusReason)
    }

    Ctrl.FocusScroller {
        id: _focusScroll

        owner: root
        flickable: _scroll
        content: _column
    }

    Keys.onUpPressed: _focusScroll.step(false)
    Keys.onDownPressed: _focusScroll.step(true)

    readonly property var show: Library.showEpisodes
    readonly property var info: show.info

    readonly property bool suggestedMatch: root.info.suggested === true
    readonly property bool matched: (root.info.title || "").length > 0

    readonly property var cast: root.info.cast || []
    readonly property string creators: root.info.creators || ""

    readonly property var fileHandles: {
        void root.show.count
        return root.show.fileHandles()
    }
    readonly property var fileNames: {
        void root.show.count
        return root.show.fileNames()
    }

    property var fixCandidates: []
    property bool fixSearching: false

    function openFixMatch() {
        root.fixCandidates = []
        _fixSheet.beginWith(root.info.title || "", true)
    }

    function confirmMatch() {
        const current = {
            "tmdbId": root.info.tmdbId,
            "kind": "tv",
            "title": root.info.title,
            "year": root.info.year,
            "overview": root.info.overview,
            "rating": root.info.rating,
            "posterPath": root.info.posterPath
        }
        Metadata.pinMatchForShow(root.fileHandles, current)
    }
    readonly property int season: show.season
    readonly property var seasons: show.seasons

    readonly property bool wide: width >= 720

    readonly property var nextUp: show.hasNextUp ? show.nextUp : null

    readonly property bool seasonWatched: show.seasonWatched
    readonly property bool seasonHasFiles: show.seasonHasFiles

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

    DetailsHero {
        id: _hero

        anchors.left: parent.left
        anchors.right: parent.right
        pageHeight: root.height
        scrollY: _scroll.contentY

        backdropPath: root.show.seasonBackdropPath || root.info.backdropPath || ""
        posterPath: root.show.seasonPosterPath || root.info.posterPath || ""
        title: root.info.title || ""
        metaText: {
            const bits = []
            if (root.info.year > 0)
                bits.push(root.info.year + "–")
            bits.push(qsTr("%n season(s)", "", root.seasons.length))
            bits.push(qsTr("%n file(s) here", "", root.info.filesOnDisk || 0))
            if ((root.info.genres || "").length > 0)
                bits.push(root.info.genres)
            if ((root.info.certification || "").length > 0)
                bits.push(root.info.certification)
            if ((root.info.rating || 0) > 0)
                bits.push(S.Format.rating(root.info.rating))
            return bits.join("  ·  ")
        }
    }

    Ctrl.IconButton {
        id: _back

        z: 4
        anchors.left: System.isTelevision ? undefined : parent.left
        anchors.right: System.isTelevision ? parent.right : undefined
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing4
        opacity: _hero.fade
        visible: opacity > 0.01
        iconSource: S.Icons.chevronLeft
        tintColor: "#FFFFFF"
        scrim: true
        accessibleName: qsTr("Back")
        onClicked: root.backRequested()
    }

    Ctrl.IconButton {
        id: _more

        z: 4
        opacity: _hero.fade
        visible: !System.isTelevision && !root.suggestedMatch && !Streaming.connected
                 && root.fileHandles.length > 0 && opacity > 0.01
        anchors.right: System.isTelevision ? undefined : parent.right
        anchors.left: System.isTelevision ? parent.left : undefined
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing4
        iconSource: S.Icons.moreVertical
        tintColor: "#FFFFFF"
        scrim: true
        accessibleName: qsTr("More")
        onClicked: _showMenu.open()
    }

    Flickable {
        id: _scroll

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        contentHeight: _column.implicitHeight + S.AppTheme.spacing32
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Column {
            id: _column

            width: parent.width - S.AppTheme.scrollBarWidth
            spacing: 0

            Item {
                width: 1
                height: _hero.fullHeight
            }

            Item {
                width: 1
                height: S.AppTheme.spacing12
                visible: root.nextUp !== null || root.seasonHasFiles
            }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton {
                    visible: root.nextUp !== null
                    variant: Ctrl.AppButton.Filled
                    iconSource: S.Icons.play
                    text: {
                        if (root.nextUp === null)
                            return ""
                        return S.Format.nextUpLabel(root.nextUp.season, root.nextUp.episode,
                                                    (root.nextUp.resumeSeconds || 0) > 0)
                    }

                    onClicked: root.playRequested(root.nextUp.handle)
                }

                Rectangle {
                    width: 1
                    height: S.AppTheme.controlHeightLarge
                    visible: root.seasonHasFiles && root.nextUp !== null
                    color: S.AppTheme.outline
                }

                Ctrl.AppButton {
                    visible: root.seasonHasFiles
                    iconOnly: true
                    variant: root.seasonWatched ? Ctrl.AppButton.Tonal
                                                : Ctrl.AppButton.Outlined
                    iconSource: S.Icons.check
                    text: root.seasonWatched ? qsTr("Mark season unwatched")
                                             : qsTr("Mark season watched")

                    onClicked: root.setSeasonWatched(!root.seasonWatched)
                }
            }

            Ctrl.Card {
                width: parent.width
                variant: true
                visible: root.suggestedMatch

                Column {
                    width: parent.width
                    spacing: S.AppTheme.spacing4

                    Text {
                        width: parent.width
                        text: root.fileHandles.length === 1
                              ? qsTr("We think this file is %1.")
                                .arg(root.info.title || "")
                              : qsTr("We think these %1 files are %2.")
                                .arg(root.fileHandles.length)
                                .arg(root.info.title || "")
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs15
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Nobody has said so yet. Fix match confirms "
                                   + "it, or finds the right show.")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                        wrapMode: Text.Wrap
                    }

                    Item {
                        width: 1
                        height: S.AppTheme.spacing8
                    }

                    Ctrl.AppButton {
                        visible: !System.isTelevision
                                 && root.fileHandles.length > 0
                        size: Ctrl.AppButton.Medium
                        variant: Ctrl.AppButton.Filled
                        iconSource: S.Icons.search
                        text: qsTr("Fix match")

                        onClicked: root.openFixMatch()
                    }
                }
            }

            Ctrl.SectionLabel {
                visible: root.cast.length > 0
                text: qsTr("Cast")
                topPadding: S.AppTheme.spacing12
            }

            Ctrl.CastGrid {
                width: parent.width
                cast: root.cast
            }

            Ctrl.SectionLabel {
                visible: root.creators.length > 0
                text: qsTr("Crew")
            }

            Column {
                width: parent.width
                visible: root.creators.length > 0
                spacing: S.AppTheme.spacing2

                Ctrl.KeyValueRow {
                    width: parent.width
                    label: qsTr("Created by")
                    value: root.creators
                }
            }

            Ctrl.SectionLabel {
                visible: (root.info.overview || "").length > 0
                text: qsTr("Overview")
                topPadding: S.AppTheme.spacing12
            }

            Text {
                width: parent.width
                visible: (root.info.overview || "").length > 0
                text: root.info.overview || ""
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                lineHeight: 1.55
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.Wrap
            }

            Item {
                width: 1
                height: S.AppTheme.spacing14
            }

            Ctrl.TmdbCredit {
                width: parent.width
            }

            Item {
                id: _seasonsAndEpisodes

                width: parent.width
                height: root.wide
                        ? Math.max(_seasonList.height,
                                   _seasonsLabel.height + _episodeList.height)
                        : _seasonList.height + S.AppTheme.spacing16
                          + _episodeList.height

                Column {
                    id: _seasonList

                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: root.wide ? 236 : parent.width
                    spacing: S.AppTheme.spacing2

                    Ctrl.SectionLabel {
                        id: _seasonsLabel

                        text: qsTr("Seasons")
                    }

                    Repeater {
                        model: root.seasons

                        delegate: Rectangle {
                            id: _seasonRow

                            required property var modelData

                            readonly property bool current:
                                root.season === _seasonRow.modelData

                            width: parent.width
                            height: S.AppTheme.controlHeightLarge
                                    + S.AppTheme.spacing8
                            radius: S.AppTheme.radiusMedium
                            color: {
                                if (_seasonRow.current)
                                    return S.AppTheme.surfaceVariant
                                return _seasonArea.containsMouse
                                       ? S.AppTheme.hover : "transparent"
                            }

                            MouseArea {
                                id: _seasonArea

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.show.season = _seasonRow.modelData
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: S.AppTheme.spacing16
                                anchors.right: _seasonCount.left
                                anchors.rightMargin: S.AppTheme.spacing8
                                anchors.verticalCenter: parent.verticalCenter
                                text: _seasonRow.modelData === 0
                                      ? qsTr("Specials")
                                      : qsTr("Season %1").arg(_seasonRow.modelData)
                                color: S.AppTheme.textPrimary
                                font.pixelSize: S.AppTheme.fs16
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }

                            Text {
                                id: _seasonCount

                                anchors.right: parent.right
                                anchors.rightMargin: S.AppTheme.spacing16
                                anchors.verticalCenter: parent.verticalCenter
                                text: {
                                    void root.show.count
                                    return qsTr("%1 ep").arg(
                                        root.show.episodeCountFor(_seasonRow.modelData))
                                }
                                color: S.AppTheme.textSecondary
                                font.pixelSize: S.AppTheme.fs13
                            }
                        }
                    }
                }

                Column {
                    id: _episodeList

                    anchors.left: root.wide ? _seasonList.right : parent.left
                    anchors.leftMargin: root.wide ? S.AppTheme.spacing24 : 0
                    anchors.right: parent.right
                    anchors.top: root.wide ? parent.top : _seasonList.bottom
                    anchors.topMargin: root.wide ? _seasonsLabel.height
                                                 : S.AppTheme.spacing16

                    Repeater {
                        model: root.show

                        delegate: Column {
                            id: _episode

                            required property var model

                            readonly property bool playable:
                                _episode.model.hasFile && !_episode.model.missing
                            readonly property bool partWatched:
                                (_episode.model.resumeSeconds || 0) > 0

                            width: parent.width

                            Ctrl.Divider { width: parent.width }

                            Item {
                                width: parent.width
                                height: Math.max(_body.implicitHeight, _still.height)
                                        + 2 * S.AppTheme.spacing16

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: _episode.model.hasFile
                                    onClicked: root.detailsRequested(_episode.model.handle)
                                }

                                Text {
                                    id: _number

                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 28
                                    text: String(_episode.model.episode)
                                          .padStart(2, "0")
                                    color: S.AppTheme.textDisabled
                                    font.family: S.AppTheme.monoFontFamily
                                    font.pixelSize: S.AppTheme.fs16
                                }

                                Rectangle {
                                    id: _still

                                    anchors.left: _number.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: root.wide ? 180 : 140
                                    height: Math.round(width * 9 / 16)
                                    radius: S.AppTheme.radiusSmall
                                    color: S.AppTheme.surfaceVariant

                                    Ctrl.RoundedClip {
                                        anchors.fill: parent
                                        radius: _still.radius

                                        Image {
                                            anchors.fill: parent
                                            source: {
                                                void _episode.model.artworkStamp
                                                return Metadata.stillUrl(
                                                            _episode.model.stillPath || "", 300)
                                            }
                                            sourceSize.width: Math.ceil(parent.width * Screen.devicePixelRatio)
                                            sourceSize.height: Math.ceil(parent.height * Screen.devicePixelRatio)
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            visible: status === Image.Ready
                                        }

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: S.AppTheme.seekBarHeight + 2
                                            visible: _episode.model.progress > 0
                                            color: Qt.rgba(0, 0, 0, 0.45)

                                            Rectangle {
                                                width: parent.width
                                                       * Math.min(1, _episode.model.progress)
                                                height: parent.height
                                                color: S.AppTheme.primary
                                            }
                                        }
                                    }

                                    Ctrl.ThemedIcon {
                                        anchors.centerIn: parent
                                        width: 22
                                        height: 22
                                        visible: (_episode.model.stillPath || "").length === 0
                                        source: S.Icons.noVideo
                                        tintColor: S.AppTheme.textDisabled
                                        showPlaceholder: false
                                    }

                                    Rectangle {
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        anchors.margins: S.AppTheme.spacing4
                                        visible: _episode.model.watched
                                        width: _seen.implicitWidth + S.AppTheme.spacing6
                                        height: _seen.implicitHeight + 2
                                        radius: S.AppTheme.radiusPill
                                        color: S.AppTheme.primary

                                        Text {
                                            id: _seen

                                            anchors.centerIn: parent
                                            text: qsTr("Seen")
                                            color: S.AppTheme.onPrimaryStrong
                                            font.pixelSize: S.AppTheme.fs11
                                        }
                                    }
                                }

                                Column {
                                    id: _body

                                    anchors.left: _still.right
                                    anchors.leftMargin: S.AppTheme.spacing16
                                    anchors.right: _action.left
                                    anchors.rightMargin: S.AppTheme.spacing16
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: S.AppTheme.spacing2

                                    Text {
                                        width: parent.width
                                        text: _episode.model.title.length > 0
                                              ? _episode.model.title
                                              : qsTr("Episode %1").arg(_episode.model.episode)
                                        color: _episode.playable
                                               ? S.AppTheme.textPrimary
                                               : S.AppTheme.textDisabled
                                        font.pixelSize: S.AppTheme.fs16
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: parent.width
                                        text: {
                                            if (!_episode.model.hasFile)
                                                return qsTr("Not on disk")
                                            if (_episode.model.missing)
                                                return qsTr("Missing on disk")

                                            const bits = []
                                            if (_episode.model.watched)
                                                bits.push(qsTr("Seen"))
                                            if (_episode.model.airDate.length > 0)
                                                bits.push(_episode.model.airDate)
                                            if (_episode.model.runtimeMinutes > 0)
                                                bits.push(S.Format.runtime(_episode.model.runtimeMinutes))
                                            return bits.join("  ·  ")
                                        }
                                        color: _episode.model.hasFile
                                               && !_episode.model.missing
                                               ? S.AppTheme.textSecondary
                                               : S.AppTheme.error
                                        font.pixelSize: S.AppTheme.fs12
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: parent.width
                                        visible: _episode.model.overview.length > 0
                                        topPadding: S.AppTheme.spacing4
                                        text: _episode.model.overview
                                        color: S.AppTheme.textSecondary
                                        font.pixelSize: S.AppTheme.fs12
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                }

                                Ctrl.AppButton {
                                    id: _action

                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: _episode.playable
                                    size: Ctrl.AppButton.Medium
                                    variant: _episode.partWatched
                                             ? Ctrl.AppButton.Filled
                                             : Ctrl.AppButton.Outlined
                                    iconSource: S.Icons.play
                                    iconOnly: root.compact
                                    text: _episode.partWatched
                                          ? qsTr("Resume %1").arg(
                                                S.Format.clock(_episode.model.positionSeconds))
                                          : (_episode.model.watched ? qsTr("Again")
                                                                    : qsTr("Play"))

                                    onClicked: root.playRequested(_episode.model.handle)
                                }
                            }
                        }
                    }
                }
            }

            Ctrl.EmptyState {
                width: parent.width
                visible: root.show.count === 0
                iconSource: S.Icons.noVideo
                title: qsTr("No episodes here")
                message: qsTr("Episodes appear once their files are matched to this show.")
            }

        }
    }

    Connections {
        target: Metadata

        function onCandidatesReady(candidates) {
            root.fixSearching = false
            root.fixCandidates = candidates
        }
    }

    Ctrl.BottomSheet {
        id: _showMenu

        title: root.info.title || qsTr("Show")

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.search
                title: qsTr("Fix match")
                subtitle: qsTr("Every episode here is re-matched together")

                onClicked: {
                    _showMenu.close()
                    root.openFixMatch()
                }
            }
        }
    }

    Ctrl.FixMatchSheet {
        id: _fixSheet

        searchTv: true
        matched: false
        currentTmdbId: root.info.tmdbId === undefined ? 0 : root.info.tmdbId
        candidates: root.fixCandidates
        searching: root.fixSearching

        fileTotal: root.fileHandles.length
        fileNames: root.fileNames
        guessTitle: {
            const title = root.info.title || ""
            if (title.length === 0)
                return ""
            const year = root.info.year > 0 ? " (" + root.info.year + ")" : ""
            return title + year
        }
        guessConfirmable: root.suggestedMatch

        onSearchRequested: (text, tv) => {
            root.fixSearching = true
            Metadata.searchCandidates(text, tv)
        }

        onPicked: (candidate) => Metadata.pinMatchForShow(root.fileHandles, candidate)
        onGuessConfirmed: root.confirmMatch()
    }
}
