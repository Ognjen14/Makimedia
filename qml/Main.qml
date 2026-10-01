import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "./Singletons" as S
import "./Pages" as P
import "./Controls" as C
import com.topicdev.makimedia 1.0

ApplicationWindow {
    id: window

    width: 1920
    height: 1080
    minimumWidth: 360
    minimumHeight: 480

    readonly property int _normalMinimumWidth: 360
    readonly property int _normalMinimumHeight: 480
    visible: true
    title: qsTr("Makimedia")

    color: (System.isTelevision && _stack.depth > 1)
        ? "transparent"
        : S.AppTheme.background

    function _applySystemBars() {
        System.setLightStatusBar(!S.AppTheme.darkMode)
    }

    function _applyFontScale() {
        S.AppTheme.fontScale = System.isTelevision ? 1.0 : AppSettings.fontSizeScale
    }

    function _applySubtitleStyle() {
        MpvPlayer.applySubtitleStyle({
            scalePercent: AppSettings.subtitleScalePercent,
            edgeStyle: AppSettings.subtitleEdgeStyle,
            position: AppSettings.subtitlePosition,
            color: AppSettings.subtitleColor,
            bold: AppSettings.subtitleBold,
            subtitleLanguage: AppSettings.subtitleLanguage,
            audioLanguage: AppSettings.audioLanguage,
            subtitlesOnByDefault: AppSettings.subtitlesOnByDefault
        })
    }

    Connections {
        target: AppSettings

        function onSubtitleScalePercentChanged() { window._applySubtitleStyle() }
        function onSubtitleEdgeStyleChanged() { window._applySubtitleStyle() }
        function onSubtitlePositionChanged() { window._applySubtitleStyle() }
        function onSubtitleColorChanged() { window._applySubtitleStyle() }
        function onSubtitleBoldChanged() { window._applySubtitleStyle() }
        function onSubtitleLanguageChanged() { window._applySubtitleStyle() }
        function onAudioLanguageChanged() { window._applySubtitleStyle() }
        function onSubtitlesOnByDefaultChanged() { window._applySubtitleStyle() }

        function onForceHardwareDecodingChanged() {
            MpvPlayer.setForceHardwareDecoding(AppSettings.forceHardwareDecoding)
        }
    }

    function _applyScreenAwake() {
        const keep = AppSettings.keepScreenOn
                     && MpvPlayer.fileLoaded
                     && !MpvPlayer.paused
        System.setKeepScreenOn(keep)
    }

    property bool _probeStored: false

    function _tryStoreProbe() {
        if (_probeStored || _openHandle.length === 0)
            return
        if (!MpvPlayer.fileLoaded || MpvPlayer.duration <= 0)
            return
        _probeTimer.restart()
    }

    function _storeProbe() {
        if (_probeStored || _openHandle.length === 0)
            return
        if (!MpvPlayer.fileLoaded || MpvPlayer.duration <= 0)
            return
        _probeStored = true
        Library.recordProbe(_openHandle,
                            MpvPlayer.duration,
                            MpvPlayer.fileFormat,
                            MpvPlayer.videoCodec,
                            MpvPlayer.audioCodec,
                            MpvPlayer.videoWidth,
                            MpvPlayer.videoHeight,
                            MpvPlayer.hdr,
                            MpvPlayer.audioTracks.embeddedCount(),
                            MpvPlayer.subtitleTracks.embeddedCount())
    }

    Timer {
        id: _probeTimer

        interval: 1200
        onTriggered: window._storeProbe()
    }

    function _savePlayback(refreshViews) {
        if (_openHandle.length > 0 && MpvPlayer.duration > 0) {
            Library.recordPlayback(_openHandle, MpvPlayer.position,
                                   MpvPlayer.duration,
                                   refreshViews !== false)
        }
    }

    Connections {
        target: System
        function onVideoPermissionChanged() {
            if (!System.videoPermissionGranted && !System.videoPartialAccessAccepted)
                return
            Library.ensureManagedRoots()
            Library.rescanAllQuiet()
        }

        function onStorageIndexChanged(volumeName) {
            if (!System.videoPermissionGranted && !System.videoPartialAccessAccepted)
                return
            Library.ensureManagedRoots()
            Library.rescanStorageQuiet(volumeName)
        }
    }

    Connections {
        target: AppSettings
        function onFontSizeScaleChanged() { window._applyFontScale() }
        function onKeepScreenOnChanged() { window._applyScreenAwake() }
    }

    Connections {
        target: AppInstance

        function onOpenRequested(path) {
            window._comeBack()
            if (path.length > 0)
                window.openPlayer(path, false)
        }
    }

    readonly property bool _closesToTray:
        System.supportsTrayIcon && AppSettings.closeToTray

    property bool _quitting: false

    function _quitForGood() {
        if (window._quitting)
            return
        window._quitting = true
        window._savePlayback(false)
        Library.noteUi("quitting for good, not to the notification area")
        Qt.quit()
    }

    function _comeBack() {
        if (System.supportsTrayIcon) {
            System.restoreFromTray(window)
            return
        }
        window.show()
        window.raise()
        window.requestActivate()
    }

    function _goToTray() {
        if (_stack.depth > 1)
            closePlayer()

        _saveGeometry()
        System.hideToTray(window)

        if (!System.inTray)
            return

        if (!AppSettings.trayNoticeSeen) {
            AppSettings.trayNoticeSeen = true
            System.showTrayNotice(
                qsTr("Makimedia is still running"),
                qsTr("Your devices can keep streaming from here. Right-click the icon to quit."))
        }
    }

    function _applyTrayStatus() {
        if (!System.supportsTrayIcon)
            return

        let tooltip = qsTr("Makimedia")
        if (Streaming.serving) {
            tooltip = Streaming.watchingCount > 0
                      ? qsTr("Makimedia · %n device(s) watching", "", Streaming.watchingCount)
                      : qsTr("Makimedia · ready to stream")
        }
        System.setTrayStatus(tooltip, Streaming.serving)
    }

    Connections {
        target: Streaming

        function onServingChanged() { window._applyTrayStatus() }
        function onDevicesChanged() { window._applyTrayStatus() }
    }

    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive) {
                window._savePlayback(false)
                if (System.pausesPlaybackInBackground)
                    MpvPlayer.pauseForBackground()
                return
            }

            if (System.usesManagedStorageRoots
                    && (System.videoPermissionGranted || System.videoPartialAccessAccepted)) {
                Library.ensureManagedRoots()
                Library.rescanStorageQuiet("")
            }

            if (Metadata.available)
                Metadata.matchUnmatched()

            const shell = _stack.get(0)
            if (shell && shell.ensureFocus)
                shell.ensureFocus()
        }
    }

    property double _lastBackMs: 0

    function _handleBack() {
        const now = Date.now()
        if (now - _lastBackMs < 250)
            return true
        _lastBackMs = now

        if (_setupWindow.shown) {
            if (Setup.finished)
                Setup.acknowledge()
            else
                System.minimizeApp()
            return true
        }

        if (now - S.PopupRegistry.closedAtMs < 250)
            return true

        if (S.PopupRegistry.closeTop())
            return true

        if (System.inMiniPlayer) {
            System.leaveMiniPlayer(window)
            window.minimumWidth = window._normalMinimumWidth
            window.minimumHeight = window._normalMinimumHeight
            return true
        }

        const current = _stack.currentItem
        if (current && current.handleBack && current.handleBack())
            return true

        if (_stack.depth > 1) {
            closePlayer()
            return true
        }
        System.minimizeApp()
        return true
    }

    Connections {
        target: System

        function onBackPressed() { window._handleBack() }

        function onTrayOpenRequested() { window._comeBack() }

        function onTrayStopStreamingRequested() { Streaming.stopServing() }

        function onTrayQuitRequested() { window._quitForGood() }
    }

    onClosing: (close) => {
        _saveGeometry()
        _savePlayback(false)

        if (System.usesSystemBackNavigation) {
            close.accepted = !_handleBack()
            return
        }

        if (window._quitting)
            return

        if (window._closesToTray) {
            close.accepted = false
            _goToTray()
            return
        }

        if (System.supportsTrayIcon)
            _quitForGood()
    }

    function _toggleMiniPlayer() {
        if (!System.supportsMiniPlayer)
            return

        if (System.inMiniPlayer) {
            System.leaveMiniPlayer(window)
            window.minimumWidth = window._normalMinimumWidth
            window.minimumHeight = window._normalMinimumHeight
            return
        }

        _saveGeometry()
        exitFullScreen()

        window.minimumWidth = 240
        window.minimumHeight = 135

        System.enterMiniPlayer(window,
                               MpvPlayer.videoWidth,
                               MpvPlayer.videoHeight)
    }

    function _saveGeometry() {
        if (!System.usesWindowGeometry)
            return
        if (System.inMiniPlayer)
            return
        if (window.visibility === Window.Hidden)
            return
        if (window.visibility === Window.FullScreen)
            return
        if (window.visibility === Window.Maximized) {
            AppSettings.windowMaximized = true
            return
        }
        AppSettings.windowMaximized = false
        AppSettings.windowX = window.x
        AppSettings.windowY = window.y
        AppSettings.windowWidth = window.width
        AppSettings.windowHeight = window.height
    }

    function _restoreGeometry() {
        if (!System.usesWindowGeometry)
            return

        window.width = Math.max(minimumWidth, AppSettings.windowWidth)
        window.height = Math.max(minimumHeight, AppSettings.windowHeight)

        const savedX = AppSettings.windowX
        const savedY = AppSettings.windowY
        if (savedX >= 0 && savedY >= 0) {
            const screenRight = Screen.desktopAvailableWidth
            const screenBottom = Screen.desktopAvailableHeight
            if (savedX < screenRight - 100 && savedY < screenBottom - 100) {
                window.x = savedX
                window.y = savedY
            }
        }
        if (AppSettings.windowMaximized)
            window.visibility = Window.Maximized
    }

    Timer {
        id: _geometryTimer

        interval: 400
        onTriggered: window._saveGeometry()
    }

    onXChanged: _geometryTimer.restart()
    onYChanged: _geometryTimer.restart()
    onWidthChanged: _geometryTimer.restart()
    onHeightChanged: _geometryTimer.restart()
    onVisibilityChanged: _geometryTimer.restart()

    Component.onCompleted: {
        _restoreGeometry()
        _applySystemBars()
        _applyFontScale()
        _applyTrayStatus()
        S.DevTools.touchCapable = System.hasTouchInput

        if (MpvPlayer.ready) {
            MpvPlayer.volume = AppSettings.playerVolume
            MpvPlayer.setForceHardwareDecoding(AppSettings.forceHardwareDecoding)
            _applySubtitleStyle()
        }

        if (System.usesManagedStorageRoots) {
            if (System.hasVideoPermission() || System.videoPartialAccessAccepted) {
                Library.ensureManagedRoots()
                Library.rescanAllQuiet()
            } else if (!System.videoPermissionBlocked
                       && !System.videoAccessSkipped) {
                System.requestVideoPermission()
            }
        } else {
            Library.rescanAllQuiet()
        }

        const launch = Library.takeLaunchPath()
        if (launch.length > 0)
            openPlayer(launch, false)
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        context: Qt.WindowShortcut
        onActivated: window._handleBack()
    }

    DropArea {
        id: _dropArea

        anchors.fill: parent
        z: 100
        keys: ["text/uri-list"]

        onDropped: (drop) => {
            if (!drop.hasUrls) {
                drop.accepted = false
                return
            }
            drop.accepted = window.openDropped(drop.urls)
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 99
        visible: _dropArea.containsDrag
        color: Qt.rgba(0, 0, 0, 0.55)

        Text {
            anchors.centerIn: parent
            text: qsTr("Drop a video file to play it")
            color: "#FFFFFF"
            font.pixelSize: S.AppTheme.fs18
            font.weight: Font.Medium
        }
    }

    StackView {
        id: _stack

        anchors.fill: parent
        initialItem: _shellPage

        pushEnter: null
        pushExit: null
        replaceEnter: null
        replaceExit: null
        popEnter: null
        popExit: null
    }

    C.SetupWindow {
        id: _setupWindow

        anchors.fill: parent
        z: 100
        shown: Setup.shown && _stack.depth <= 1
    }

    C.AppDialog {
        id: _lostDialog

        z: 110
        title: qsTr("Lost %1").arg(Streaming.lostServerName)
        message: qsTr("The film stopped where it was. It picks up from here the next time you connect.")
        acceptText: qsTr("OK")
        closeOnScrim: false
        onClosed: window.closePlayer()
    }

    Connections {
        target: Streaming

        function onLostChanged() {
            if (Streaming.lostServerName.length === 0)
                return
            if (_stack.depth > 1)
                _lostDialog.open()
            else
                Streaming.acknowledgeLost()
        }
    }

    property string _openHandle: ""
    property bool _skipResume: false

    function openDropped(urls) {
        for (let i = 0; i < urls.length; i++) {
            const candidate = String(urls[i])
            if (!Library.isPlayableFile(candidate))
                continue
            openPlayer(Library.handleFromUrl(candidate), false)
            return true
        }
        return false
    }

    function _applyAttachedSubtitle() {
        const attached = Library.attachedSubtitles(_openHandle)
        for (let i = 0; i < attached.length; i++) {
            const url = Library.subtitleMpvUrl(attached[i].handle)
            if (url.length === 0) {
                console.warn("attached subtitle could not be opened:",
                             attached[i].handle)
                continue
            }
            MpvPlayer.addSubtitleFile(url, attached[i].displayName)
        }
    }

    function refuseWhileStreaming() {
        Library.noteUi("asked to play while streaming - refused")
        const shell = _stack.get(0)
        if (shell && shell.showMessage)
            shell.showMessage(qsTr("Stop streaming to play files here"))
    }

    function openRemote(url) {
        if (url.length === 0)
            return

        _savePlayback()

        _openHandle = ""
        _skipResume = true
        _probeStored = true
        _probeTimer.stop()

        Library.noteUi("playing a file from the PC by its number")
        MpvPlayer.setSubtitlesForNextOpen(url, [])
        MpvPlayer.openPath(url)

        if (_stack.depth <= 1) {
            System.setImmersiveMode(true)
            _stack.push(_playerPage)
        }
    }

    function openPlayer(handle, fromStart) {
        if (Streaming.serving) {
            refuseWhileStreaming()
            return
        }

        const started = Date.now()
        Library.noteUi("play pressed")

        const url = Library.mpvUrlFor(handle)
        if (url.length === 0)
            return
        Library.noteUi("play: handle resolved after " + (Date.now() - started) + " ms")

        _savePlayback()

        _openHandle = handle
        _skipResume = fromStart === true
        _probeStored = false
        _probeTimer.stop()

        if (url.startsWith("http://") || url.startsWith("https://"))
            SeekPreview.close()
        else
            SeekPreview.openFile(url)
        MpvPlayer.setSubtitlesForNextOpen(url, Library.siblingSubtitleTracks(handle))
        Library.noteUi("play: subtitles listed after " + (Date.now() - started) + " ms")

        MpvPlayer.openPath(url)
        Library.noteUi("play: open posted after " + (Date.now() - started) + " ms")

        if (_stack.depth <= 1) {
            System.setImmersiveMode(true)
            _stack.push(_playerPage)
            Library.noteUi("play: player page pushed after "
                           + (Date.now() - started) + " ms")
        }
    }

    function toggleFullScreen() {
        window.visibility = (window.visibility === Window.FullScreen)
                            ? Window.Windowed
                            : Window.FullScreen
    }

    function exitFullScreen() {
        if (window.visibility === Window.FullScreen)
            window.visibility = Window.Windowed
    }

    function closePlayer() {
        if (System.inMiniPlayer) {
            System.leaveMiniPlayer(window)
            window.minimumWidth = window._normalMinimumWidth
            window.minimumHeight = window._normalMinimumHeight
        }

        _savePlayback()
        _openHandle = ""
        System.setImmersiveMode(false)
        System.releaseScreenBrightness()
        exitFullScreen()
        MpvPlayer.stop()
        SeekPreview.close()
        if (_stack.depth > 1)
            _stack.pop()

        const shell = _stack.get(0)
        if (shell && shell.claimFocus)
            Qt.callLater(shell.claimFocus)

        if (Streaming.lostServerName.length > 0)
            Streaming.acknowledgeLost()
    }

    Connections {
        target: MpvPlayer
        function onPausedChanged() { window._applyScreenAwake() }

        function onDurationChanged() { window._tryStoreProbe() }

        function onVideoResolutionChanged() { window._tryStoreProbe() }

        function onReadyChanged() {
            if (!MpvPlayer.ready)
                return
            MpvPlayer.volume = AppSettings.playerVolume
            MpvPlayer.setForceHardwareDecoding(AppSettings.forceHardwareDecoding)
            window._applySubtitleStyle()
        }

        function onFileFormatChanged() { window._tryStoreProbe() }

        function onFileLoadedChanged() {
            window._applyScreenAwake()
            window._tryStoreProbe()
            if (!MpvPlayer.fileLoaded || window._openHandle.length === 0)
                return

            window._applyAttachedSubtitle()

            if (window._skipResume) {
                window._skipResume = false
                return
            }
            const resume = Library.resumePositionFor(window._openHandle)
            if (resume > 1)
                MpvPlayer.seekAbsolute(resume)
        }
    }

    Timer {
        interval: 10000
        running: MpvPlayer.fileLoaded && !MpvPlayer.paused
        repeat: true
        onTriggered: window._savePlayback(false)
    }

    Component {
        id: _shellPage

        P.AppShell {
            id: _shell

            onPlayRequested: (handle) => window.openPlayer(handle, false)
            onPlayFromStartRequested: (handle) => window.openPlayer(handle, true)
            onRemotePlayRequested: (url) => window.openRemote(url)
        }
    }

    Component {
        id: _playerPage

        P.PlayerScreen {
            fullScreen: window.visibility === Window.FullScreen
            touchCapable: System.hasTouchInput
            fileHandle: window._openHandle

            onBackRequested: window.closePlayer()
            onFullScreenToggleRequested: window.toggleFullScreen()
            onPlayRequested: (handle) => window.openPlayer(handle, false)
            onMiniPlayerToggleRequested: window._toggleMiniPlayer()
        }
    }
}
