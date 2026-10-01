pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import "../Singletons" as S
import "../Controls" as Ctrl
import "SettingsScreens" as SP
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal scanFoldersRequested()
    signal drawerRequested()
    signal remotePlayRequested(string url)

    readonly property bool acceptsFocus: true
    readonly property string sourceCodeUrl: "https://github.com/Ognjen14/Makimedia"
    readonly property string websiteUrl: "https://makimedia.org/"
    readonly property string openSubtitlesUrl: "https://www.opensubtitles.com"

    readonly property string mpvBuilds:
        "Windows: mpv 0.41.0-923-g7b8915bc1 with FFmpeg 1d7b14f61. "
        + "Android: mpv 0.40.0-292-g9f153e2a2 with FFmpeg n8.0."

    function showLicence(fileName, heading) {
        const text = System.readBundledText(":/licenses/" + fileName)

        _licenceSheet.title = heading
        _licenceSheet.body = text.length > 0
            ? text
            : qsTr("This build does not carry %1. Ask through %2 and we will "
                   + "send it.").arg(fileName).arg(root.websiteUrl)
        _licenceSheet.open()
    }

    function centreY(item) {
        return item.mapToItem(root, 0, item.height / 2).y
    }

    function sideBySide(a, b) {
        return Math.abs(centreY(a) - centreY(b))
               < Math.max(a.height, b.height) * 0.5
    }

    function moveTo(item) {
        if (!item)
            return
        item.forceActiveFocus(Qt.TabFocusReason)
        showFocused(item)
    }

    function stepFocus(forward) {
        const current = Window.activeFocusItem
        if (!current)
            return

        let next = current.nextItemInFocusChain(forward)
        let guard = 0
        while (next && next !== current && sideBySide(current, next)
               && guard++ < 64) {
            next = next.nextItemInFocusChain(forward)
        }
        moveTo(next)
    }

    function stepAcross(forward) {
        const current = Window.activeFocusItem
        if (!current) {
            root.drawerRequested()
            return
        }

        const next = current.nextItemInFocusChain(forward)
        if (next && next !== current && sideBySide(current, next)) {
            moveTo(next)
            return
        }
        if (!forward)
            root.leaveLeft()
    }

    function showFocused(item) {
        _focusScroll.reveal(item)
    }

    Ctrl.FocusScroller {
        id: _focusScroll

        owner: root
        flickable: _scroll
        content: _loader.item
    }

    function leaveLeft() {
        if (root.page === "root")
            root.drawerRequested()
        else
            root.page = "root"
    }

    function firstFocusable(item) {
        if (!item)
            return null

        const kids = item.children
        for (let i = 0; i < kids.length; ++i) {
            const child = kids[i]
            if (!child.visible)
                continue
            if (child.activeFocusOnTab === true && child.enabled)
                return child
            const found = firstFocusable(child)
            if (found)
                return found
        }
        return null
    }

    function takeFocus() {
        const first = firstFocusable(_loader.item)
        if (first) {
            first.forceActiveFocus(Qt.TabFocusReason)
            _scroll.contentY = 0
            return
        }
        root.forceActiveFocus(Qt.TabFocusReason)
    }

    Keys.onUpPressed: root.stepFocus(false)
    Keys.onDownPressed: root.stepFocus(true)
    Keys.onLeftPressed: root.stepAcross(false)
    Keys.onRightPressed: root.stepAcross(true)

    Keys.onPressed: (event) => {
        if (event.key !== Qt.Key_Select
            && event.key !== Qt.Key_Return
            && event.key !== Qt.Key_Enter) {
            return
        }

        const current = Window.activeFocusItem
        if (!current || typeof current.clicked !== "function")
            return

        if (current.checkable === true && typeof current.toggle === "function")
            current.toggle()
        current.clicked()
        event.accepted = true
    }

    property string page: "root"

    property var declinedVolumes: []

    readonly property bool touchScheme: {
        const mode = AppSettings.playerControlScheme
        if (mode === "touch")
            return true
        if (mode === "desktop")
            return false
        return System.hasTouchInput
    }

    readonly property var languageOptions: [
        { code: "", label: qsTr("Any") },
        { code: "en", label: qsTr("English") },
        { code: "hr", label: qsTr("Croatian") },
        { code: "sr", label: qsTr("Serbian") },
        { code: "de", label: qsTr("German") },
        { code: "fr", label: qsTr("French") },
        { code: "es", label: qsTr("Spanish") },
        { code: "it", label: qsTr("Italian") },
        { code: "ja", label: qsTr("Japanese") }
    ]

    ListModel {
        id: _matchResults
    }

    ListModel {
        id: _parsedNames
    }

    property var pendingParsedNames: []
    property int parsedTotal: 0

    function startMatchPreview() {
        _matchResults.clear()
        const files = Library.previewFilenameParsing()
        for (let i = 0; i < files.length; i++)
            Metadata.previewMatch(files[i].handle, files[i].fileName)
    }

    function previewRow(result) {
        return {
            "fileName": String(result.fileName || ""),
            "error": String(result.error || ""),
            "confident": result.confident === true,
            "suggested": result.suggested === true,
            "score": Number(result.score || 0),
            "matchedTitle": String(result.matchedTitle || ""),
            "matchedYear": Number(result.matchedYear || 0),
            "reason": String(result.reason || "")
        }
    }

    Connections {
        target: Metadata

        function onPreviewReady(result) {
            const row = root.previewRow(result)
            let low = 0
            let high = _matchResults.count
            while (low < high) {
                const middle = (low + high) >> 1
                if (_matchResults.get(middle).score >= row.score)
                    low = middle + 1
                else
                    high = middle
            }
            _matchResults.insert(low, row)
        }
    }

    function refreshDeclinedVolumes() {
        declinedVolumes = Library.declinedStorageVolumes()
    }

    function parsedRow(entry) {
        return {
            "fileName": String(entry.fileName || ""),
            "title": String(entry.title || ""),
            "year": Number(entry.year || 0),
            "isEpisode": entry.isEpisode === true,
            "season": Number(entry.season || 0),
            "episode": Number(entry.episode || 0),
            "episodeTitle": String(entry.episodeTitle || ""),
            "releaseGroup": String(entry.releaseGroup || "")
        }
    }

    function refreshParsedNames() {
        _parseFill.stop()
        _parsedNames.clear()
        pendingParsedNames = Library.previewFilenameParsing()
        parsedTotal = pendingParsedNames.length
        if (parsedTotal > 0)
            _parseFill.start()
    }

    Timer {
        id: _parseFill

        interval: 16
        repeat: true
        onTriggered: {
            const end = Math.min(root.pendingParsedNames.length, _parsedNames.count + 100)
            for (let i = _parsedNames.count; i < end; ++i)
                _parsedNames.append(root.parsedRow(root.pendingParsedNames[i]))
            if (_parsedNames.count >= root.pendingParsedNames.length) {
                stop()
                root.pendingParsedNames = []
            }
        }
    }

    Component.onCompleted: refreshDeclinedVolumes()

    readonly property string pageTitle: {
        switch (page) {
        case "appearance":
            return qsTr("Appearance")
        case "player":
            return qsTr("Player")
        case "subtitles":
            return qsTr("Subtitles")
        case "about":
            return qsTr("About")
        case "shortcuts":
            if (System.isTelevision)
                return qsTr("The remote")
            return root.touchScheme ? qsTr("Gestures") : qsTr("Keyboard shortcuts")
        case "rules":
            return qsTr("How your library is read")
        case "parser":
            return qsTr("Filename parsing")
        case "matching":
            return qsTr("TMDB matching")
        case "developer":
            return qsTr("Developer")
        case "streaming":
            return qsTr("Streaming")
        default:
            return qsTr("Settings")
        }
    }

    function goBack() {
        if (page === "root")
            return false
        page = "root"
        return true
    }

    Ctrl.AppBar {
        id: _bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: root.page !== "root"
        height: visible ? implicitHeight : 0
        leading: Ctrl.AppBar.Back
        title: root.pageTitle

        onLeadingTriggered: root.page = "root"
    }

    Flickable {
        id: _scroll

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _bar.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        contentHeight: _loader.item ? _loader.item.implicitHeight + S.AppTheme.spacing32 : 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Loader {
            id: _loader

            width: parent.width - S.AppTheme.scrollBarWidth
            sourceComponent: {
                switch (root.page) {
                case "appearance":
                    return _appearance
                case "player":
                    return _player
                case "subtitles":
                    return _subtitles
                case "about":
                    return _about
                case "shortcuts":
                    if (System.isTelevision)
                        return _remote
                    return root.touchScheme ? _gestures : _shortcuts
                case "rules":
                    return _libraryRules
                case "parser":
                    return _parserPreview
                case "matching":
                    return _matchPreview
                case "developer":
                    return _developer
                case "streaming":
                    return _streamingPage
                default:
                    return _rootPage
                }
            }
        }
    }

    Ctrl.BottomSheet {
        id: _licenceSheet

        property string body: ""

        Text {
            width: parent.width
            text: _licenceSheet.body
            color: S.AppTheme.textSecondary
            font.family: S.AppTheme.monoFontFamily
            font.pixelSize: S.AppTheme.fs12
            lineHeight: 1.25
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }

    Ctrl.BottomSheet {
        id: _hiddenCollections

        title: qsTr("Collections you removed")

        Column {
            width: parent.width
            spacing: 0

            Text {
                width: parent.width
                bottomPadding: S.AppTheme.spacing8
                text: qsTr("Put one back on the Collections page.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Repeater {
                model: Library.hiddenCollections

                delegate: Ctrl.ListRow {
                    required property var modelData

                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.collections
                    title: modelData.name.length > 0 ? modelData.name : qsTr("Collection")
                    subtitle: modelData.custom ? qsTr("One you made") : qsTr("From TMDB")
                    trailingText: qsTr("Put back")

                    onClicked: Library.restoreCollection(modelData.collectionId)
                }
            }
        }
    }

    Ctrl.BottomSheet {
        id: _hiddenCollectionFilms

        title: qsTr("Films you took out of a collection")

        Column {
            width: parent.width
            spacing: 0

            Text {
                width: parent.width
                bottomPadding: S.AppTheme.spacing8
                text: qsTr("Put one back into the collection that lists it.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Repeater {
                model: Library.hiddenCollectionFilms

                delegate: Ctrl.ListRow {
                    required property var modelData

                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.collections
                    title: modelData.title.length > 0 ? modelData.title : qsTr("Film")
                    subtitle: modelData.collectionName.length > 0
                              ? modelData.collectionName
                              : qsTr("A collection")
                    trailingText: qsTr("Put back")

                    onClicked: Library.restoreCollectionFilm(modelData.scope, modelData.tmdbId)
                }
            }
        }
    }

    Ctrl.BottomSheet {
        id: _discardedShows

        title: qsTr("Folders you said are not shows")

        Column {
            width: parent.width
            spacing: 0

            Text {
                width: parent.width
                bottomPadding: S.AppTheme.spacing8
                text: qsTr("Put one back and Identify shows will ask about it again. "
                           + "The files never left your library.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Repeater {
                model: Library.discardedShows

                delegate: Ctrl.ListRow {
                    required property var modelData

                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.tvShows
                    title: modelData.title.length > 0
                           ? modelData.title
                           : qsTr("Unnamed folder")
                    subtitle: modelData.folder.length > 0
                              ? modelData.folder
                              : (modelData.fileCount === 1
                                 ? qsTr("1 file")
                                 : qsTr("%1 files").arg(modelData.fileCount))
                    trailingText: qsTr("Put back")

                    onClicked: Library.restoreShow(modelData.title)
                }
            }
        }
    }

    Ctrl.BottomSheet {
        id: _removedFiles

        title: qsTr("Removed from library")

        Column {
            width: parent.width
            spacing: 0

            Text {
                width: parent.width
                bottomPadding: S.AppTheme.spacing8
                text: qsTr("These files are still on disk. Put one back and the next scan "
                           + "adds it to your library again.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Ctrl.ListRow {
                width: parent.width
                visible: Library.removedFiles.count > 1
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.plus
                title: qsTr("Put all back")
                subtitle: qsTr("%n file(s)", "", Library.removedFiles.count)
                onClicked: {
                    Library.restoreAllRemoved()
                    _removedFiles.close()
                }
            }

            Repeater {
                model: Library.removedFiles

                delegate: Ctrl.ListRow {
                    required property string handle
                    required property string displayName
                    required property string folder

                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.noVideo
                    title: displayName
                    subtitle: folder
                    trailingText: qsTr("Put back")

                    onClicked: {
                        Library.restoreRemoved(handle)
                        if (Library.removedFiles.count === 0)
                            _removedFiles.close()
                    }
                }
            }
        }
    }

    Component {
        id: _rootPage

        Column {
            spacing: 0

            Ctrl.SectionLabel { text: qsTr("Library") }

            Ctrl.ListRow {
                width: parent.width
                visible: System.usesManagedStorageRoots
                         && !System.videoPermissionGranted
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.lock
                title: qsTr("Videos on this device")
                subtitle: System.videoAccessPartial
                          ? qsTr("Only the ones you picked are in the library")
                          : qsTr("None of them are in the library")
                onClicked: System.openAppSettings()
            }

            Ctrl.Divider {
                width: parent.width
                visible: System.usesManagedStorageRoots
                         && !System.videoPermissionGranted
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected && !System.isTelevision
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.folder
                title: qsTr("Scan folders")
                subtitle: qsTr("%n folder(s) indexed", "", Library.folders.count)
                onClicked: root.scanFoldersRequested()
            }

            Repeater {
                model: Streaming.connected ? [] : root.declinedVolumes

                delegate: Column {
                    required property var modelData

                    width: parent.width

                    Ctrl.Divider { width: parent.width }

                    Ctrl.ListRow {
                        width: parent.width
                        leading: Ctrl.ListRow.Icon
                        iconSource: S.Icons.plus
                        title: qsTr("Scan %1").arg(modelData.displayName)
                        subtitle: qsTr("You skipped this one. Tap to add it to the library.")
                        onClicked: {
                            Library.adoptStorageVolume(modelData.handle)
                            root.refreshDeclinedVolumes()
                        }
                    }
                }
            }

            Ctrl.Divider {
                width: parent.width
                visible: !Streaming.connected && !System.isTelevision
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.search
                title: Metadata.pending > 0
                       ? qsTr("Looking up %n file(s)", "", Metadata.pending)
                       : qsTr("Look up anything still unmatched")
                subtitle: Metadata.available
                          ? qsTr("%1 matched · %2 suggested · asks again about files TMDB had nothing for")
                            .arg(Metadata.matchedCount)
                            .arg(Metadata.suggestedCount)
                          : qsTr("Needs a TMDB key")
                enabled: Metadata.available && Metadata.pending === 0
                onClicked: Metadata.matchUnmatchedAgain()
            }

            Ctrl.Divider {
                width: parent.width
                visible: !Streaming.connected && Metadata.creditsSweeping
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected && Metadata.creditsSweeping
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.spinner
                iconSpinning: visible
                hoverEnabled: false
                focusPolicy: Qt.NoFocus
                title: qsTr("Filling in cast and crew")
                subtitle: qsTr("%n title(s) still to ask about", "",
                               Metadata.creditsRemaining)
            }

            Ctrl.Divider {
                width: parent.width
                visible: !Streaming.connected && Metadata.artworkWarming
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected && Metadata.artworkWarming
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.spinner
                iconSpinning: visible
                hoverEnabled: false
                focusPolicy: Qt.NoFocus
                title: qsTr("Downloading artwork")
                subtitle: qsTr("%n picture(s) to go", "",
                               Metadata.artworkRemaining)
            }

            Ctrl.Divider {
                width: parent.width
                visible: !System.isTelevision
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !System.isTelevision
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.alertCircle
                title: qsTr("How your library is read")
                subtitle: qsTr("Folder layouts, names, subtitles and the formats that are indexed")
                onClicked: root.page = "rules"
            }

            Ctrl.Divider {
                width: parent.width
                visible: !Streaming.connected && Library.hiddenCollections.length > 0
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected && Library.hiddenCollections.length > 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.collections
                title: qsTr("Collections you removed")
                subtitle: Library.hiddenCollections.length === 1
                          ? qsTr("1 is off the Collections page")
                          : qsTr("%1 are off the Collections page")
                            .arg(Library.hiddenCollections.length)
                onClicked: _hiddenCollections.open()
            }

            Ctrl.Divider {
                width: parent.width
                visible: !Streaming.connected && Library.hiddenCollectionFilms.length > 0
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected && Library.hiddenCollectionFilms.length > 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.collections
                title: qsTr("Films you took out of a collection")
                subtitle: Library.hiddenCollectionFilms.length === 1
                          ? qsTr("1 is off its collection")
                          : qsTr("%1 are off their collections")
                            .arg(Library.hiddenCollectionFilms.length)
                onClicked: _hiddenCollectionFilms.open()
            }

            Ctrl.Divider {
                width: parent.width
                visible: !Streaming.connected && Library.discardedShows.length > 0
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !Streaming.connected && Library.discardedShows.length > 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.tvShows
                title: qsTr("Folders you said are not shows")
                subtitle: Library.discardedShows.length === 1
                          ? qsTr("1 is off Identify shows")
                          : qsTr("%1 are off Identify shows")
                            .arg(Library.discardedShows.length)
                onClicked: _discardedShows.open()
            }

            Ctrl.Divider {
                width: parent.width
                visible: !System.isTelevision && !Streaming.connected
                         && Library.removedFiles.count > 0
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !System.isTelevision && !Streaming.connected
                         && Library.removedFiles.count > 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.noVideo
                title: qsTr("Removed from library")
                subtitle: qsTr("%n file(s) taken out, still on disk", "",
                               Library.removedFiles.count)
                onClicked: _removedFiles.open()
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.Divider {
                width: parent.width
                visible: Streaming.canConnect
            }

            Ctrl.ListRow {
                width: parent.width
                visible: Streaming.canConnect
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.play
                title: qsTr("Streaming")
                subtitle: Streaming.connected
                          ? qsTr("Watching the library of %1").arg(Streaming.connectedServerName)
                          : qsTr("Watch a PC's library on this device")
                onClicked: root.page = "streaming"
            }

            Ctrl.SectionLabel { text: qsTr("Playback") }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.play
                title: qsTr("Player")
                subtitle: qsTr("Skip %1s · hold speed %2×")
                          .arg(AppSettings.skipIntervalSeconds)
                          .arg(S.Format.speed(AppSettings.holdToSpeedMultiplier))
                onClicked: root.page = "player"
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.subtitles
                title: qsTr("Subtitles")
                subtitle: qsTr("Size %1% · %2 · %3")
                          .arg(AppSettings.subtitleScalePercent)
                          .arg(AppSettings.subtitleEdgeStyle)
                          .arg(AppSettings.subtitlePosition)
                onClicked: root.page = "subtitles"
            }

            Ctrl.SectionLabel { text: qsTr("App") }

            Ctrl.SettingsRow {
                width: parent.width
                visible: System.supportsTrayIcon
                height: visible ? implicitHeight : 0
                iconSource: S.Icons.tray
                trailing: Ctrl.SettingsRow.Switch
                title: qsTr("Keep running when the window is closed")
                subtitle: qsTr("Waits in the notification area, so your devices keep streaming. Right-click the icon to quit")
                switchChecked: AppSettings.closeToTray
                onSwitchToggled: (checked) => AppSettings.closeToTray = checked
            }

            Ctrl.Divider {
                width: parent.width
                visible: System.supportsTrayIcon
            }

            Ctrl.ListRow {
                width: parent.width
                visible: System.usesSystemFilePicker
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.folder
                title: qsTr("Save log file")
                subtitle: System.isTelevision
                          ? qsTr("Copies makimedia.log to the Download folder")
                          : qsTr("Copies makimedia.log to a place you choose, such as a USB drive")
                onClicked: System.saveLogFile()
            }

            Ctrl.Divider {
                width: parent.width
                visible: System.usesSystemFilePicker
            }

            Ctrl.ListRow {
                width: parent.width
                visible: !System.isTelevision
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.appearance
                title: qsTr("Appearance")
                subtitle: S.AppTheme.accentNames[S.AppTheme.safeAccentIndex]
                onClicked: root.page = "appearance"
            }

            Ctrl.Divider {
                width: parent.width
                visible: !System.isTelevision
            }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.alertCircle
                title: qsTr("About")
                subtitle: qsTr("Version %1 · open source").arg(Qt.application.version)
                onClicked: root.page = "about"
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.play
                title: System.isTelevision
                       ? qsTr("The remote")
                       : (root.touchScheme ? qsTr("Gestures") : qsTr("Keyboard shortcuts"))
                subtitle: System.isTelevision
                          ? qsTr("What each key does while a film plays")
                          : (root.touchScheme
                             ? qsTr("How the player responds to touch")
                             : qsTr("What each key does while a film plays"))
                onClicked: root.page = "shortcuts"
            }

            Ctrl.Divider {
                width: parent.width
                visible: S.DevTools.showDeveloperSurfaces
            }

            Ctrl.ListRow {
                width: parent.width
                visible: S.DevTools.showDeveloperSurfaces
                height: visible ? implicitHeight : 0
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.alertCircle
                title: qsTr("Developer")
                subtitle: qsTr("Test hooks, remove before release")
                onClicked: root.page = "developer"
            }
        }
    }

    Component {
        id: _libraryRules

        LibraryRulesScreen {
            width: _loader.width
        }
    }

    Component {
        id: _shortcuts

        SP.ShortcutsPage {}
    }

    Component {
        id: _matchPreview

        Column {
            spacing: 0

            Text {
                width: parent.width
                bottomPadding: S.AppTheme.spacing12
                text: Metadata.available
                      ? qsTr("Each file is searched on TMDB and the best result scored. Nothing is written to the library yet. 80 or above would be applied, 45 to 79 offered as a suggestion, below 45 rejected.")
                      : qsTr("No TMDB key, so nothing can be searched. Add one in Settings or build with one.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs12
                wrapMode: Text.Wrap
            }

            Ctrl.ListRow {
                width: parent.width
                title: Metadata.pending > 0
                       ? qsTr("Working, %n left", "", Metadata.pending)
                       : qsTr("Preview only, writes nothing")
                subtitle: qsTr("%n result(s) so far", "", _matchResults.count)
                enabled: Metadata.available && Metadata.pending === 0
                onClicked: root.startMatchPreview()
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Match and save to the library")
                subtitle: qsTr("%1 matched · %2 suggested · pinned files are left alone")
                          .arg(Metadata.matchedCount)
                          .arg(Metadata.suggestedCount)
                enabled: Metadata.available && Metadata.pending === 0
                onClicked: {
                    _matchResults.clear()
                    Metadata.matchLibrary()
                }
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Clear saved matches")
                subtitle: qsTr("Keeps anything you pinned by hand")
                enabled: Metadata.pending === 0
                onClicked: Metadata.clearAllMatches()
            }

            Repeater {
                model: _matchResults

                delegate: Column {
                    id: _result

                    required property var model

                    width: parent.width

                    Ctrl.Divider { width: parent.width }

                    Ctrl.ListRow {
                        width: parent.width
                        monoTitle: true
                        title: _result.model.fileName
                        subtitle: {
                            if (_result.model.error.length > 0)
                                return qsTr("FAILED: %1").arg(_result.model.error)

                            const verdict = _result.model.confident
                                ? qsTr("APPLY")
                                : (_result.model.suggested ? qsTr("suggest") : qsTr("reject"))

                            return qsTr("%1 %2  ·  %3 (%4)  ·  %5")
                                   .arg(verdict)
                                   .arg(_result.model.score)
                                   .arg(_result.model.matchedTitle)
                                   .arg(_result.model.matchedYear > 0
                                        ? _result.model.matchedYear : qsTr("no year"))
                                   .arg(_result.model.reason)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: _parserPreview

        Column {
            spacing: 0

            Text {
                width: parent.width
                bottomPadding: S.AppTheme.spacing12
                text: qsTr("What each indexed filename becomes before it is sent to TMDB. Anything wrong here will match the wrong title, so this is the thing to read carefully.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs12
                wrapMode: Text.Wrap
            }

            Repeater {
                model: _parsedNames

                delegate: Column {
                    id: _parsed

                    required property var model

                    width: parent.width

                    Ctrl.Divider { width: parent.width }

                    Ctrl.ListRow {
                        width: parent.width
                        monoTitle: true
                        title: _parsed.model.fileName
                        subtitle: {
                            const entry = _parsed.model
                            const bits = []
                            bits.push(entry.title.length > 0
                                      ? qsTr("title: %1").arg(entry.title)
                                      : qsTr("NO TITLE"))
                            if (entry.year > 0)
                                bits.push(qsTr("year: %1").arg(entry.year))
                            if (entry.isEpisode)
                                bits.push(S.Format.episodeCode(entry.season, entry.episode))
                            if (entry.episodeTitle.length > 0)
                                bits.push(qsTr("episode: %1").arg(entry.episodeTitle))
                            if (entry.releaseGroup.length > 0)
                                bits.push(qsTr("group: %1").arg(entry.releaseGroup))
                            return bits.join("  ·  ")
                        }
                    }
                }
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Parse again")
                subtitle: qsTr("%n file(s) in the library", "", root.parsedTotal)
                onClicked: root.refreshParsedNames()
            }
        }
    }

    Component {
        id: _remote

        Column {
            spacing: 0

            Ctrl.SectionLabel {
                text: qsTr("While a film plays")
                topPadding: 0
            }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Select"),
                      description: qsTr("Play or pause, and show the controls") },
                    { keys: qsTr("Left"),
                      description: qsTr("Back %1 seconds")
                                   .arg(AppSettings.skipIntervalSeconds) },
                    { keys: qsTr("Right"),
                      description: qsTr("Forward %1 seconds")
                                   .arg(AppSettings.skipIntervalSeconds) },
                    { keys: qsTr("Down"),
                      description: qsTr("The row of chips: audio, subtitles, their style and delay, aspect, zoom, speed") },
                    { keys: qsTr("Up"),
                      description: qsTr("The next episode when it is offered, otherwise the controls") },
                    { keys: qsTr("Back"),
                      description: qsTr("Leave the film. It keeps your place") }
                ]
            }

            Ctrl.SectionLabel { text: qsTr("Around the app") }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Left"),
                      description: qsTr("From the first thing on a page, open the menu on the side") },
                    { keys: qsTr("Up"),
                      description: qsTr("From the top row of Home, the offer to connect to a PC") },
                    { keys: qsTr("Select"),
                      description: qsTr("Open what is highlighted") },
                    { keys: qsTr("Back"),
                      description: qsTr("Up one level, and out of the app from Home") }
                ]
            }

            Ctrl.SectionLabel { text: qsTr("Keys some remotes have") }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Play, Pause"),
                      description: qsTr("As Select does, without showing the controls") },
                    { keys: qsTr("Stop"),
                      description: qsTr("Pause") }
                ]
            }
        }
    }

    Component {
        id: _gestures

        Column {
            spacing: 0

            Ctrl.SectionLabel {
                text: qsTr("Playback")
                topPadding: 0
            }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Tap"),
                      description: qsTr("Show or hide the controls") },
                    { keys: qsTr("Double-tap left"),
                      description: qsTr("Back %1 seconds")
                                   .arg(AppSettings.skipIntervalSeconds) },
                    { keys: qsTr("Double-tap right"),
                      description: qsTr("Forward %1 seconds")
                                   .arg(AppSettings.skipIntervalSeconds) },
                    { keys: qsTr("Press and hold"),
                      description: qsTr("Play at %1x until you let go")
                                   .arg(S.Format.speed(AppSettings.holdToSpeedMultiplier)) }
                ]
            }

            Ctrl.SectionLabel { text: qsTr("Drag") }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Sideways"),
                      description: qsTr("Scrub, about two minutes across the screen") },
                    { keys: qsTr("Up or down, right"),
                      description: qsTr("Volume") },
                    { keys: qsTr("Up or down, left"),
                      description: qsTr("Brightness") }
                ]
            }

            Ctrl.SectionLabel { text: qsTr("Two fingers") }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Pinch"),
                      description: qsTr("Zoom in and out") },
                    { keys: qsTr("Drag"),
                      description: qsTr("Move the picture while zoomed in") }
                ]
            }

            Ctrl.SectionLabel { text: qsTr("When the controls are locked") }

            Ctrl.KeyHints {
                width: parent.width
                keyColumnWidth: 148
                monoKeys: false
                hints: [
                    { keys: qsTr("Double-tap"),
                      description: qsTr("Unlock. Every other gesture is ignored") }
                ]
            }

            Text {
                width: parent.width
                topPadding: S.AppTheme.spacing12
                text: qsTr("Double-tapping the middle of the screen does nothing on purpose, so a mistimed pause never skips the video.")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs11
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: _streamingPage

        Column {
            spacing: 0

            Component.onCompleted: Streaming.startLooking()

            Column {
                width: parent.width
                visible: Streaming.connected
                spacing: 0

                Ctrl.SectionLabel {
                    text: qsTr("Connected")
                    topPadding: 0
                }

                Ctrl.ListRow {
                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.close
                    title: qsTr("Disconnect from %1").arg(Streaming.connectedServerName)
                    subtitle: qsTr("This device's own films come back")
                    onClicked: Streaming.disconnectFromServer()
                }
            }

            Ctrl.SectionLabel {
                text: qsTr("PCs streaming on this network")
                topPadding: Streaming.connected ? S.AppTheme.spacing16 : 0
            }

            Text {
                width: parent.width
                visible: Streaming.servers.count === 0
                bottomPadding: S.AppTheme.spacing8
                text: qsTr("Looking for a PC that is streaming. On the PC, open Stream to devices and start streaming.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Repeater {
                model: Streaming.servers

                delegate: Ctrl.ListRow {
                    required property string serverId
                    required property string name
                    required property string host
                    required property int port
                    required property bool selected

                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: selected ? S.Icons.check : S.Icons.play
                    title: name
                    subtitle: qsTr("On this network")
                    trailingText: Streaming.connected ? "" : qsTr("Connect")
                    onClicked: {
                        Streaming.selectServer(serverId)
                        Streaming.connectToServer(serverId)
                    }
                }
            }

            Text {
                width: parent.width
                topPadding: S.AppTheme.spacing8
                visible: Streaming.lastError.length > 0
                text: Streaming.lastError
                color: S.AppTheme.error
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Ctrl.SectionLabel {
                visible: Streaming.storedLibraries.count > 0
                text: qsTr("Libraries kept on this device")
            }

            Text {
                width: parent.width
                visible: Streaming.storedLibraries.count > 0
                bottomPadding: S.AppTheme.spacing8
                text: qsTr("A copy of each PC's list of films, so connecting again is quick. Forgetting one only removes the copy.")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Repeater {
                model: Streaming.storedLibraries

                delegate: Ctrl.ListRow {
                    required property string serverId
                    required property string name
                    required property double bytes

                    width: parent.width
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.all
                    title: name
                    subtitle: bytes >= 1048576
                              ? qsTr("%1 MB").arg((bytes / 1048576).toFixed(1))
                              : qsTr("%1 KB").arg(Math.max(1, Math.round(bytes / 1024)))
                    trailingText: qsTr("Forget")
                    onClicked: Streaming.forgetStoredLibrary(serverId)
                }
            }
        }
    }

    Component {
        id: _developer

        Column {
            spacing: 0

            Ctrl.SectionLabel {
                text: qsTr("Artwork")
                topPadding: 0
            }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.close
                title: qsTr("Clear image cache")
                subtitle: qsTr("Posters and thumbnails are fetched again as you browse")
                onClicked: {
                    Library.clearThumbnailCache()
                    Metadata.clearPosterCache()
                }
            }

            Column {
                width: parent.width
                visible: Streaming.canConnect
                spacing: 0

                Ctrl.SectionLabel { text: qsTr("Streaming") }


                Text {
                    width: parent.width
                    bottomPadding: S.AppTheme.spacing8
                    text: Streaming.connected
                          ? qsTr("Plays one file from %1 by its number, the file id the PC's log shows.")
                            .arg(Streaming.connectedServerName)
                          : qsTr("Connect to a PC in Settings, Streaming first.")
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs13
                    wrapMode: Text.Wrap
                }

                Row {
                    width: parent.width
                    spacing: S.AppTheme.spacing8

                    Rectangle {
                        width: parent.width - _playRemoteButton.width - parent.spacing
                        height: S.AppTheme.controlHeightLarge - 8
                        radius: S.AppTheme.radiusSmall
                        color: S.AppTheme.surfaceVariant
                        border.width: _fileIdField.activeFocus ? 2 : 1
                        border.color: _fileIdField.activeFocus ? S.AppTheme.focus : S.AppTheme.outline

                        TextField {
                            id: _fileIdField

                            anchors.fill: parent
                            leftPadding: S.AppTheme.spacing12
                            rightPadding: S.AppTheme.spacing12
                            topPadding: 0
                            bottomPadding: 0
                            verticalAlignment: TextInput.AlignVCenter
                            placeholderText: qsTr("File number")
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1 }
                            color: S.AppTheme.textPrimary
                            font.pixelSize: S.AppTheme.fs14
                            selectByMouse: true
                            background: Item {}
                        }
                    }

                    Ctrl.AppButton {
                        id: _playRemoteButton

                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Play")
                        variant: Ctrl.AppButton.Filled
                        enabled: Streaming.connected && _fileIdField.text.length > 0
                        onClicked: {
                            const url = Streaming.remoteFileUrl(parseInt(_fileIdField.text))
                            if (url.length > 0)
                                root.remotePlayRequested(url)
                        }
                    }
                }

                Ctrl.ListRow {
                    width: parent.width
                    title: qsTr("Make the next connect download the library again")
                    subtitle: qsTr("Forgets which version of each PC's library is kept here, so the next connect says Updating and carries progress over")
                    onClicked: Streaming.forgetStoredRevisions()
                }

                Ctrl.SettingsRow {
                    width: parent.width
                    title: qsTr("Pretend the PC's library is newer than this app")
                    subtitle: qsTr("The next connect is refused with the message that asks to update Makimedia")
                    trailing: Ctrl.SettingsRow.Switch
                    switchChecked: Streaming.pretendNewerLibrary
                    onSwitchToggled: (checked) => Streaming.pretendNewerLibrary = checked
                }
            }

            Ctrl.SectionLabel { text: qsTr("Library setup") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Play a normal setup")
                subtitle: qsTr("Fake counts through all five steps in about 15 s, then OK. Nothing is scanned, matched or downloaded")
                enabled: !Setup.active
                onClicked: Setup.simulate("normal")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Play a setup where TMDB cannot be reached")
                subtitle: qsTr("Matching ends Failed, artwork is skipped, the message sits above OK")
                enabled: !Setup.active
                onClicked: Setup.simulate("unreachable")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Play a setup where some artwork fails")
                subtitle: qsTr("Artwork finishes with 37 images that could not be downloaded")
                enabled: !Setup.active
                onClicked: Setup.simulate("artwork")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Play a setup where more files arrive")
                subtitle: qsTr("After artwork it goes back to scanning for 120 more in the same window")
                enabled: !Setup.active
                onClicked: Setup.simulate("more")
            }

            Ctrl.SectionLabel { text: qsTr("Scanning") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Filename parsing")
                subtitle: qsTr("What every indexed file becomes before it reaches TMDB")
                onClicked: {
                    root.refreshParsedNames()
                    root.page = "parser"
                }
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("TMDB matching")
                subtitle: qsTr("What each file would match to, and how confident. Writes nothing")
                onClicked: root.page = "matching"
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Run the silent rescan now")
                subtitle: qsTr("The real startup pass, without restarting. No banner, and a message only if it finds something")
                enabled: Library.ready && !Library.scanning
                onClicked: Library.rescanAllQuiet()
            }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Probe files that have never been played")
                subtitle: qsTr("Fills duration, resolution, codecs and track counts in the background. Watch the log; the details rows fill as it goes")
                enabled: Library.ready && !Library.scanning
                onClicked: Library.probeUnprobed(true)
            }

            Ctrl.SettingsRow {
                width: parent.width
                title: qsTr("Audio focus tests in the player")
                subtitle: qsTr("Adds an Interrupt chip to the player controls, the only place these can be triggered while something is playing")
                trailing: Ctrl.SettingsRow.Switch
                switchChecked: S.DevTools.audioFocusTools
                onSwitchToggled: (checked) => S.DevTools.audioFocusTools = checked
            }

            Ctrl.SectionLabel { text: qsTr("Player controls") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Detected input")
                subtitle: S.DevTools.touchCapable
                          ? qsTr("Touchscreen present, touch controls by default")
                          : qsTr("No touchscreen, mouse and keyboard by default")
                enabled: false
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ChipGroup {
                width: parent.width
                options: [
                    { key: "auto", label: qsTr("Automatic") },
                    { key: "touch", label: qsTr("Force touch") },
                    { key: "desktop", label: qsTr("Force mouse") }
                ]
                valueRole: "key"
                textRole: "label"
                value: AppSettings.playerControlScheme
                onChosen: (value) => AppSettings.playerControlScheme = value
            }

            Ctrl.SectionLabel { text: qsTr("First run") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Show the welcome screen")
                subtitle: qsTr("Desktop only. Android shows the permission screen instead")
                enabled: !System.usesManagedStorageRoots
                onClicked: S.DevTools.forceWelcome = true
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Show the permission screen")
                subtitle: qsTr("Without actually revoking the permission. Tapping Allow on it clears this")
                enabled: System.usesManagedStorageRoots
                onClicked: S.DevTools.forcePermissionGate = true
            }

            Ctrl.SectionLabel { text: qsTr("Player") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Playback failed message")
                subtitle: qsTr("Shows over the player, needs a file open")
                onClicked: S.DevTools.playbackFailureRequested(
                               qsTr("the audio codec is not supported by this device"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.SettingsRow {
                width: parent.width
                title: qsTr("Force software decoding readout")
                subtitle: qsTr("Makes the player report a decoder fallback")
                trailing: Ctrl.SettingsRow.Switch
                switchChecked: S.DevTools.forceSoftwareDecode
                onSwitchToggled: (checked) => S.DevTools.forceSoftwareDecode = checked
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.SettingsRow {
                width: parent.width
                title: qsTr("Let a picture too large to play through")
                subtitle: qsTr("Plays an 8K file instead of refusing it, so the 20-second give-up is what ends it. The only way left to reach the watchdog")
                trailing: Ctrl.SettingsRow.Switch
                switchChecked: MpvPlayer.allowUnsupportedPicture
                onSwitchToggled: (checked) => MpvPlayer.allowUnsupportedPicture = checked
            }

            Ctrl.Divider {
                width: parent.width
                visible: System.canSimulateTelevision
            }

            Ctrl.SettingsRow {
                width: parent.width
                visible: System.canSimulateTelevision
                height: visible ? implicitHeight : 0
                title: qsTr("Treat this device as a television")
                subtitle: qsTr("Television video surface and layout, from the next launch. Close the app and open it again to apply")
                trailing: Ctrl.SettingsRow.Switch
                switchChecked: AppSettings.treatAsTelevision
                onSwitchToggled: (checked) => AppSettings.treatAsTelevision = checked
            }

            Ctrl.SectionLabel { text: qsTr("Failure states") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("File not found dialog")
                subtitle: qsTr("The dialog raised when a played file has gone")
                onClicked: S.DevTools.fileMissingRequested(
                               "Dune.Part.Two.2024.2160p.HDR.mkv")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Folder offline message")
                subtitle: qsTr("What a disconnected drive or NAS reports")
                onClicked: S.DevTools.folderUnavailableRequested("\\\\nas\\media")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Generic error message")
                subtitle: qsTr("The snackbar used for every library error")
                onClicked: S.DevTools.errorRequested(
                               qsTr("That folder could not be saved."))
            }
        }
    }

    Component {
        id: _appearance

        SP.AppearancePage {}
    }

    Component {
        id: _player

        SP.PlayerPage {}
    }

    Component {
        id: _subtitles

        Column {
            spacing: 0

            Rectangle {
                width: parent.width
                height: 112
                radius: S.AppTheme.radiusLarge

                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: S.AppTheme.surfaceRaised }
                    GradientStop { position: 1.0; color: S.AppTheme.surfaceVariant }
                }

                Rectangle {
                    id: _subBox

                    readonly property string edge: AppSettings.subtitleEdgeStyle
                    readonly property string position: AppSettings.subtitlePosition

                    anchors.horizontalCenter: parent.horizontalCenter
                    y: position === "top"
                       ? S.AppTheme.spacing10
                       : (position === "raised"
                          ? parent.height * 0.55
                          : parent.height - height - S.AppTheme.spacing14)

                    width: _subText.implicitWidth + S.AppTheme.spacing8
                    height: _subText.implicitHeight + S.AppTheme.spacing4
                    radius: 2
                    color: edge === "box" ? "#A0000000" : "transparent"

                    Text {
                        id: _subText

                        anchors.centerIn: parent
                        text: qsTr("The quick brown fox")
                        color: AppSettings.subtitleColor
                        font.bold: AppSettings.subtitleBold
                        font.pixelSize: Math.round(S.AppTheme.fs15
                                                   * AppSettings.subtitleScalePercent / 100)
                        style: _subBox.edge === "outline"
                               ? Text.Outline
                               : (_subBox.edge === "shadow"
                                  ? Text.Raised : Text.Normal)
                        styleColor: "#000000"
                    }
                }
            }

            Ctrl.SectionLabel { text: qsTr("Size") }

            Ctrl.ChipGroup {
                width: parent.width
                options: [80, 100, 120, 150]
                textFor: (percent) => percent + "%"
                value: AppSettings.subtitleScalePercent
                onChosen: (value) => AppSettings.subtitleScalePercent = value
            }

            Ctrl.SectionLabel { text: qsTr("Edge style") }

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

            Ctrl.SectionLabel { text: qsTr("Colour") }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing8

                Repeater {
                    model: S.SubtitleColours.all

                    delegate: Ctrl.ColourSwatch {
                        required property var modelData

                        swatchColor: modelData.value
                        label: modelData.label
                        selected: AppSettings.subtitleColor === modelData.value
                        onClicked: AppSettings.subtitleColor = modelData.value
                    }
                }
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

            Ctrl.SectionLabel { text: qsTr("Preferred subtitle language") }

            Ctrl.ChipGroup {
                width: parent.width
                options: root.languageOptions
                valueRole: "code"
                textRole: "label"
                value: AppSettings.subtitleLanguage
                onChosen: (value) => AppSettings.subtitleLanguage = value
            }

            Ctrl.SectionLabel { text: qsTr("Preferred audio language") }

            Ctrl.ChipGroup {
                width: parent.width
                options: root.languageOptions
                valueRole: "code"
                textRole: "label"
                value: AppSettings.audioLanguage
                onChosen: (value) => AppSettings.audioLanguage = value
            }

            Text {
                width: parent.width
                topPadding: S.AppTheme.spacing6
                text: qsTr("A language preference picks the matching track when a file opens. It does not change the file you are watching now.")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs11
                wrapMode: Text.Wrap
            }

            Ctrl.SectionLabel { text: qsTr("Behaviour") }

            Ctrl.SettingsRow {
                width: parent.width
                trailing: Ctrl.SettingsRow.Switch
                title: qsTr("Bold text")
                subtitle: qsTr("Easier to read over bright scenes")
                switchChecked: AppSettings.subtitleBold
                onSwitchToggled: (checked) => AppSettings.subtitleBold = checked
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.SettingsRow {
                width: parent.width
                trailing: Ctrl.SettingsRow.Switch
                title: qsTr("Turn subtitles on automatically")
                subtitle: qsTr("Off means every video starts with no subtitles until you pick one")
                switchChecked: AppSettings.subtitlesOnByDefault
                onSwitchToggled: (checked) => AppSettings.subtitlesOnByDefault = checked
            }


            Ctrl.Divider { width: parent.width }

            Ctrl.SettingsRow {
                width: parent.width
                trailing: Ctrl.SettingsRow.Switch
                title: qsTr("Remember track per show")
                subtitle: qsTr("Reuse language choice on next episode")
                switchChecked: AppSettings.rememberTrackPerShow
                onSwitchToggled: (checked) => AppSettings.rememberTrackPerShow = checked
            }

            Ctrl.SectionLabel {
                visible: SubtitleSearch.available
                text: qsTr("Searching opensubtitles.com")
            }

            Column {
                width: parent.width
                visible: SubtitleSearch.available
                spacing: S.AppTheme.spacing10

                Text {
                    width: parent.width
                    rightPadding: S.AppTheme.spacing16
                    text: qsTr("Your own account, so the downloads come out of your daily allowance rather than a shared one. Searching happens on this PC; a device watching its library sees whatever lands beside the film.")
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs13
                    lineHeight: 1.25
                    wrapMode: Text.Wrap
                }

                component Field: Column {
                    property alias field: _input
                    property alias text: _input.text
                    property alias hidden: _input.echoMode
                    property string label
                    property string hint

                    width: parent ? parent.width : 0
                    spacing: S.AppTheme.spacing4

                    Text {
                        text: parent.label
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs12
                        font.weight: Font.Medium
                    }

                    Rectangle {
                        width: parent.width
                        height: S.AppTheme.controlHeightLarge - 8
                        radius: S.AppTheme.radiusSmall
                        color: S.AppTheme.surfaceVariant
                        border.width: _input.activeFocus ? 2 : 1
                        border.color: _input.activeFocus ? S.AppTheme.focus
                                                         : S.AppTheme.outline

                        TextField {
                            id: _input

                            anchors.fill: parent
                            leftPadding: S.AppTheme.spacing12
                            rightPadding: S.AppTheme.spacing12
                            topPadding: 0
                            bottomPadding: 0
                            verticalAlignment: TextInput.AlignVCenter
                            color: S.AppTheme.textPrimary
                            font.pixelSize: S.AppTheme.fs14
                            selectByMouse: true
                            background: Item {}
                        }
                    }

                    Text {
                        width: parent.width
                        rightPadding: S.AppTheme.spacing16
                        visible: parent.hint.length > 0
                        text: parent.hint
                        color: S.AppTheme.error
                        font.pixelSize: S.AppTheme.fs12
                        wrapMode: Text.Wrap
                    }
                }

                Field {
                    id: _account

                    label: qsTr("Username")
                    hint: text.indexOf("@") >= 0
                          ? qsTr("That looks like an email address. The website accepts one, the API does not - use the username from your opensubtitles profile.")
                          : ""
                    text: AppSettings.subtitleAccount
                    field.onEditingFinished: AppSettings.subtitleAccount = text
                }

                Field {
                    id: _secret

                    label: qsTr("Password")
                    hidden: TextInput.Password
                    text: AppSettings.subtitlePassword
                    field.onEditingFinished: AppSettings.subtitlePassword = text
                }

                Row {
                    spacing: S.AppTheme.spacing12

                    Ctrl.AppButton {
                        text: SubtitleSearch.signedIn ? qsTr("Sign in again")
                                                      : qsTr("Sign in")
                        variant: Ctrl.AppButton.Filled
                        enabled: !SubtitleSearch.busy
                                 && AppSettings.subtitleAccount.length > 0
                                 && AppSettings.subtitlePassword.length > 0
                        onClicked: {
                            _account.field.editingFinished()
                            _secret.field.editingFinished()
                            _signInProblem.text = ""
                            SubtitleSearch.signIn()
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            if (SubtitleSearch.busy)
                                return qsTr("Signing in…")
                            if (SubtitleSearch.signedIn)
                                return qsTr("Signed in · %n download(s) left today",
                                            "", SubtitleSearch.downloadsLeft)
                            if (AppSettings.subtitleAccount.length === 0)
                                return qsTr("Not signed in")
                            return qsTr("Not signed in yet — this happens by itself a few seconds after the app starts")
                        }
                        color: SubtitleSearch.signedIn ? S.AppTheme.primary
                                                       : S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                    }
                }

                Text {
                    id: _signInProblem

                    width: parent.width
                    rightPadding: S.AppTheme.spacing16
                    visible: text.length > 0
                    color: S.AppTheme.error
                    font.pixelSize: S.AppTheme.fs13
                    wrapMode: Text.Wrap
                }

                Connections {
                    target: SubtitleSearch

                    function onFailed(reason) {
                        _signInProblem.text = reason
                    }

                    function onSignedInChanged() {
                        if (SubtitleSearch.signedIn)
                            _signInProblem.text = ""
                    }
                }
            }

            Ctrl.SectionLabel {
                visible: SubtitleSearch.available
                text: qsTr("Languages to search for")
            }

            Flow {
                width: parent.width
                visible: SubtitleSearch.available
                spacing: S.AppTheme.spacing8
                bottomPadding: S.AppTheme.spacing12

                Repeater {
                    model: [
                        { code: "en", name: qsTr("English") },
                        { code: "sr", name: qsTr("Serbian") },
                        { code: "hr", name: qsTr("Croatian") },
                        { code: "bs", name: qsTr("Bosnian") },
                        { code: "sl", name: qsTr("Slovenian") },
                        { code: "mk", name: qsTr("Macedonian") },
                        { code: "sq", name: qsTr("Albanian") },
                        { code: "de", name: qsTr("German") },
                        { code: "fr", name: qsTr("French") },
                        { code: "es", name: qsTr("Spanish") },
                        { code: "pt", name: qsTr("Portuguese") },
                        { code: "it", name: qsTr("Italian") },
                        { code: "nl", name: qsTr("Dutch") },
                        { code: "pl", name: qsTr("Polish") },
                        { code: "cs", name: qsTr("Czech") },
                        { code: "sk", name: qsTr("Slovak") },
                        { code: "hu", name: qsTr("Hungarian") },
                        { code: "ro", name: qsTr("Romanian") },
                        { code: "bg", name: qsTr("Bulgarian") },
                        { code: "el", name: qsTr("Greek") },
                        { code: "tr", name: qsTr("Turkish") },
                        { code: "ru", name: qsTr("Russian") },
                        { code: "uk", name: qsTr("Ukrainian") },
                        { code: "sv", name: qsTr("Swedish") },
                        { code: "da", name: qsTr("Danish") },
                        { code: "no", name: qsTr("Norwegian") },
                        { code: "fi", name: qsTr("Finnish") },
                        { code: "ar", name: qsTr("Arabic") },
                        { code: "he", name: qsTr("Hebrew") },
                        { code: "zh-CN", name: qsTr("Chinese") },
                        { code: "ja", name: qsTr("Japanese") },
                        { code: "ko", name: qsTr("Korean") }
                    ]

                    delegate: Ctrl.Chip {
                        required property var modelData

                        text: modelData.name
                        selected: AppSettings.subtitleSearchLanguages
                                            .indexOf(modelData.code) >= 0
                        onClicked: {
                            const picked =
                                AppSettings.subtitleSearchLanguages.slice()
                            const at = picked.indexOf(modelData.code)
                            if (at >= 0)
                                picked.splice(at, 1)
                            else
                                picked.push(modelData.code)
                            if (picked.length > 0)
                                AppSettings.subtitleSearchLanguages = picked
                        }
                    }
                }
            }
        }
    }

    Component {
        id: _about

        Column {
            spacing: 0

            Item {
                width: parent.width
                height: _aboutHeader.height

                Column {
                    id: _aboutHeader

                    width: parent.width
                    topPadding: S.AppTheme.spacing24
                    bottomPadding: S.AppTheme.spacing16
                    spacing: S.AppTheme.spacing12

                    Item {
                        id: _appIconBox

                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 88
                        height: 88

                        Image {
                            id: _appIconImage

                            anchors.fill: parent
                            source: S.Icons.appIcon
                            sourceSize.width: 176
                            sourceSize.height: 176
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            visible: false
                        }

                        Rectangle {
                            id: _appIconMask

                            anchors.fill: parent
                            radius: S.AppTheme.radiusLarge
                            color: S.AppTheme.textPrimary
                            visible: false
                        }

                        OpacityMask {
                            anchors.fill: parent
                            source: _appIconImage
                            maskSource: _appIconMask
                        }
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Makimedia %1").arg(Qt.application.version)
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs18
                        font.weight: Font.Medium
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Free and open source. No ads, no subscription, no analytics.")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs12
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Makimedia is a video player, not a streaming service. It doesn't provide any films or shows; it plays files you already have.")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs12
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        visible: !System.isTelevision
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("makimedia.org")
                        color: S.AppTheme.primary
                        font.pixelSize: S.AppTheme.fs12
                        font.weight: Font.Medium
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    visible: !System.isTelevision
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.openUrlExternally(root.websiteUrl)
                }
            }

            Ctrl.SectionLabel { text: qsTr("Credits") }

            Ctrl.Card {
                width: parent.width
                variant: true
                implicitHeight: _tmdb.implicitHeight + 2 * S.AppTheme.spacing14

                Ctrl.TmdbCredit {
                    id: _tmdb

                    anchors.left: parent.left
                    anchors.right: parent.right
                    showNotice: true
                    caption: qsTr("Tap for themoviedb.org")
                }
            }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("OpenSubtitles")
                subtitle: qsTr("Subtitle search · tap for opensubtitles.com")
                onClicked: Qt.openUrlExternally(root.openSubtitlesUrl)
            }

            Ctrl.SectionLabel { text: qsTr("Legal") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Privacy policy")
                subtitle: qsTr("What leaves your device and what never does · opens the website")
                onClicked: Qt.openUrlExternally(root.websiteUrl + "privacy.html")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Legal notice")
                subtitle: qsTr("Your responsibility, trademarks and warranty · opens the website")
                onClicked: Qt.openUrlExternally(root.websiteUrl + "legal.html")
            }

            Ctrl.SectionLabel { text: qsTr("Licences") }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Makimedia")
                subtitle: qsTr("GPLv2 or, at your option, any later version · tap for "
                               + "the licence")
                onClicked: root.showLicence("GPL-2.0.txt",
                                            qsTr("GNU General Public License v2"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Source code")
                subtitle: qsTr("Makimedia's own code, on GitHub · tap to open")
                onClicked: Qt.openUrlExternally(root.sourceCodeUrl)
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("libmpv / FFmpeg")
                subtitle: qsTr("GPLv3 · %1 · tap for the licence").arg(root.mpvBuilds)
                onClicked: root.showLicence("GPL-3.0.txt",
                                            qsTr("GNU General Public License v3"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("FFmpeg, the parts that are not GPL")
                subtitle: qsTr("LGPLv2.1 · tap for the licence")
                onClicked: root.showLicence("LGPL-2.1.txt",
                                            qsTr("GNU Lesser General Public License v2.1"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Third-party notices")
                subtitle: qsTr("Where each part comes from, and how to get the source "
                               + "for mpv and FFmpeg · tap to read")
                onClicked: root.showLicence("NOTICE.txt",
                                            qsTr("Third-party notices"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Qt")
                subtitle: qsTr("LGPLv3 · dynamically linked · tap for the licence")
                onClicked: root.showLicence("LGPL-3.0.txt",
                                            qsTr("GNU Lesser General Public License v3"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("SQLite")
                subtitle: qsTr("Public domain")
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("OpenSSL")
                subtitle: qsTr("Apache License 2.0 · inside libmpv on Windows, bundled on "
                               + "Android · tap for the licence")
                onClicked: root.showLicence("Apache-2.0.txt",
                                            qsTr("Apache License 2.0"))
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.ListRow {
                width: parent.width
                title: qsTr("Roboto")
                subtitle: qsTr("Apache License 2.0 · bundled · tap for the licence")
                onClicked: root.showLicence("Apache-2.0.txt",
                                            qsTr("Apache License 2.0"))
            }
        }
    }
}
