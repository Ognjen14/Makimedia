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
    property var attachedSubtitles: []

    signal backRequested()
    signal playRequested(string handle)
    signal playFromStartRequested(string handle)
    signal drawerRequested()
    signal fixMatchRequested(string handle)

    readonly property bool acceptsFocus: true

    function takeFocus() {
        _play.forceActiveFocus(Qt.TabFocusReason)
    }

    readonly property int safeX: Math.round(width * 0.045)
    readonly property int safeY: Math.round(height * 0.055)
    readonly property int contentWidth: width - 2 * safeX

    readonly property int titleSize: Math.round(height * 0.080)
    readonly property int actionHeight: Math.round(height * 0.066)

    readonly property int cardWidth: Math.round(contentWidth * 0.55)

    readonly property int overviewHeight: Math.round(S.AppTheme.fs15 * 1.35 * 3)

    readonly property bool hasRating: (metadata.rating || 0) > 0
    readonly property string certificationText: metadata.certification || ""

    readonly property string qualityText:
        S.Format.quality(info.width, info.height, info.hdr, root.value("videoCodec"))

    readonly property bool hasChips:
        hasRating || certificationText.length > 0 || qualityText.length > 0

    readonly property bool matched: (metadata.title || "").length > 0
    readonly property string shownTitle:
        matched ? metadata.title : (info.displayName || "")

    readonly property bool indexed: info.indexed === true
    readonly property bool finished: info.finished === true
    readonly property real positionSeconds:
        info.positionSeconds === undefined ? 0 : info.positionSeconds
    readonly property real progress:
        info.progress === undefined ? 0 : Math.max(0, Math.min(1, info.progress))
    readonly property real resumeSeconds:
        info.resumeSeconds === undefined ? 0 : info.resumeSeconds
    readonly property bool partiallyWatched: !finished && resumeSeconds > 0

    function value(key) {
        return info[key] === undefined ? "" : info[key]
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    TvAmbientBackdrop {
        topScrim: true
        source: {
            void Metadata.artworkRevision
            return root.matched
                    ? Metadata.backdropUrl(metadata.backdropPath || "", 780)
                    : ""
        }
    }

    TvCastStrip {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: root.safeX
        anchors.topMargin: root.safeY

        faceSize: Math.round(root.height * 0.118)
        cast: root.metadata.cast || []
        directors: root.metadata.directors || ""
        writers: root.metadata.writers || ""
    }

    Item {
        id: _bottomBox

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: root.safeX
        anchors.rightMargin: root.safeX
        anchors.bottomMargin: root.safeY
        height: _bottom.implicitHeight

        ColumnLayout {
            id: _bottom

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: S.AppTheme.spacing10

            Text {
                Layout.fillWidth: true
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
                Layout.fillWidth: true
                text: {
                    const bits = []
                    if ((metadata.year || 0) > 0)
                        bits.push(metadata.year)
                    if (root.value("durationText").length > 0)
                        bits.push(root.value("durationText"))
                    if ((metadata.genres || "").length > 0)
                        bits.push(metadata.genres)
                    return bits.join("  ·  ")
                }
                visible: text.length > 0
                color: S.AppTheme.textPrimary
                font.pixelSize: S.AppTheme.fs18
                elide: Text.ElideRight
            }

            Row {
                spacing: S.AppTheme.spacing8
                visible: root.hasChips

                Ctrl.Chip {
                    compact: true
                    enabled: false
                    visible: root.hasRating
                    text: S.Format.rating(metadata.rating)
                }
                Ctrl.Chip {
                    compact: true
                    enabled: false
                    visible: root.certificationText.length > 0
                    text: root.certificationText
                }
                Ctrl.Chip {
                    compact: true
                    enabled: false
                    visible: root.qualityText.length > 0
                    text: root.qualityText
                }
            }

            Text {
                Layout.preferredWidth: root.contentWidth
                Layout.maximumWidth: root.contentWidth
                Layout.preferredHeight: root.overviewHeight

                text: metadata.overview || ""
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
                Layout.maximumWidth: Math.round(root.contentWidth * 0.34)
                Layout.topMargin: S.AppTheme.spacing2
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

            Row {
                id: _actions

                Layout.topMargin: S.AppTheme.spacing8
                spacing: S.AppTheme.spacing12

                transformOrigin: Item.Left
                scale: implicitWidth > root.contentWidth
                       ? root.contentWidth / implicitWidth
                       : 1

                Ctrl.AppButton {
                    id: _play

                    height: root.actionHeight
                    text: root.finished
                          ? qsTr("Play again")
                          : (root.partiallyWatched ? qsTr("Resume") : qsTr("Play"))
                    variant: Ctrl.AppButton.Filled
                    iconSource: S.Icons.play
                    focus: true

                    KeyNavigation.right: root.partiallyWatched ? _fromStart : _watched
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
                    text: root.finished ? qsTr("Mark as unwatched")
                                        : qsTr("Mark as watched")
                    variant: Ctrl.AppButton.Outlined
                    iconSource: root.finished ? "" : S.Icons.check
                    enabled: root.indexed

                    KeyNavigation.left: root.partiallyWatched ? _fromStart : _play
                    KeyNavigation.right: _subtitles.visible
                                         ? _subtitles
                                         : (_fixMatch.visible ? _fixMatch : null)
                    onClicked: {
                        Library.setWatched(root.info.fileId, !root.finished)
                        root.refresh()
                    }
                }

                Ctrl.AppButton {
                    id: _subtitles

                    height: root.actionHeight
                    text: root.attachedSubtitles.length > 0
                          ? qsTr("Change subtitles") : qsTr("Subtitles")
                    variant: Ctrl.AppButton.Outlined
                    visible: !Streaming.connected

                    KeyNavigation.left: _watched
                    KeyNavigation.right: _fixMatch.visible ? _fixMatch : null
                    onClicked: _subtitleSheet.open()
                }

                Ctrl.AppButton {
                    id: _fixMatch

                    height: root.actionHeight
                    visible: root.metadata.suggested === true
                    text: qsTr("Fix match")
                    variant: Ctrl.AppButton.Outlined
                    enabled: Metadata.available && root.indexed

                    KeyNavigation.left: _subtitles.visible ? _subtitles : _watched
                    onClicked: root.openFixMatch()
                }
            }
        }
    }

    TvArtworkCard {
        id: _card

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: root.safeX
        anchors.topMargin: root.safeY

        readonly property int room:
            _bottomBox.y - root.safeY - S.AppTheme.spacing16

        width: Math.max(Math.round(root.contentWidth * 0.34),
                        Math.min(root.cardWidth,
                                 Math.round(room / S.AppTheme.backdropAspectRatio)))
        height: implicitHeight

        source: {
            void Metadata.artworkRevision
            return root.matched
                    ? Metadata.backdropUrl(metadata.backdropPath || "", 1280)
                    : ""
        }
    }

    TvSubtitleSheet {
        id: _subtitleSheet

        z: 50
        handle: root.handle

        onClosed: _subtitles.forceActiveFocus(Qt.TabFocusReason)
    }

    Ctrl.FixMatchSheet {
        id: _fixSheet

        pinned: root.metadata.pinned === true
        matched: root.matched
        currentTmdbId: root.metadata.tmdbId
        candidates: root.fixCandidates
        searching: root.fixSearching

        onSearchRequested: (text, tv) => {
            root.fixSearching = true
            Metadata.searchCandidates(text, tv)
        }
        onPicked: (candidate) => Metadata.pinMatch(root.handle, candidate)
        onUnpinRequested: Metadata.unpinMatch(root.handle)
        onNotMediaRequested: Metadata.clearMatch(root.handle)
        onClosed: Qt.callLater(root.takeFocus)
    }

    property var fixCandidates: []
    property bool fixSearching: false

    function openFixMatch() {
        root.fixCandidates = []
        const guess = Library.parsedNameFor(root.handle)
        _fixSheet.beginWith(guess.title || root.shownTitle, guess.isEpisode === true)
    }

    function refresh() {
        if (handle.length > 0)
            info = Library.fileInfo(handle)
    }

    function refreshAttachedSubtitle() {
        attachedSubtitles = Library.attachedSubtitles(root.handle)
    }

    onHandleChanged: {
        refresh()
        refreshAttachedSubtitle()
        takeFocus()
    }

    Component.onCompleted: {
        refresh()
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

        function onCandidatesReady(candidates) {
            if (!_fixSheet.opened)
                return
            root.fixSearching = false
            root.fixCandidates = candidates
        }
    }

}
