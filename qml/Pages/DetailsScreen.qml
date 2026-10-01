pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    property string handle: ""
    property var info: ({})

    readonly property var cast: root.metadata.cast || []
    readonly property string directors: root.metadata.directors || ""
    readonly property string writers: root.metadata.writers || ""
    readonly property bool hasCrew: directors.length > 0 || writers.length > 0

    component Fact: Rectangle {
        id: _fact

        property string label
        property string value

        width: parent ? parent.cellWidth : 200
        implicitHeight: Math.max(_factLabel.implicitHeight,
                                 _factValue.implicitHeight)
                        + 2 * S.AppTheme.spacing10
        height: parent && parent.cellHeight > 0 ? parent.cellHeight
                                                : implicitHeight
        color: S.AppTheme.surface

        Text {
            id: _factLabel

            anchors.left: parent.left
            anchors.leftMargin: S.AppTheme.spacing14
            anchors.top: parent.top
            anchors.topMargin: S.AppTheme.spacing10 + 1
            width: 84
            text: _fact.label
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs11
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.4
        }

        Text {
            id: _factValue

            anchors.left: _factLabel.right
            anchors.leftMargin: S.AppTheme.spacing12
            anchors.right: parent.right
            anchors.rightMargin: S.AppTheme.spacing14
            anchors.top: parent.top
            anchors.topMargin: S.AppTheme.spacing10
            text: _fact.value
            color: S.AppTheme.textPrimary
            font.pixelSize: S.AppTheme.fs13
            wrapMode: Text.WrapAnywhere
        }
    }

    signal backRequested()
    signal playRequested(string handle)
    signal playFromStartRequested(string handle)
    signal detailsRequested(string handle)
    signal collectionRequested(var collectionId)

    readonly property bool acceptsFocus: true

    function takeFocus() {
        _primary.forceActiveFocus(Qt.TabFocusReason)
    }

    Ctrl.FocusScroller {
        id: _focusScroll

        owner: root
        flickable: _scroll
        content: _column
    }

    Keys.onUpPressed: _focusScroll.step(false)
    Keys.onDownPressed: _focusScroll.step(true)

    property var attachedSubtitles: []
    property var siblingSubtitles: []

    readonly property int embeddedSubtitleCount: root.info.subtitleTrackCount || 0

    property var collection: ({})

    readonly property url videoFolderUrl: {
        const folder = root.value("folderPath")
        return folder.length > 0 ? Qt.resolvedUrl("file:///" + folder) : ""
    }
    readonly property var downloadedSubtitles:
        root.attachedSubtitles.filter((s) => s.downloaded === true)
    readonly property var pickedSubtitles:
        root.attachedSubtitles.filter((s) => s.downloaded !== true)
    property bool awaitingSubtitlePick: false

    property var metadata: ({})
    readonly property bool matched: (metadata.title || "").length > 0
    readonly property bool isEpisode: metadata.isEpisode === true

    readonly property string overviewText: {
        if (metadata.isEpisode === true
                && (metadata.episodeOverview || "").length > 0) {
            return metadata.episodeOverview
        }
        return metadata.overview || ""
    }

    readonly property bool hasHero: matched
    readonly property bool wide: width >= 720
    readonly property string backdropPath:
        metadata.episodeStillPath || metadata.backdropPath || ""

    readonly property string posterPath:
        metadata.seasonPosterPath || metadata.posterPath || ""

    readonly property int shownRuntimeMinutes: {
        if (metadata.isEpisode === true
                && (metadata.episodeRuntimeMinutes || 0) > 0) {
            return metadata.episodeRuntimeMinutes
        }
        if ((metadata.runtimeMinutes || 0) > 0)
            return metadata.runtimeMinutes
        return durationSeconds > 0 ? Math.round(durationSeconds / 60) : 0
    }

    readonly property string shownDate: {
        if (metadata.isEpisode === true
                && (metadata.episodeAirDate || "").length > 0) {
            return metadata.episodeAirDate
        }
        return (metadata.year || 0) > 0 ? String(metadata.year) : ""
    }

    readonly property string qualityText:
        S.Format.quality(info.width, info.height, info.hdr, "")

    property bool autoOpenFixMatch: false

    property var fixCandidates: []
    property bool fixSearchTv: false
    property bool fixSearching: false

    function refreshMetadata() {
        metadata = Metadata.metadataForFile(root.handle)
        nextEpisode = metadata.isEpisode === true
                      ? Metadata.nextEpisodeFor(root.handle) : ({})
    }

    property var nextEpisode: ({})
    readonly property bool hasNextEpisode:
        (nextEpisode.handle || "").length > 0

    readonly property bool offersFixMatch: !root.isEpisode
                                           && !Streaming.connected

    function openFixMatch() {
        if (!root.offersFixMatch)
            return

        fixCandidates = []
        const guess = Library.parsedNameFor(root.handle)
        _fixSheet.beginWith(guess.title || root.displayName,
                            guess.isEpisode === true)
    }

    function refreshAttachedSubtitle() {
        attachedSubtitles = Library.attachedSubtitles(root.handle)
    }

    function refreshSiblingSubtitles() {
        siblingSubtitles = Library.siblingSubtitleTracks(root.handle)
    }

    function chooseSubtitleFile() {
        if (System.usesSystemFilePicker) {
            awaitingSubtitlePick = true
            System.pickSubtitleFile()
            return
        }
        _subtitleDialog.open()
    }

    readonly property string displayName:
        info.displayName === undefined ? "" : info.displayName
    readonly property bool present: info.present !== false
    readonly property bool indexed: info.indexed === true
    readonly property bool finished: info.finished === true
    readonly property real positionSeconds:
        info.positionSeconds === undefined ? 0 : info.positionSeconds
    readonly property real durationSeconds:
        info.durationSeconds === undefined ? 0 : info.durationSeconds
    readonly property real progress:
        info.progress === undefined ? 0 : Math.max(0, Math.min(1, info.progress))

    readonly property real resumeSeconds:
        info.resumeSeconds === undefined ? 0 : info.resumeSeconds

    readonly property bool partiallyWatched: !finished && resumeSeconds > 0

    property bool removedFromLibrary: false

    readonly property bool canRemove:
        !System.isTelevision && root.indexed
        && (!root.present
            || (!root.matched && root.info.suggested !== true))

    function refresh() {
        if (handle.length > 0 && !removedFromLibrary) {
            info = Library.fileInfo(handle)
            collection = Library.collectionOfMedia(info.mediaId || 0)
        }
    }

    function takeOutOfLibrary() {
        root.removedFromLibrary = true
        Library.removeFromLibrary(root.handle)
        root.backRequested()
    }

    function value(key) {
        return info[key] === undefined ? "" : info[key]
    }

    onHandleChanged: {
        removedFromLibrary = false
        refresh()
        refreshAttachedSubtitle()
        refreshMetadata()

        _scroll.contentY = 0
        if (S.AppTheme.remoteNavigation)
            takeFocus()
    }

    Component.onCompleted: {
        refreshAttachedSubtitle()
        refreshMetadata()
        if (autoOpenFixMatch)
            _fixOnOpen.start()
    }

    Timer {
        id: _fixOnOpen

        interval: 1
        onTriggered: root.openFixMatch()
    }

    Connections {
        target: Metadata

        function onMatchesChangedFor(fileHandles, mediaIds) {
            if (fileHandles.indexOf(root.handle) >= 0
                    || (root.metadata.mediaId !== undefined
                        && mediaIds.indexOf(root.metadata.mediaId) >= 0))
                root.refreshMetadata()
        }

        function onCandidatesReady(candidates) {
            root.fixSearching = false
            root.fixCandidates = candidates
        }
    }

    Ctrl.FixMatchSheet {
        id: _fixSheet

        pinned: root.metadata.pinned === true
        matched: root.matched
        currentTmdbId: root.metadata.tmdbId
        candidates: root.fixCandidates
        searching: root.fixSearching

        fileName: root.displayName
        folderPath: root.info.folderPath === undefined ? "" : root.info.folderPath
        fileTotal: 1
        guessTitle: {
            if (!root.matched)
                return ""
            const year = root.metadata.year > 0 ? " (" + root.metadata.year + ")" : ""
            return root.metadata.title + year
        }
        guessConfirmable: root.metadata.suggested === true

        onSearchRequested: (text, tv) => {
            root.fixSearchTv = tv
            root.fixSearching = true
            Metadata.searchCandidates(text, tv)
        }
        onPicked: (candidate) => Metadata.pinMatch(root.handle, candidate)
        onGuessConfirmed: Metadata.pinMatch(root.handle, root.metadata)
        onUnpinRequested: Metadata.unpinMatch(root.handle)
        onNotMediaRequested: Metadata.clearMatch(root.handle)
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    Ctrl.AppBar {
        id: _bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: !root.hasHero
        height: visible ? implicitHeight : 0
        leading: Ctrl.AppBar.Back
        title: ""

        onLeadingTriggered: root.backRequested()
    }

    DetailsHero {
        id: _hero

        anchors.left: parent.left
        anchors.right: parent.right
        height: root.hasHero ? fullHeight : 0
        visible: root.hasHero
        pageHeight: root.height
        scrollY: _scroll.contentY

        backdropPath: root.backdropPath
        posterPath: root.posterPath
        title: root.metadata.title || ""
        kicker: {
            if (!root.isEpisode)
                return ""
            const s = root.metadata.season || 0
            const e = root.metadata.episode || 0
            const name = root.metadata.episodeTitle || ""
            const code = s === 0
                ? qsTr("Specials · Episode %1").arg(e)
                : qsTr("Season %1 · Episode %2").arg(s).arg(e)
            return name.length > 0 ? code + "  ·  " + name : code
        }
        metaText: {
            const bits = []
            if (root.shownDate.length > 0)
                bits.push(root.shownDate)
            if (root.shownRuntimeMinutes > 0)
                bits.push(S.Format.runtime(root.shownRuntimeMinutes))
            if ((root.metadata.genres || "").length > 0)
                bits.push(root.metadata.genres)
            if ((root.metadata.certification || "").length > 0)
                bits.push(root.metadata.certification)
            if ((root.metadata.rating || 0) > 0)
                bits.push(S.Format.rating(root.metadata.rating))
            if (root.qualityText.length > 0)
                bits.push(root.qualityText)
            return bits.join("  ·  ")
        }
    }

    Ctrl.IconButton {
        z: 4
        anchors.left: System.isTelevision ? undefined : parent.left
        anchors.right: System.isTelevision ? parent.right : undefined
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing4
        opacity: _hero.fade
        visible: root.hasHero && opacity > 0.01
        iconSource: S.Icons.chevronLeft
        tintColor: "#FFFFFF"
        scrim: true
        accessibleName: qsTr("Back")
        onClicked: root.backRequested()
    }

    Flickable {
        id: _scroll

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: root.hasHero ? parent.top : _bar.bottom
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
                height: root.hasHero ? _hero.fullHeight : 0
            }

            Ctrl.SectionLabel {
                visible: !root.matched
                        || root.metadata.suggested === true
                        || root.metadata.pinned === true
                text: {
                    if (!root.matched)
                        return qsTr("Unmatched file")
                    if (root.metadata.suggested)
                        return qsTr("Suggested match")
                    return qsTr("Matched by you")
                }
                topPadding: 0
            }

            Text {
                width: parent.width
                visible: !root.matched
                text: root.displayName
                color: S.AppTheme.textPrimary
                font.family: S.AppTheme.monoFontFamily
                font.pixelSize: S.AppTheme.fs18
                lineHeight: 1.35
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.WrapAnywhere
            }

            Item {
                width: 1
                height: S.AppTheme.spacing14
            }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing6

                Ctrl.Chip {
                    compact: true
                    visible: !root.present
                    text: qsTr("Missing")
                    selected: true
                }

                Ctrl.Chip {
                    compact: true
                    visible: !root.matched && root.value("resolution").length > 0
                    text: root.value("resolution")
                    enabled: false
                }

                Ctrl.Chip {
                    compact: true
                    visible: !root.matched && root.value("videoCodec").length > 0
                    text: root.value("videoCodec")
                    enabled: false
                }

                Ctrl.Chip {
                    compact: true
                    visible: !root.matched && root.info.hdr === true
                    text: qsTr("HDR")
                    enabled: false
                }

                Ctrl.Chip {
                    compact: true
                    visible: !root.matched && root.value("durationText").length > 0
                    text: root.value("durationText")
                    enabled: false
                }

                Ctrl.Chip {
                    compact: true
                    visible: !root.matched && root.value("sizeText").length > 0
                    text: root.value("sizeText")
                    enabled: false
                }

                Ctrl.Chip {
                    compact: true
                    visible: root.finished
                    text: qsTr("Watched")
                    selected: true
                }
            }

            Item {
                width: 1
                height: S.AppTheme.spacing20
            }

            Ctrl.Card {
                width: parent.width
                visible: !root.present
                implicitHeight: _missingText.implicitHeight + 2 * S.AppTheme.spacing14
                color: S.AppTheme.errorContainer

                Column {
                    id: _missingText

                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: S.AppTheme.spacing6

                    Text {
                        width: parent.width
                        text: qsTr("This file is missing")
                        color: S.AppTheme.error
                        font.pixelSize: S.AppTheme.fs15
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        text: qsTr("It is not where Makimedia indexed it. The drive may be disconnected, or the file was moved, renamed or deleted.")
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs12
                        lineHeight: 1.45
                        lineHeightMode: Text.ProportionalHeight
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("It stays in your library with its watch history until you take it out. Remove from library does that, and Settings can put it back if the file returns.")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs11
                        wrapMode: Text.Wrap
                    }
                }
            }

            Item {
                width: 1
                visible: !root.present
                height: visible ? S.AppTheme.spacing14 : 0
            }

            Flow {
                id: _actions

                width: parent.width
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton {
                    id: _primary

                    text: !root.present
                          ? qsTr("File not found")
                          : (root.finished
                             ? qsTr("Play again")
                             : (root.partiallyWatched
                                ? qsTr("Resume %1").arg(S.Format.clock(root.resumeSeconds))
                                : qsTr("Play")))
                    variant: Ctrl.AppButton.Filled
                    iconSource: root.present ? S.Icons.play : ""
                    enabled: root.present
                    focus: true
                    onClicked: root.playRequested(root.handle)
                }

                Ctrl.AppButton {
                    id: _fromStart

                    visible: root.partiallyWatched
                    text: qsTr("Play from start")
                    variant: Ctrl.AppButton.Outlined
                    enabled: root.present
                    onClicked: root.playFromStartRequested(root.handle)
                }

                Rectangle {
                    width: 1
                    height: S.AppTheme.controlHeightLarge
                    color: S.AppTheme.outline
                }

                Ctrl.AppButton {
                    id: _watchedToggle

                    iconOnly: true
                    text: root.finished
                          ? qsTr("Mark as unwatched")
                          : qsTr("Mark as watched")
                    variant: root.finished ? Ctrl.AppButton.Tonal
                                           : Ctrl.AppButton.Outlined
                    iconSource: S.Icons.check
                    enabled: root.indexed
                    onClicked: {
                        Library.setWatched(root.info.fileId, !root.finished)
                        root.refresh()
                    }
                }

                Ctrl.AppButton {
                    id: _forgetProgress

                    iconOnly: true
                    text: qsTr("Remove from Continue watching")
                    variant: Ctrl.AppButton.Outlined
                    iconSource: S.Icons.close
                    visible: !System.isTelevision && root.partiallyWatched
                             && root.indexed && root.present
                    onClicked: {
                        Library.removeFromContinueWatching(root.info.fileId)
                        root.refresh()
                    }
                }

                Ctrl.AppButton {
                    id: _fixMatchButton

                    iconOnly: true
                    text: root.matched ? qsTr("Fix match") : qsTr("Find this title")
                    variant: Ctrl.AppButton.Outlined
                    iconSource: S.Icons.search
                    visible: Metadata.available && root.offersFixMatch
                    enabled: root.indexed
                    onClicked: root.openFixMatch()
                }

                Ctrl.AppButton {
                    id: _subtitleButton

                    iconOnly: true
                    text: root.attachedSubtitles.length > 0
                          ? qsTr("Subtitles (%1)").arg(root.attachedSubtitles.length)
                          : qsTr("Attach a subtitle file")
                    variant: root.attachedSubtitles.length > 0
                             ? Ctrl.AppButton.Tonal
                             : Ctrl.AppButton.Outlined
                    iconSource: S.Icons.subtitles
                    visible: !Streaming.connected
                    enabled: root.present
                    onClicked: _subtitleMenu.open()
                }

                Ctrl.AppButton {
                    id: _moreButton

                    iconOnly: true
                    text: qsTr("More")
                    variant: Ctrl.AppButton.Outlined
                    iconSource: S.Icons.moreVertical
                    visible: root.canRemove
                    onClicked: _moreMenu.open()
                }

                Ctrl.ListRow {
                    id: _collectionRow

                    visible: root.wide && !root.isEpisode
                             && root.collection.id !== undefined
                    width: Math.min(340, _actions.width)
                    height: S.AppTheme.controlHeightLarge
                    leading: (root.collection.backdropPath || "").length > 0
                             ? Ctrl.ListRow.Thumbnail : Ctrl.ListRow.Icon
                    iconSource: S.Icons.collections
                    thumbnailSource: {
                        void Metadata.artworkRevision
                        return Metadata.backdropUrl(
                            root.collection.backdropPath || "", 300)
                    }
                    title: root.collection.name || ""
                    subtitle: root.collection.place > 0
                              ? qsTr("%1 of %2 in this collection")
                                    .arg(root.collection.place)
                                    .arg(root.collection.total || 0)
                              : qsTr("Part of this collection")
                    onClicked: root.collectionRequested(root.collection.id)
                }
            }

            Item {
                width: 1
                visible: root.partiallyWatched
                height: visible ? S.AppTheme.spacing16 : 0
            }

            Item {
                width: parent.width
                visible: root.partiallyWatched
                height: visible ? _elapsed.implicitHeight : 0

                Text {
                    id: _elapsed

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: S.Format.clock(root.positionSeconds)
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs12
                }

                Text {
                    id: _left

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "-" + S.Format.clock(Math.max(0, root.durationSeconds
                                                         - root.positionSeconds))
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs12
                }

                Ctrl.LinearProgress {
                    anchors.left: _elapsed.right
                    anchors.right: _left.left
                    anchors.leftMargin: S.AppTheme.spacing12
                    anchors.rightMargin: S.AppTheme.spacing12
                    anchors.verticalCenter: parent.verticalCenter
                    value: root.progress
                }
            }

            Ctrl.SectionLabel {
                visible: root.hasNextEpisode
                text: qsTr("Next episode")
            }

            Item {
                id: _next

                width: parent.width
                visible: root.hasNextEpisode
                height: visible
                        ? Math.max(_nextStill.height, _nextBody.implicitHeight)
                          + 2 * S.AppTheme.spacing12
                        : 0

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.detailsRequested(root.nextEpisode.handle)
                }

                Rectangle {
                    id: _nextStill

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.wide ? 180 : 140
                    height: Math.round(width * 9 / 16)
                    radius: S.AppTheme.radiusSmall
                    color: S.AppTheme.surfaceVariant

                    Ctrl.RoundedClip {
                        anchors.fill: parent
                        radius: _nextStill.radius

                        Image {
                            anchors.fill: parent
                            source: {
                                void Metadata.artworkRevision
                                return Metadata.stillUrl(
                                            root.nextEpisode.stillPath || "", 300)
                            }
                            sourceSize.width: Math.ceil(parent.width * Screen.devicePixelRatio)
                            sourceSize.height: Math.ceil(parent.height * Screen.devicePixelRatio)
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: status === Image.Ready
                        }
                    }

                    Ctrl.ThemedIcon {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        visible: (root.nextEpisode.stillPath || "").length === 0
                        source: S.Icons.noVideo
                        tintColor: S.AppTheme.textDisabled
                        showPlaceholder: false
                    }
                }

                Column {
                    id: _nextBody

                    anchors.left: _nextStill.right
                    anchors.leftMargin: S.AppTheme.spacing16
                    anchors.right: _nextPlay.left
                    anchors.rightMargin: S.AppTheme.spacing16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: S.AppTheme.spacing2

                    Text {
                        width: parent.width
                        text: qsTr("Season %1 · Episode %2")
                              .arg(root.nextEpisode.season || 0)
                              .arg(root.nextEpisode.episode || 0)
                        color: S.AppTheme.primary
                        font.pixelSize: S.AppTheme.fs12
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        text: (root.nextEpisode.title || "").length > 0
                              ? root.nextEpisode.title
                              : qsTr("Episode %1").arg(root.nextEpisode.episode || 0)
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs16
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: {
                            const bits = []
                            if (root.nextEpisode.watched === true)
                                bits.push(qsTr("Seen"))
                            if ((root.nextEpisode.airDate || "").length > 0)
                                bits.push(root.nextEpisode.airDate)
                            if ((root.nextEpisode.runtimeMinutes || 0) > 0)
                                bits.push(S.Format.runtime(root.nextEpisode.runtimeMinutes))
                            return bits.join("  ·  ")
                        }
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs12
                        elide: Text.ElideRight
                    }
                }

                Ctrl.AppButton {
                    id: _nextPlay

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Outlined
                    iconSource: S.Icons.play
                    text: qsTr("Play")

                    onClicked: root.playRequested(root.nextEpisode.handle)
                }
            }

            Ctrl.SectionLabel {
                visible: root.overviewText.length > 0
                text: qsTr("Overview")
            }

            Text {
                width: parent.width
                visible: root.overviewText.length > 0
                text: root.overviewText
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                lineHeight: 1.55
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.Wrap
            }

            Ctrl.SectionLabel {
                visible: root.cast.length > 0
                text: qsTr("Cast")
            }

            Ctrl.CastGrid {
                width: parent.width
                cast: root.cast
            }

            Ctrl.SectionLabel {
                visible: root.hasCrew
                text: qsTr("Crew")
            }

            Column {
                width: parent.width
                visible: root.hasCrew
                spacing: S.AppTheme.spacing2

                Ctrl.KeyValueRow {
                    width: parent.width
                    visible: root.directors.length > 0
                    label: qsTr("Director")
                    value: root.directors
                }

                Ctrl.KeyValueRow {
                    width: parent.width
                    visible: root.writers.length > 0
                    label: qsTr("Writers")
                    value: root.writers
                }
            }

            Item {
                width: 1
                visible: root.matched
                height: visible ? S.AppTheme.spacing14 : 0
            }

            Ctrl.TmdbCredit {
                width: parent.width
                visible: root.matched
            }

            Ctrl.SectionLabel { text: qsTr("File") }

            Ctrl.Card {
                width: parent.width
                variant: true
                implicitHeight: Math.max(_pathText.implicitHeight,
                                         _copyPath.implicitHeight)
                                + 2 * S.AppTheme.spacing14

                Ctrl.AppButton {
                    id: _copyPath

                    anchors.right: parent.right
                    anchors.top: parent.top
                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Plain
                    text: System.canShowInFileManager ? qsTr("Open in Explorer")
                                                      : qsTr("Copy")
                    enabled: !System.canShowInFileManager || root.present
                    onClicked: {
                        if (System.canShowInFileManager) {
                            System.showInFileManager(root.value("path"))
                            Library.noteUi("the file was shown in the file manager")
                            return
                        }
                        System.copyToClipboard(root.value("path"))
                        Library.noteUi("the path was copied from the file section")
                    }
                }

                Text {
                    id: _pathText

                    anchors.left: parent.left
                    anchors.right: _copyPath.left
                    anchors.rightMargin: S.AppTheme.spacing8
                    anchors.verticalCenter: _copyPath.verticalCenter
                    text: root.value("path")
                    color: S.AppTheme.textSecondary
                    font.family: S.AppTheme.monoFontFamily
                    font.pixelSize: S.AppTheme.fs11
                    elide: Text.ElideLeft
                }
            }

            Item {
                width: 1
                height: S.AppTheme.spacing8
            }

            Rectangle {
                id: _factsFrame

                width: parent.width
                implicitHeight: _facts.height + 2
                height: implicitHeight
                radius: S.AppTheme.radiusMedium
                color: S.AppTheme.outline

                layer.enabled: true
                layer.effect: OpacityMask {
                    maskSource: Rectangle {
                        width: _factsFrame.width
                        height: _factsFrame.height
                        radius: _factsFrame.radius
                    }
                }

                Grid {
                    id: _facts

                    x: 1
                    y: 1
                    width: parent.width - 2
                    columns: root.wide ? 2 : 1
                    spacing: 1

                    readonly property real cellWidth:
                        (width - (columns - 1) * spacing) / columns

                    readonly property real cellHeight: {
                        let tallest = 0
                        for (let i = 0; i < children.length; ++i) {
                            const cell = children[i]
                            if (cell.visible && cell.implicitHeight !== undefined)
                                tallest = Math.max(tallest, cell.implicitHeight)
                        }
                        return tallest
                    }

                    Fact {
                        visible: root.value("sizeText").length > 0
                        label: qsTr("Size")
                        value: root.value("sizeText")
                    }

                    Fact {
                        visible: root.value("durationText").length > 0
                        label: qsTr("Duration")
                        value: root.value("durationText")
                    }

                    Fact {
                        visible: root.value("resolution").length > 0
                        label: qsTr("Resolution")
                        value: {
                            const raw = root.value("resolution")
                            const line = S.Format.scanLine(root.info.width || 0,
                                                             root.info.height || 0)
                            const bits = [raw]
                            if (line.length > 0)
                                bits.push(line)
                            if (root.info.hdr === true)
                                bits.push(qsTr("HDR"))
                            return bits.join("  ·  ")
                        }
                    }

                    Fact {
                        visible: root.value("bitrateText").length > 0
                        label: qsTr("Bitrate")
                        value: root.value("bitrateText")
                    }

                    Fact {
                        visible: root.value("videoCodec").length > 0
                        label: qsTr("Video")
                        value: root.value("videoCodec")
                    }

                    Fact {
                        visible: root.value("audioCodec").length > 0
                        label: qsTr("Audio")
                        value: {
                            const codec = root.value("audioCodec")
                            const tracks = root.info.audioTrackCount || 0
                            return tracks > 1
                                   ? codec + "  ·  " + qsTr("%n track(s)", "", tracks)
                                   : codec
                        }
                    }

                    Fact {
                        visible: root.value("container").length > 0
                        label: qsTr("Container")
                        value: root.value("container")
                    }

                    Fact {
                        visible: (root.info.subtitleTrackCount || 0) > 0
                                 || root.attachedSubtitles.length > 0
                        label: qsTr("Subtitles")
                        value: {
                            const embedded = root.info.subtitleTrackCount || 0
                            const attached = root.attachedSubtitles.length
                            const text = qsTr("%n embedded", "", embedded)
                            return attached > 0
                                   ? text + "  ·  " + qsTr("%n attached", "", attached)
                                   : text
                        }
                    }

                    Fact {
                        visible: root.value("modified").length > 0
                        label: qsTr("Modified")
                        value: root.value("modified")
                    }

                    Fact {
                        visible: !root.present
                        label: qsTr("Status")
                        value: qsTr("Not found on disk")
                    }
                }
            }

            Item {
                width: 1
                height: S.AppTheme.spacing14
            }

            Text {
                width: parent.width
                visible: root.value("durationText").length === 0
                text: qsTr("Duration, resolution and codecs stay blank until the file has been played once or a scan reads them.")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs11
                wrapMode: Text.Wrap
            }
        }
    }

    FileDialog {
        id: _subtitleDialog

        title: qsTr("Choose a subtitle file")
        nameFilters: [qsTr("Subtitle files (*.srt *.ass *.ssa *.sub *.vtt *.idx)"),
                      qsTr("All files (*)")]
        currentFolder: root.videoFolderUrl

        onAccepted: {
            Library.attachSubtitle(root.handle, selectedFile)
            root.refreshAttachedSubtitle()
            root.refreshSiblingSubtitles()
        }
    }

    Ctrl.BottomSheet {
        id: _moreMenu

        z: 60
        title: root.metadata.title || root.displayName

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                destructive: true
                iconSource: S.Icons.close
                title: qsTr("Remove from library")
                subtitle: qsTr("The file itself is left alone")
                onClicked: {
                    _moreMenu.close()
                    root.takeOutOfLibrary()
                }
            }
        }
    }

    Ctrl.FindSubtitlesSheet {
        id: _findSubtitles

        z: 60
        handle: root.handle
        videoPath: root.value("path")
        tmdbId: root.metadata.tmdbId || 0
        season: root.metadata.season || 0
        episode: root.metadata.episode || 0

        onLanded: (path, displayName) => {
            root.refreshAttachedSubtitle()
            root.refreshSiblingSubtitles()
        }
    }

    Ctrl.BottomSheet {
        id: _subtitleMenu

        z: 60
        title: qsTr("Subtitles")

        onOpenedChanged: if (opened) root.refreshSiblingSubtitles()

        pinned: [
            Ctrl.SheetRow {
                width: parent ? parent.width : 0
                showRadio: false
                visible: SubtitleSearch.available && !System.isTelevision
                iconSource: S.Icons.search
                title: qsTr("Find subtitles online")
                subtitle: root.isEpisode
                          ? qsTr("From opensubtitles.com, saved beside the episode")
                          : qsTr("From opensubtitles.com, saved beside the film")
                onClicked: {
                    _subtitleMenu.close()
                    _findSubtitles.start()
                }
            },

            Ctrl.SheetRow {
                width: parent ? parent.width : 0
                showRadio: false
                iconSource: S.Icons.plus
                title: qsTr("Add a subtitle file")
                subtitle: qsTr("They all load when this video plays")
                onClicked: {
                    _subtitleMenu.close()
                    root.chooseSubtitleFile()
                }
            },

            Ctrl.Divider { width: parent ? parent.width : 0 }
        ]

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SectionLabel {
                visible: root.downloadedSubtitles.length > 0
                topPadding: 0
                text: qsTr("Downloaded")
            }

            Repeater {
                model: root.downloadedSubtitles

                delegate: Ctrl.SheetRow {
                    required property var modelData

                    width: parent.width
                    showRadio: false
                    enabled: false
                    iconSource: S.Icons.subtitles
                    title: modelData.displayName
                    subtitle: root.isEpisode
                              ? qsTr("From opensubtitles.com, beside the episode")
                              : qsTr("From opensubtitles.com, beside the film")
                }
            }

            Ctrl.SectionLabel {
                visible: root.pickedSubtitles.length > 0
                text: qsTr("Added by hand")
            }

            Repeater {
                model: root.pickedSubtitles

                delegate: Ctrl.SheetRow {
                    required property var modelData

                    width: parent.width
                    showRadio: false
                    enabled: false
                    iconSource: S.Icons.subtitles
                    title: modelData.displayName
                    subtitle: qsTr("A file you chose")
                }
            }

            Ctrl.SectionLabel {
                visible: root.siblingSubtitles.length > 0
                text: root.isEpisode ? qsTr("Found beside the episode")
                                     : qsTr("Found beside the film")
            }

            Repeater {
                model: root.siblingSubtitles

                delegate: Ctrl.SheetRow {
                    required property var modelData

                    width: parent.width
                    showRadio: false
                    enabled: false
                    iconSource: S.Icons.subtitles
                    title: modelData.title
                    subtitle: qsTr("By its name, or in a Subs folder")
                }
            }

            Ctrl.SectionLabel {
                visible: root.embeddedSubtitleCount > 0
                text: qsTr("Inside the file")
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                enabled: false
                visible: root.embeddedSubtitleCount > 0
                iconSource: S.Icons.subtitles
                title: qsTr("%n subtitle track(s)", "",
                            root.embeddedSubtitleCount)
                subtitle: qsTr("Chosen from the player while it plays")
            }

            Text {
                width: parent.width
                visible: root.attachedSubtitles.length === 0
                         && root.siblingSubtitles.length === 0
                         && root.embeddedSubtitleCount === 0
                topPadding: S.AppTheme.spacing8
                text: qsTr("This one has no subtitles at all yet. To take one away, delete the file from its folder.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }
        }
    }

    Connections {
        target: System

        function onSubtitleFilePicked(url) {
            if (!root.awaitingSubtitlePick)
                return
            root.awaitingSubtitlePick = false
            Library.attachSubtitle(root.handle, url)
            root.refreshAttachedSubtitle()
        }

        function onSubtitleFilePickCancelled() {
            root.awaitingSubtitlePick = false
        }
    }

    Connections {
        target: Library

        function onFileCountChanged() { root.refresh() }

        function onSubtitleAttached(fileHandle, displayName) {
            if (fileHandle === root.handle)
                root.refreshAttachedSubtitle()
        }
    }
}
