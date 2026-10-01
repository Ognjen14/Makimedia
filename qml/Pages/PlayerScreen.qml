pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Window
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal backRequested()
    signal fullScreenToggleRequested()
    signal playRequested(string handle)
    signal miniPlayerToggleRequested()

    property bool fullScreen: false
    property bool touchCapable: false
    property string fileHandle: ""
    property string failure: ""

    property var nextEpisode: ({})
    readonly property bool hasNextEpisode:
        (nextEpisode.handle || "").length > 0

    readonly property int nextEpisodeLead:
        AppSettings.autoPlayNextEpisode ? 30 : 10

    readonly property real secondsLeft:
        MpvPlayer.fileLoaded && MpvPlayer.duration > 0
        ? Math.max(0, MpvPlayer.duration - MpvPlayer.position)
        : -1

    readonly property bool nearTheEnd:
        root.secondsLeft >= 0 && root.secondsLeft <= root.nextEpisodeLead

    readonly property bool nextEpisodePrompt:
        root.hasNextEpisode && (root.nearTheEnd || MpvPlayer.endReached)

    readonly property real nextEpisodeFill:
        AppSettings.autoPlayNextEpisode
        ? Math.max(0, Math.min(1, 1 - root.secondsLeft / root.nextEpisodeLead))
        : 1

    function refreshNextEpisode() {
        nextEpisode = root.fileHandle.length > 0
                      ? Metadata.nextEpisodeFor(root.fileHandle)
                      : ({})
    }

    function playNextEpisode() {
        if (!root.hasNextEpisode)
            return
        root.playRequested(root.nextEpisode.handle)
    }

    onFileHandleChanged: {
        root.failure = ""
        root.refreshNextEpisode()
        root.cancelPendingSkip()
    }
    Component.onCompleted: {
        root.syncSpeedIndex()
        Library.noteUi("player page built")
        root.refreshNextEpisode()
    }

    Connections {
        target: MpvPlayer

        function onEndReachedChanged() {
            if (!MpvPlayer.endReached)
                return
            if (!AppSettings.autoPlayNextEpisode || !root.hasNextEpisode)
                return

            Library.noteUi("auto-playing the next episode")
            root.playNextEpisode()
        }
    }

    Connections {
        target: S.PopupRegistry

        function onHasOpenChanged() {
            if (!S.PopupRegistry.hasOpen && S.AppTheme.remoteNavigation)
                root.releaseControlFocus()
        }
    }

    readonly property bool miniPlayer: System.inMiniPlayer

    readonly property bool chromeVisible: controlsVisible && !miniPlayer

    property bool controlsVisible: true
    property bool locked: false
    property real speedBeforeHold: 1.0
    property bool holdingSpeed: false
    property real brightness: -1

    readonly property bool touchScheme: {
        const mode = AppSettings.playerControlScheme
        if (mode === "touch")
            return true
        if (mode === "desktop")
            return false
        return root.touchCapable
    }

    readonly property int skipSeconds: AppSettings.skipIntervalSeconds

    readonly property bool hardwareDecoding:
        !S.DevTools.forceSoftwareDecode
        && MpvPlayer.hwdecActive.length > 0
        && MpvPlayer.hwdecActive !== "no"

    readonly property bool awaitingPicture:
        System.isTelevision
        && (MpvPlayer.loading || !MpvPlayer.fileLoaded)

    Binding {
        target: Streaming
        property: "playingTitle"
        value: root.fileHandle.length > 0 ? root.playingTitle : ""
        when: Streaming.canConnect
        restoreMode: Binding.RestoreValue
    }

    Binding {
        target: Streaming
        property: "playingHandle"
        value: root.fileHandle
        when: Streaming.canConnect
        restoreMode: Binding.RestoreValue
    }

    readonly property string playingTitle: {
        const reload = MpvPlayer.mediaTitle

        if (root.fileHandle.length === 0)
            return reload

        const meta = Metadata.metadataForFile(root.fileHandle)
        const title = meta.title || ""

        if (meta.isEpisode === true && title.length > 0) {
            const parts = [title, S.Format.episodeCode(meta.season, meta.episode)]
            if ((meta.episodeTitle || "").length > 0)
                parts.push(meta.episodeTitle)
            return parts.join("  ·  ")
        }

        if (title.length > 0)
            return title

        const info = Library.fileInfo(root.fileHandle)
        if (info.indexed === true && (info.displayName || "").length > 0)
            return info.displayName

        return reload
    }

    readonly property var speedSteps: [0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0]
    property int speedIndex: 2

    readonly property var aspectModes: [
        { label: qsTr("Fit"),    aspect: "-1",     pan: 0.0 },
        { label: qsTr("Fill"),   aspect: "-1",     pan: 1.0 },
        { label: "16:9",         aspect: "16:9",   pan: 0.0 },
        { label: "16:10",        aspect: "16:10",  pan: 0.0 },
        { label: "4:3",          aspect: "4:3",    pan: 0.0 },
        { label: "1.85:1",       aspect: "1.85:1", pan: 0.0 },
        { label: "2.35:1",       aspect: "2.35:1", pan: 0.0 },
        { label: "2.39:1",       aspect: "2.39:1", pan: 0.0 }
    ]
    property int aspectIndex: 0

    focus: true

    readonly property real trackChipWidth: Math.max(120, Math.min(root.width * 0.25, 260))

    function trackLabel(track, fallback) {
        const title = track.title === undefined || track.title === "" ? "" : track.title
        const lang = track.lang === undefined || track.lang === "" ? "" : track.lang
        if (title.length > 0)
            return title
        if (lang.length > 0)
            return lang
        return fallback
    }

    readonly property bool showsPictureSubtitles: !System.isTelevision

    function pictureSubtitle(track) {
        const codec = String(track.codec === undefined ? "" : track.codec).toLowerCase()
        return codec === "dvd_subtitle" || codec === "hdmv_pgs_subtitle"
            || codec === "dvb_subtitle" || codec === "xsub"
    }

    function firstShowableSubtitle() {
        const tracks = MpvPlayer.subtitleTracks
        const wanted = AppSettings.subtitleLanguage
        let fallback = 0

        for (let i = 0; i < tracks.count; ++i) {
            const track = tracks.at(i)
            if (track.id === undefined || track.id <= 0
                    || root.pictureSubtitle(track))
                continue
            if (wanted.length > 0 && wanted !== "any"
                    && String(track.lang || "").toLowerCase()
                       .indexOf(wanted.toLowerCase()) === 0)
                return track.id
            if (fallback === 0)
                fallback = track.id
        }
        return fallback
    }

    function trackDetail(track) {
        const parts = []
        if (track.codec !== undefined && track.codec !== "")
            parts.push(String(track.codec).toUpperCase())
        parts.push(track.external === true ? qsTr("external file") : qsTr("embedded"))
        return parts.join("  ·  ")
    }

    Keys.onPressed: (event) => {
        if (System.usesWindowGeometry || root.locked)
            return

        if (S.PopupRegistry.hasOpen)
            return

        switch (event.key) {
        case Qt.Key_Select:
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
        case Qt.Key_MediaTogglePlayPause:
            root.togglePause()
            root.revealControls()
            event.accepted = true
            break
        case Qt.Key_Left:
            root.skip(-root.skipSeconds, false)
            event.accepted = true
            break
        case Qt.Key_Right:
            root.skip(root.skipSeconds, false)
            event.accepted = true
            break
        case Qt.Key_Down:
            root.focusPlaybackControls()
            event.accepted = true
            break
        case Qt.Key_Up:
            if (!root.focusNextEpisodeOffer())
                root.revealControls()
            event.accepted = true
            break
        case Qt.Key_MediaPlay:
        case Qt.Key_MediaPause:
        case Qt.Key_MediaStop:
        case Qt.Key_MediaNext:
        case Qt.Key_MediaPrevious:
            break
        }
    }

    function revealControls() {
        if (locked)
            return
        controlsVisible = true
        _hideTimer.restart()
    }

    function hideControls() {
        controlsVisible = false
        _hideTimer.stop()
    }

    function toggleControls() {
        if (locked)
            return
        if (controlsVisible)
            hideControls()
        else
            revealControls()
    }

    function setLocked(value) {
        locked = value
        if (value)
            hideControls()
        else
            revealControls()
    }

    property real _pendingSkip: 0
    property bool _seekForward: true
    property real _skipOrigin: 0

    function skip(seconds, reveal) {
        if (root._pendingSkip === 0)
            root._skipOrigin = MpvPlayer.position

        root._pendingSkip += seconds

        if (MpvPlayer.duration > 0) {
            root._pendingSkip =
                Math.max(-root._skipOrigin,
                         Math.min(root._pendingSkip,
                                  MpvPlayer.duration - root._skipOrigin))
        }

        const total = Math.round(root._pendingSkip)
        root._seekForward = total >= 0
        _volumeOsd.hide()
        _brightnessOsd.hide()
        _seekOsd.flash((total > 0 ? "+" : "") + total + "s",
                       S.Format.clock(root._skipOrigin + root._pendingSkip))
        _skipCommit.restart()

        if (reveal !== false)
            revealControls()
    }

    Timer {
        id: _skipCommit

        interval: 250
        onTriggered: {
            const total = root._pendingSkip
            root._pendingSkip = 0
            if (total !== 0)
                MpvPlayer.seekRelative(total)
        }
    }

    function cancelPendingSkip() {
        _skipCommit.stop()
        root._pendingSkip = 0
    }

    function seekTo(seconds, reveal) {
        root.cancelPendingSkip()
        MpvPlayer.seekAbsolute(seconds)
        if (reveal !== false)
            revealControls()
    }

    function showScrubPreview(seconds) {
        const delta = Math.round(seconds - MpvPlayer.position)
        _centreOsd.show(S.Format.clock(seconds),
                        (delta > 0 ? "+" : "") + delta + "s")
    }

    function hideCentreOsd() {
        _centreOsd.hide()
    }

    function findSubtitlesOnline() {
        const info = Library.fileInfo(root.fileHandle)
        const meta = Metadata.metadataForFile(root.fileHandle)

        _findSubtitles.handle = root.fileHandle
        _findSubtitles.videoPath = info.path || ""
        _findSubtitles.tmdbId = meta.tmdbId || 0
        _findSubtitles.season = meta.season || 0
        _findSubtitles.episode = meta.episode || 0
        _findSubtitles.start()
    }

    function syncSpeedIndex() {
        let nearest = 2
        let best = Number.MAX_VALUE
        for (let i = 0; i < speedSteps.length; ++i) {
            const gap = Math.abs(speedSteps[i] - MpvPlayer.speed)
            if (gap < best) {
                best = gap
                nearest = i
            }
        }
        speedIndex = nearest
    }

    function applySpeed(index, wrap) {
        const count = speedSteps.length
        const next = wrap
                     ? ((index % count) + count) % count
                     : Math.max(0, Math.min(count - 1, index))
        speedIndex = next
        MpvPlayer.speed = speedSteps[next]
        _centreOsd.flash(speedSteps[next].toFixed(2) + "×", qsTr("Speed"))
        revealControls()
    }

    function cycleSpeed(direction) {
        applySpeed(speedIndex + direction, false)
    }

    function stepSpeed() {
        applySpeed(speedIndex + 1, true)
    }

    function beginHoldSpeed() {
        if (holdingSpeed || !MpvPlayer.fileLoaded)
            return
        holdingSpeed = true
        speedBeforeHold = MpvPlayer.speed
        MpvPlayer.speed = AppSettings.holdToSpeedMultiplier
        _centreOsd.flash(S.Format.speed(AppSettings.holdToSpeedMultiplier) + "×",
                         qsTr("Hold for speed"))
    }

    function endHoldSpeed() {
        if (!holdingSpeed)
            return
        holdingSpeed = false
        MpvPlayer.speed = speedBeforeHold
    }

    property real zoomFactor: 1
    property real panX: 0
    property real panY: 0

    readonly property bool zoomed: Math.abs(zoomFactor - 1) > 0.01
                                   || Math.abs(panX) > 0.001
                                   || Math.abs(panY) > 0.001

    property real committedZoom: 1
    property real committedPanX: 0
    property real committedPanY: 0

    readonly property real previewScale: zoomFactor / committedZoom
    readonly property real previewOffsetX: (panX - committedPanX) * width
    readonly property real previewOffsetY: (panY - committedPanY) * height

    Connections {
        target: MpvPlayer

        function onVideoZoomChanged() {
            root.committedZoom = Math.pow(2, MpvPlayer.videoZoom)
        }

        function onVideoPanChanged() {
            root.committedPanX = MpvPlayer.videoPanX
            root.committedPanY = MpvPlayer.videoPanY
        }
    }

    Timer {
        id: _zoomCommit

        interval: 180
        onTriggered: root.flushZoom()
    }

    function flushZoom() {
        _zoomCommit.stop()
        MpvPlayer.videoZoom = Math.log(root.zoomFactor) / Math.LN2
        MpvPlayer.videoPanX = root.panX
        MpvPlayer.videoPanY = root.panY
    }

    function setZoomFactor(factor, live) {
        zoomFactor = Math.max(0.25, Math.min(4.0, factor))

        if (zoomFactor <= 1.0) {
            panX = 0
            panY = 0
        }

        const label = Math.round(zoomFactor * 100) + "%"

        if (live === true) {
            _centreOsd.show(label, qsTr("Zoom"))
            _zoomCommit.restart()
            return
        }

        _centreOsd.flash(label, qsTr("Zoom"))
        _zoomCommit.restart()
        revealControls()
    }

    function nudgeZoom(step) {
        setZoomFactor(root.zoomFactor * (step > 0 ? 1.1 : 1 / 1.1))
    }

    function panBy(dx, dy) {
        if (!root.zoomed)
            return
        const limit = Math.max(0, (root.zoomFactor - 1) / 2)
        panX = Math.max(-limit, Math.min(limit, panX + dx))
        panY = Math.max(-limit, Math.min(limit, panY + dy))
        _zoomCommit.restart()
    }

    function endZoomGesture() {
        flushZoom()
        _centreOsd.hide()
    }

    function resetZoom() {
        zoomFactor = 1
        panX = 0
        panY = 0
        flushZoom()
        _centreOsd.flash(qsTr("100%"), qsTr("Zoom"))
        revealControls()
    }

    function aspectRatioOf(spec) {
        const text = String(spec)
        if (text === "-1")
            return -1
        const parts = text.split(":")
        if (parts.length === 2)
            return Number(parts[0]) / Number(parts[1])
        return Number(text)
    }

    function applyAspect(index) {
        const count = aspectModes.length
        aspectIndex = ((index % count) + count) % count
        const mode = aspectModes[aspectIndex]
        MpvPlayer.aspectOverride = mode.aspect
        MpvPlayer.panscan = mode.pan
        MpvPlayer.setSurfaceAspect(root.aspectRatioOf(mode.aspect), mode.pan > 0)
        zoomFactor = 1
        panX = 0
        panY = 0
        MpvPlayer.resetVideoZoom()
        _centreOsd.flash(mode.label, qsTr("Aspect"))
        revealControls()
    }

    function cycleAspect() {
        applyAspect(aspectIndex + 1)
    }

    function aspectLabel() {
        return aspectModes[aspectIndex].label
    }

    function subtitleLabel() {
        if (MpvPlayer.subtitleTrackId <= 0)
            return qsTr("Subs · off")
        void MpvPlayer.subtitleTracks.revision
        const track = MpvPlayer.subtitleTracks.byId(MpvPlayer.subtitleTrackId)
        if (track.id !== undefined) {
            return qsTr("Subs · %1").arg(
                trackLabel(track, qsTr("Track %1").arg(track.id)))
        }
        return qsTr("Subs · on")
    }

    function audioLabel() {
        void MpvPlayer.audioTracks.revision
        const track = MpvPlayer.audioTracks.byId(MpvPlayer.audioTrackId)
        if (track.id !== undefined) {
            return qsTr("Audio · %1").arg(
                trackLabel(track, qsTr("Track %1").arg(track.id)))
        }
        return qsTr("Audio · %n track(s)", "", MpvPlayer.audioTracks.count)
    }

    function toggleMute() {
        const next = !MpvPlayer.muted
        MpvPlayer.muted = next
        if (_seekOsd.visible)
            return
        _volumeOsd.flash(next ? qsTr("Muted") : MpvPlayer.volume + "%",
                         qsTr("Volume"),
                         next ? 0 : MpvPlayer.volume / 130)
    }

    function setVolume(value) {
        const next = Math.max(0, Math.min(130, Math.round(value)))
        MpvPlayer.volume = next
        AppSettings.playerVolume = next
        if (next > 0)
            MpvPlayer.muted = false
        if (_seekOsd.visible)
            return
        _volumeOsd.flash(next + "%", qsTr("Volume"), next / 130)
    }

    function nudgeVolume(delta) {
        setVolume(MpvPlayer.volume + delta)
    }

    function setBrightness(value) {
        if (!System.canSetBrightness)
            return
        const next = Math.max(0.01, Math.min(1.0, value))
        brightness = next
        System.setScreenBrightness(next)
        if (_seekOsd.visible)
            return
        _brightnessOsd.flash(Math.round(next * 100) + "%",
                             qsTr("Brightness"), next)
    }

    function adjustBrightness(delta) {
        if (!System.canSetBrightness)
            return
        if (brightness < 0)
            brightness = System.systemBrightness()
        setBrightness(brightness + delta)
    }

    property bool pendingToolFocus: false

    function focusPlaybackControls() {
        revealControls()
        if (_toolChips.visible)
            _toolChips.focusFirst()
        else
            pendingToolFocus = true
    }

    function releaseControlFocus() {
        if (!root.focusNextEpisodeOffer())
            root.forceActiveFocus(Qt.TabFocusReason)
    }

    function focusNextEpisodeOffer() {
        if (!S.AppTheme.remoteNavigation || !root.nextEpisodePrompt
                || root.locked || root.miniPlayer)
            return false
        _nextEpisodeButton.forceActiveFocus(Qt.TabFocusReason)
        return true
    }

    function togglePause() {
        MpvPlayer.togglePause()
        revealControls()
    }

    function openAudioTracks() {
        _audioSheet.open()
        revealControls()
    }

    function openSubtitleTracks() {
        _subtitleSheet.open()
        revealControls()
    }

    function openSubtitleStyle() {
        _subtitleStyleSheet.open()
    }

    function openDelays() {
        _delaySheet.open()
        revealControls()
    }

    property bool awaitingSubtitlePick: false
    property bool awaitingFolderAccess: false
    property int folderAccessRevision: 0

    function chooseSubtitleFile() {
        if (System.usesSystemFilePicker) {
            awaitingSubtitlePick = true
            System.pickSubtitleFile()
            return
        }
        _subtitleDialog.open()
    }

    Connections {
        target: System

        function onSubtitleFilePicked(url) {
            if (!root.awaitingSubtitlePick)
                return
            root.awaitingSubtitlePick = false
            root.attachSubtitleFile(url)
        }

        function onSubtitleFilePickCancelled() {
            root.awaitingSubtitlePick = false
        }

        function onSubtitleFolderAccessResult(granted) {
            if (!root.awaitingFolderAccess)
                return
            root.awaitingFolderAccess = false
            root.folderAccessRevision++
            if (!granted)
                return

            if (Library.canRequestSubtitleFolder(root.fileHandle)) {
                _toast.show(qsTr("Choose this video's folder or the Subs folder inside it"))
                return
            }

            const tracks = Library.siblingSubtitleTracks(root.fileHandle)
            if (tracks.length === 0) {
                _toast.show(qsTr("No subtitles in that folder match this video"))
                return
            }

            for (let i = tracks.length - 1; i >= 0; i--)
                MpvPlayer.addSubtitleFile(tracks[i].url, tracks[i].title)
            _toast.show(qsTr("Loaded %n subtitle(s)", "", tracks.length))
        }
    }

    function attachSubtitleFile(url) {
        const handle = Library.handleFromUrl(url)

        const name = root.fileHandle.length > 0
                     ? Library.attachSubtitle(root.fileHandle, url)
                     : ""

        const mpvUrl = Library.subtitleMpvUrl(handle)
        if (mpvUrl.length === 0) {
            _toast.show(qsTr("That subtitle could not be opened"))
            return
        }

        MpvPlayer.addSubtitleFile(mpvUrl, name)
        _toast.show(qsTr("Added %1").arg(name))
    }

    function openAudioFocusTests() {
        _focusSheet.focusState = Platform.audioFocusState()
        _focusSheet.open()
        revealControls()
    }

    function openFile() {
        _fileDialog.open()
    }

    function handleBack() {
        if (locked) {
            setLocked(false)
            return true
        }

        if (fullScreen && !System.isTelevision) {
            fullScreenToggleRequested()
            return true
        }
        return false
    }

    Timer {
        id: _hideTimer

        interval: S.AppTheme.playerControlsHideMs
        running: root.controlsVisible && !MpvPlayer.paused && MpvPlayer.fileLoaded
                 && !root.locked
        onTriggered: root.controlsVisible = false
    }

    Rectangle {
        anchors.fill: parent
        color: (!System.isTelevision || root.awaitingPicture)
               ? "black"
               : "transparent"
    }

    Item {
        anchors.fill: parent
        clip: true

        Loader {
            id: _video

            anchors.fill: parent
            active: !System.isTelevision
            sourceComponent: MpvRenderItem {
                controller: MpvPlayer
            }

            transform: [
                Scale {
                    origin.x: _video.width / 2
                    origin.y: _video.height / 2
                    xScale: root.previewScale
                    yScale: root.previewScale
                },
                Translate {
                    x: root.previewOffsetX
                    y: root.previewOffsetY
                }
            ]
        }
    }

    Ctrl.SubtitleOverlay {
        anchors.fill: parent

        text: System.isTelevision ? MpvPlayer.subtitleText : ""
        scalePercent: AppSettings.subtitleScalePercent
        edgeStyle: AppSettings.subtitleEdgeStyle
        position: AppSettings.subtitlePosition
        textColor: AppSettings.subtitleColor
        bold: AppSettings.subtitleBold
    }

    Loader {
        id: _input

        anchors.fill: parent
        sourceComponent: root.touchScheme ? _touchInput : _desktopInput
    }

    Component {
        id: _touchInput

        PlayerTouchInput { player: root }
    }


    Component {
        id: _desktopInput

        PlayerDesktopInput { player: root }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: MpvPlayer.buffering || MpvPlayer.loading
        visible: running
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: !MpvPlayer.fileLoaded && !MpvPlayer.buffering && !MpvPlayer.loading

        iconSource: (MpvPlayer.ready && root.failure.length === 0)
                    ? S.Icons.noVideo : S.Icons.alertCircle

        title: {
            if (root.failure.length > 0)
                return qsTr("This one will not play")
            return MpvPlayer.ready ? qsTr("No file open")
                                   : qsTr("Playback engine failed")
        }

        message: {
            if (root.failure.length > 0)
                return root.failure
            return MpvPlayer.ready
                   ? qsTr("Open a video file to start playing.")
                   : qsTr("mpv could not be initialised. Check the log.")
        }

        actionText: root.failure.length > 0
                    ? qsTr("Close")
                    : (MpvPlayer.ready ? qsTr("Open file") : "")

        onActionTriggered: {
            if (root.failure.length > 0)
                root.backRequested()
            else
                root.openFile()
        }
    }

    FileDialog {
        id: _fileDialog

        title: qsTr("Open video")
        nameFilters: [qsTr("Video files (*.mkv *.mp4 *.avi *.mov *.webm *.m4v *.ts *.wmv)"),
                      qsTr("All files (*)")]
        onAccepted: MpvPlayer.open(selectedFile)
    }

    Item {
        id: _topBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.touchScheme ? 72 : 64
        opacity: root.chromeVisible ? 1 : 0
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.72) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        Ctrl.IconButton {
            id: _back

            anchors.left: parent.left
            anchors.leftMargin: S.AppTheme.spacing12
            anchors.verticalCenter: parent.verticalCenter
            iconSource: S.Icons.chevronLeft
            tintColor: "#FFFFFF"
            accessibleName: qsTr("Back to library")
            onClicked: root.backRequested()
        }

        Column {
            anchors.left: _back.right
            anchors.leftMargin: S.AppTheme.spacing12
            anchors.right: _topActions.left
            anchors.rightMargin: S.AppTheme.spacing12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                width: parent.width
                text: root.playingTitle.length > 0
                      ? root.playingTitle
                      : qsTr("Nothing playing")
                color: "#FFFFFF"
                font.pixelSize: S.AppTheme.fs15
                font.weight: Font.Medium
                elide: Text.ElideMiddle
            }

            Row {
                width: parent.width
                visible: MpvPlayer.fileLoaded
                spacing: 0

                Text {
                    text: [MpvPlayer.videoResolution, MpvPlayer.audioCodec]
                          .filter(function (part) { return part && part.length > 0 })
                          .join("  ·  ")
                    color: "#FFFFFF"
                    opacity: 0.66
                    font.pixelSize: S.AppTheme.fs11
                    elide: Text.ElideRight
                }

                Text {
                    visible: text.length > 0
                    text: (MpvPlayer.videoResolution.length > 0
                           || MpvPlayer.audioCodec.length > 0) ? "  ·  " : ""
                    color: "#FFFFFF"
                    opacity: 0.66
                    font.pixelSize: S.AppTheme.fs11
                }

                Text {
                    text: root.hardwareDecoding
                          ? qsTr("hw: %1").arg(MpvPlayer.hwdecActive)
                          : qsTr("software decoding")
                    color: root.hardwareDecoding
                           ? S.AppTheme.success
                           : S.AppTheme.warning
                    font.pixelSize: S.AppTheme.fs11
                    font.weight: Font.Medium
                }
            }
        }

        Row {
            id: _topActions

            anchors.right: parent.right
            anchors.rightMargin: S.AppTheme.spacing12
            anchors.verticalCenter: parent.verticalCenter
            spacing: S.AppTheme.spacing4

            Ctrl.IconButton {
                iconSource: S.Icons.miniPlayer
                tintColor: "#FFFFFF"
                visible: System.supportsMiniPlayer
                accessibleName: qsTr("Mini player")
                enabled: MpvPlayer.fileLoaded
                onClicked: root.miniPlayerToggleRequested()
            }

            Ctrl.IconButton {
                iconSource: S.Icons.lock
                tintColor: "#FFFFFF"
                visible: !System.isTelevision
                accessibleName: qsTr("Lock controls")
                onClicked: root.setLocked(true)
            }
        }
    }

    Row {
        id: _transport

        anchors.centerIn: parent
        spacing: root.touchScheme ? 64 : 56
        opacity: root.chromeVisible ? 1 : 0
        visible: opacity > 0.01 && MpvPlayer.fileLoaded

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        Ctrl.PlayerButton {
            id: _skipBack

            anchors.verticalCenter: parent.verticalCenter
            visible: !System.isTelevision
            iconSource: S.Icons.skipBack
            accessibleName: qsTr("Skip back")
            KeyNavigation.right: _playPause
            Keys.onUpPressed: root.releaseControlFocus()
            onClicked: root.skip(-root.skipSeconds)
        }

        Ctrl.PlayerButton {
            id: _playPause

            anchors.verticalCenter: parent.verticalCenter
            primary: true
            iconSource: MpvPlayer.paused ? S.Icons.play : S.Icons.pause
            accessibleName: MpvPlayer.paused ? qsTr("Play") : qsTr("Pause")
            KeyNavigation.left: System.isTelevision ? null : _skipBack
            KeyNavigation.right: System.isTelevision ? null : _skipForward
            Keys.onUpPressed: root.releaseControlFocus()
            onClicked: root.togglePause()
        }

        Ctrl.PlayerButton {
            id: _skipForward

            anchors.verticalCenter: parent.verticalCenter
            visible: !System.isTelevision
            iconSource: S.Icons.skipForward
            accessibleName: qsTr("Skip forward")
            KeyNavigation.left: _playPause
            Keys.onUpPressed: root.releaseControlFocus()
            onClicked: root.skip(root.skipSeconds)
        }
    }

    Item {
        id: _bottomBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: _bottomColumn.implicitHeight + 2 * S.AppTheme.spacing16
        opacity: root.chromeVisible ? 1 : 0
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.82) }
            }
        }

        Column {
            id: _bottomColumn

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: S.AppTheme.spacing16
            anchors.rightMargin: S.AppTheme.spacing16
            anchors.bottomMargin: S.AppTheme.spacing16
            spacing: S.AppTheme.spacing12

            Item {
                width: parent.width
                height: root.touchScheme
                        ? S.AppTheme.touchTargetMinimum
                        : S.AppTheme.touchTargetMinimum / 2

                Text {
                    id: _elapsed

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: S.Format.clock(_seek.displayPosition)
                    color: "#FFFFFF"
                    font.pixelSize: S.AppTheme.fs12
                }

                Text {
                    id: _remaining

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: S.Format.clock(MpvPlayer.duration)
                    color: "#FFFFFF"
                    font.pixelSize: S.AppTheme.fs12
                }

                Ctrl.SeekBar {
                    id: _seek

                    anchors.left: _elapsed.right
                    anchors.right: _remaining.left
                    anchors.leftMargin: S.AppTheme.spacing12
                    anchors.rightMargin: S.AppTheme.spacing12
                    anchors.verticalCenter: parent.verticalCenter
                    enabled: MpvPlayer.fileLoaded
                    position: MpvPlayer.position
                    duration: MpvPlayer.duration
                    handleSize: root.touchScheme
                                ? S.AppTheme.seekBarHandleSize + 4
                                : S.AppTheme.seekBarHandleSize
                    previewEnabled: MpvPlayer.fileLoaded
                    previewSource: SeekPreview.frameUrl

                    onSeekRequested: (seconds) => root.seekTo(seconds)
                    onScrubbing: root.revealControls()
                    onPreviewRequested: (seconds) => SeekPreview.requestFrame(seconds)
                }
            }

            Flow {
                id: _toolChips

                width: parent.width
                spacing: S.AppTheme.spacing8

                function focusFirst() {
                    for (let i = 0; i < children.length; ++i) {
                        if (children[i].visible && children[i].enabled) {
                            children[i].forceActiveFocus(Qt.TabFocusReason)
                            return
                        }
                    }
                }

                function step(forward) {
                    const current = Window.activeFocusItem
                    if (!current)
                        return
                    const next = current.nextItemInFocusChain(forward)
                    if (next && next.parent === _toolChips)
                        next.forceActiveFocus(Qt.TabFocusReason)
                }

                onVisibleChanged: {
                    if (visible && root.pendingToolFocus) {
                        root.pendingToolFocus = false
                        focusFirst()
                    }
                }

                Keys.onLeftPressed: _toolChips.step(false)
                Keys.onRightPressed: _toolChips.step(true)
                Keys.onUpPressed: root.releaseControlFocus()

                Ctrl.ToolChip {
                    text: root.audioLabel()
                    maxTextWidth: root.trackChipWidth
                    active: MpvPlayer.audioTrackId > 0
                    enabled: MpvPlayer.audioTracks.count > 0
                    onClicked: root.openAudioTracks()
                }

                Ctrl.ToolChip {
                    text: root.subtitleLabel()
                    maxTextWidth: root.trackChipWidth
                    active: MpvPlayer.subtitleTrackId > 0
                    onClicked: root.openSubtitleTracks()
                }

                Ctrl.ToolChip {
                    text: qsTr("Subtitle style")
                    enabled: MpvPlayer.subtitleTrackId > 0
                    onClicked: root.openSubtitleStyle()
                }

                Ctrl.ToolChip {
                    text: qsTr("Delay %1s").arg(MpvPlayer.subDelay.toFixed(2))
                    active: Math.abs(MpvPlayer.subDelay) > 0.001
                            || Math.abs(MpvPlayer.audioDelay) > 0.001
                    onClicked: root.openDelays()
                }

                Ctrl.ToolChip {
                    text: root.aspectLabel()
                    active: MpvPlayer.aspectOverride !== "-1"
                    onClicked: root.cycleAspect()
                }

                Ctrl.ToolChip {
                    visible: root.zoomed
                    active: true
                    text: qsTr("Zoom %1%").arg(Math.round(root.zoomFactor * 100))
                    onClicked: root.resetZoom()
                }

                Ctrl.ToolChip {
                    text: MpvPlayer.speed.toFixed(2) + "×"
                    active: Math.abs(MpvPlayer.speed - 1.0) > 0.01
                    onClicked: root.stepSpeed()
                }

                Ctrl.ToolChip {
                    text: qsTr("Interrupt")
                    visible: S.DevTools.audioFocusTools
                    onClicked: root.openAudioFocusTests()
                }
            }
        }
    }

    Item {
        id: _nextEpisodeButton

        activeFocusOnTab: S.AppTheme.remoteNavigation

        scale: activeFocus ? S.AppTheme.focusScale : 1.0

        Behavior on scale {
            NumberAnimation { duration: 120 }
        }

        onVisibleChanged: {
            if (visible && S.AppTheme.remoteNavigation)
                forceActiveFocus(Qt.TabFocusReason)
        }

        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Select:
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Space:
                root.playNextEpisode()
                event.accepted = true
                break
            case Qt.Key_Down:
                root.focusPlaybackControls()
                event.accepted = true
                break
            case Qt.Key_Up:
                root.revealControls()
                event.accepted = true
                break
            }
        }

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.bottomMargin: root.controlsVisible
                              ? _bottomBar.height + S.AppTheme.spacing8
                              : S.AppTheme.spacing16
        z: 25
        width: _nextEpisodeRow.implicitWidth + 2 * S.AppTheme.spacing20
        height: S.AppTheme.controlHeightMedium
        opacity: root.nextEpisodePrompt && !root.locked && !root.miniPlayer
                 ? 1 : 0
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: 160 }
        }

        Rectangle {
            anchors.fill: parent
            radius: S.AppTheme.radiusPill
            color: Qt.rgba(0, 0, 0, 0.55)
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.30)

            Rectangle {
                readonly property real sweep:
                    Math.max(0, Math.min(1, root.nextEpisodeFill))

                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: height + (parent.width - height) * sweep
                radius: height / 2
                color: S.AppTheme.primaryUnderWhite
            }
        }

        Rectangle {
            anchors.fill: parent
            z: 1
            radius: S.AppTheme.radiusPill
            color: "transparent"
            border.width: S.AppTheme.focusThickness
            border.color: S.AppTheme.focus
            visible: _nextEpisodeButton.activeFocus
        }

        Row {
            id: _nextEpisodeRow

            anchors.centerIn: parent
            spacing: S.AppTheme.spacing8

            Ctrl.ThemedIcon {
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                source: S.Icons.nextEpisode
                tintColor: "#FFFFFF"
                showPlaceholder: false
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Next episode")
                color: "#FFFFFF"
                font.pixelSize: S.AppTheme.fs14
                font.weight: Font.Medium
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.playNextEpisode()
        }
    }

    MouseArea {
        id: _miniSurface

        anchors.fill: parent
        z: 26
        visible: root.miniPlayer
        enabled: root.miniPlayer
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        onPressed: System.startWindowMove(Window.window)
        onDoubleClicked: root.miniPlayerToggleRequested()

        Item {
            anchors.fill: parent
            opacity: _miniSurface.containsMouse ? 1 : 0
            visible: opacity > 0.01

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }

            Rectangle {
                anchors.fill: parent
                color: Qt.rgba(0, 0, 0, 0.35)
            }

            Ctrl.PlayerButton {
                anchors.centerIn: parent
                primary: true
                iconSource: MpvPlayer.paused ? S.Icons.play : S.Icons.pause
                accessibleName: MpvPlayer.paused ? qsTr("Play") : qsTr("Pause")
                onClicked: root.togglePause()
            }

            Ctrl.IconButton {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: S.AppTheme.spacing4
                iconSource: S.Icons.miniPlayer
                tintColor: "#FFFFFF"
                accessibleName: qsTr("Back to the full window")
                onClicked: root.miniPlayerToggleRequested()
            }
        }
    }

    Ctrl.Snackbar {
        id: _toast

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: _bottomBar.top
        anchors.margins: S.AppTheme.spacing16
        z: 30
    }

    Ctrl.PlayerOsd {
        id: _centreOsd

        anchors.centerIn: parent
        z: 20
    }

    Ctrl.PlayerOsd {
        id: _seekOsd

        x: root._seekForward
           ? parent.width - width - S.AppTheme.spacing24
           : S.AppTheme.spacing24
        anchors.verticalCenter: parent.verticalCenter
        panelOpacity: 0.55
        z: 20
    }

    Ctrl.PlayerOsd {
        id: _volumeOsd

        anchors.right: parent.right
        anchors.rightMargin: S.AppTheme.spacing24
        anchors.verticalCenter: parent.verticalCenter
        z: 20
    }

    Ctrl.PlayerOsd {
        id: _brightnessOsd

        anchors.left: parent.left
        anchors.leftMargin: S.AppTheme.spacing24
        anchors.verticalCenter: parent.verticalCenter
        z: 20
    }

    Item {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing16
        width: _lockChip.width
        height: _lockChip.height
        visible: root.locked
        z: 40

        Ctrl.ToolChip {
            id: _lockChip

            text: root.touchScheme
                  ? qsTr("Controls locked · double-tap to unlock")
                  : qsTr("Controls locked · double-click to unlock")
            onClicked: root.setLocked(false)
        }
    }

    Ctrl.BottomSheet {
        id: _audioSheet

        restoresFocus: false

        z: 50
        title: qsTr("Audio track")

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                visible: MpvPlayer.audioTracks.count === 0
                showRadio: false
                title: qsTr("No audio tracks")
                enabled: false
            }

            Repeater {
                model: MpvPlayer.audioTracks

                delegate: Ctrl.SheetRow {
                    required property var model

                    width: parent.width
                    title: root.trackLabel(model, qsTr("Track %1").arg(model.id))
                    subtitle: root.trackDetail(model)
                    selected: MpvPlayer.audioTrackId === model.id

                    onClicked: {
                        MpvPlayer.audioTrackId = model.id
                        _audioSheet.close()
                    }
                }
            }
        }
    }

    Ctrl.FindSubtitlesSheet {
        id: _findSubtitles

        z: 50

        onLanded: (path, displayName) => {
            MpvPlayer.addSubtitleFile(path, displayName)
        }
    }

    Ctrl.BottomSheet {
        id: _subtitleSheet

        restoresFocus: false

        z: 50
        title: qsTr("Subtitles")

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                title: qsTr("Off")
                selected: MpvPlayer.subtitleTrackId <= 0

                onClicked: {
                    MpvPlayer.subtitleTrackId = 0
                    _subtitleSheet.close()
                }
            }

            Repeater {
                model: MpvPlayer.subtitleTracks

                delegate: Ctrl.SheetRow {
                    required property var model

                    readonly property bool unavailable:
                        !root.showsPictureSubtitles && root.pictureSubtitle(model)

                    width: parent.width
                    title: root.trackLabel(model, qsTr("Track %1").arg(model.id))
                    subtitle: unavailable
                              ? qsTr("A picture subtitle, which this television cannot show")
                              : root.trackDetail(model)
                    selected: MpvPlayer.subtitleTrackId === model.id
                    enabled: !unavailable
                    opacity: unavailable ? 0.5 : 1

                    onClicked: {
                        MpvPlayer.subtitleTrackId = model.id
                        _subtitleSheet.close()
                    }
                }
            }

            Ctrl.Divider {
                width: parent.width
                visible: !System.isTelevision && !Streaming.connected
            }

            Ctrl.SheetRow {
                width: parent.width
                visible: SubtitleSearch.available && !Streaming.connected
                         && !System.isTelevision
                showRadio: false
                iconSource: S.Icons.search
                title: qsTr("Find subtitles online")
                subtitle: qsTr("From opensubtitles.com, and loaded straight away")
                onClicked: {
                    _subtitleSheet.close()
                    root.findSubtitlesOnline()
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                visible: !System.isTelevision && !Streaming.connected
                showRadio: false
                iconSource: S.Icons.plus
                title: qsTr("Add a subtitle file")
                subtitle: qsTr("Loads it now and remembers it for this video")
                onClicked: {
                    _subtitleSheet.close()
                    root.chooseSubtitleFile()
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                visible: {
                    void root.folderAccessRevision
                    return !System.isTelevision && !Streaming.connected
                        && Library.canRequestSubtitleFolder(root.fileHandle)
                }
                showRadio: false
                iconSource: S.Icons.folder
                title: qsTr("Find subtitles in this folder")
                subtitle: qsTr("Allow the folder once and matching subtitles load for every video in it")
                onClicked: {
                    _subtitleSheet.close()
                    root.awaitingFolderAccess = true
                    Library.requestSubtitleFolder(root.fileHandle)
                }
            }
        }
    }

    FileDialog {
        id: _subtitleDialog

        title: qsTr("Choose a subtitle file")
        nameFilters: [qsTr("Subtitle files (*.srt *.ass *.ssa *.sub *.vtt *.idx)"),
                      qsTr("All files (*)")]

        onAccepted: root.attachSubtitleFile(selectedFile)
    }

    Ctrl.BottomSheet {
        id: _subtitleStyleSheet

        restoresFocus: false

        z: 50
        title: qsTr("Subtitle style")

        Column {
            width: parent.width
            spacing: S.AppTheme.spacing10

            Ctrl.SectionLabel {
                text: qsTr("Size")
                topPadding: 0
            }

            Ctrl.ChipGroup {
                width: parent.width
                options: [80, 100, 120, 150]
                textFor: (percent) => percent + "%"
                value: AppSettings.subtitleScalePercent
                onChosen: (value) => AppSettings.subtitleScalePercent = value
            }

            Ctrl.SectionLabel { text: qsTr("Outline") }

            Ctrl.ChipGroup {
                width: parent.width
                options: [
                    { value: "none", label: qsTr("None") },
                    { value: "outline", label: qsTr("Outline") },
                    { value: "shadow", label: qsTr("Shadow") },
                    { value: "box", label: qsTr("Box") }
                ]
                valueRole: "value"
                textRole: "label"
                value: AppSettings.subtitleEdgeStyle
                onChosen: (value) => AppSettings.subtitleEdgeStyle = value
            }

            Ctrl.SectionLabel { text: qsTr("Position") }

            Ctrl.ChipGroup {
                width: parent.width
                options: [
                    { value: "bottom", label: qsTr("Bottom") },
                    { value: "raised", label: qsTr("Raised") },
                    { value: "top", label: qsTr("Top") }
                ]
                valueRole: "value"
                textRole: "label"
                value: AppSettings.subtitlePosition
                onChosen: (value) => AppSettings.subtitlePosition = value
            }

            Ctrl.SectionLabel { text: qsTr("Colour") }

            Row {
                spacing: S.AppTheme.spacing8

                Repeater {
                    model: S.SubtitleColours.quick

                    delegate: Ctrl.ColourSwatch {
                        required property var modelData

                        swatchColor: modelData.value
                        label: modelData.label
                        selected: AppSettings.subtitleColor === modelData.value
                        onClicked: AppSettings.subtitleColor = modelData.value
                    }
                }
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.SettingsRow {
                width: parent.width
                trailing: Ctrl.SettingsRow.Switch
                title: qsTr("Bold")
                subtitle: qsTr("Heavier text, easier over a busy picture")
                switchChecked: AppSettings.subtitleBold
                onSwitchToggled: (checked) => AppSettings.subtitleBold = checked
            }

            Text {
                width: parent.width
                topPadding: S.AppTheme.spacing6
                text: qsTr("The same settings as in Settings · Subtitles, kept for next time. More colours and the language preferences are there.")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs11
                wrapMode: Text.Wrap
            }
        }
    }

    Ctrl.BottomSheet {
        id: _delaySheet

        restoresFocus: false

        z: 50
        title: qsTr("Delay")

        Column {
            width: parent.width
            spacing: S.AppTheme.spacing10

            Ctrl.SectionLabel {
                text: qsTr("Subtitle delay")
                topPadding: 0
            }

            Item {
                width: parent.width
                height: _subDelayChips.implicitHeight

                Text {
                    anchors.left: parent.left
                    anchors.right: _subDelayChips.left
                    anchors.rightMargin: S.AppTheme.spacing12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Shift subtitles against the audio")
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs12
                    elide: Text.ElideRight
                }

                Row {
                    id: _subDelayChips

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: S.AppTheme.spacing6

                    Ctrl.Chip {
                        compact: true
                        text: qsTr("−0.1s")
                        onClicked: MpvPlayer.subDelay = MpvPlayer.subDelay - 0.1
                    }

                    Ctrl.Chip {
                        compact: true
                        selected: true
                        text: MpvPlayer.subDelay.toFixed(2) + "s"
                        onClicked: MpvPlayer.subDelay = 0
                    }

                    Ctrl.Chip {
                        compact: true
                        text: qsTr("+0.1s")
                        onClicked: MpvPlayer.subDelay = MpvPlayer.subDelay + 0.1
                    }
                }
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.SectionLabel { text: qsTr("Audio delay") }

            Item {
                width: parent.width
                height: _audioDelayChips.implicitHeight

                Text {
                    anchors.left: parent.left
                    anchors.right: _audioDelayChips.left
                    anchors.rightMargin: S.AppTheme.spacing12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Shift the audio against the picture")
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs12
                    elide: Text.ElideRight
                }

                Row {
                    id: _audioDelayChips

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: S.AppTheme.spacing6

                    Ctrl.Chip {
                        compact: true
                        text: qsTr("−0.1s")
                        onClicked: MpvPlayer.audioDelay = MpvPlayer.audioDelay - 0.1
                    }

                    Ctrl.Chip {
                        compact: true
                        selected: true
                        text: MpvPlayer.audioDelay.toFixed(2) + "s"
                        onClicked: MpvPlayer.audioDelay = 0
                    }

                    Ctrl.Chip {
                        compact: true
                        text: qsTr("+0.1s")
                        onClicked: MpvPlayer.audioDelay = MpvPlayer.audioDelay + 0.1
                    }
                }
            }

            Text {
                width: parent.width
                text: qsTr("Tap the middle chip to reset that delay to zero.")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs11
                wrapMode: Text.Wrap
            }
        }
    }

    Ctrl.BottomSheet {
        id: _focusSheet

        property string focusState: ""

        z: 50
        title: qsTr("Audio focus tests")

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                title: qsTr("System audio focus: %1").arg(_focusSheet.focusState)
                subtitle: qsTr("Tap to read it again. NOT held means the system never gave us focus, so it will never tell us we lost it")
                onClicked: _focusSheet.focusState = Platform.audioFocusState()
            }

            Ctrl.Divider { width: parent.width }

            Repeater {
                model: [
                    { event: "transient",
                      label: qsTr("A call comes in"),
                      hint: qsTr("Pauses and remembers, so Call ends resumes it") },
                    { event: "regain",
                      label: qsTr("The call ends"),
                      hint: qsTr("Resumes only if an interruption paused it") },
                    { event: "loss",
                      label: qsTr("Another app takes the audio for good"),
                      hint: qsTr("Pauses and does not come back on its own") },
                    { event: "duck",
                      label: qsTr("A notification arrives"),
                      hint: qsTr("Drops to 30% of the current volume, keeps playing") },
                    { event: "unduck",
                      label: qsTr("The notification finishes"),
                      hint: qsTr("Returns to the volume from before the duck") },
                    { event: "noisy",
                      label: qsTr("Headphones are pulled out"),
                      hint: qsTr("Pauses and stays paused when they go back in") }
                ]

                delegate: Ctrl.SheetRow {
                    required property var modelData

                    width: parent.width
                    showRadio: false
                    title: modelData.label
                    subtitle: modelData.hint
                    onClicked: Platform.simulateAudioFocusEvent(modelData.event)
                }
            }

            Text {
                width: parent.width
                topPadding: S.AppTheme.spacing12
                text: qsTr("The sheet stays open so a pause and its matching resume can be triggered one after the other.")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs11
                wrapMode: Text.Wrap
            }
        }
    }

    Connections {
        target: MpvPlayer

        function onPlaybackFailed(reason) {
            root.failure = reason
        }

        function onFileLoadedChanged() {
            if (MpvPlayer.fileLoaded) {
                root.failure = ""
                root.aspectIndex = 0
                root.syncSpeedIndex()
            }
        }

        function onSubtitleTrackIdChanged() {
            if (root.showsPictureSubtitles || MpvPlayer.subtitleTrackId <= 0)
                return
            void MpvPlayer.subtitleTracks.revision
            const track = MpvPlayer.subtitleTracks.byId(MpvPlayer.subtitleTrackId)
            if (track.id === undefined || !root.pictureSubtitle(track))
                return

            const instead = root.firstShowableSubtitle()
            if (instead > 0) {
                MpvPlayer.subtitleTrackId = instead
                return
            }

            MpvPlayer.subtitleTrackId = 0
            _toast.show(qsTr("This television cannot show picture subtitles"))
        }
    }

    Connections {
        target: S.DevTools

        function onPlaybackFailureRequested(reason) {
            root.failure = reason
        }
    }
}
