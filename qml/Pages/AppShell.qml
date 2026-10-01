pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../Singletons" as S
import "../Controls" as Ctrl
import "TV" as TV
import com.topicdev.makimedia 1.0

Item {
    id: root

    enum DrawerMode {
        Modal,
        Rail,
        Permanent
    }

    signal playRequested(string handle)
    signal playFromStartRequested(string handle)
    signal remotePlayRequested(string url)

    property bool _wasConnected: false

    function showMessage(text) {
        _toast.show(text)
    }

    Connections {
        target: Streaming

        function onServingChanged() {
            if (Streaming.serving)
                root.selectKey("streaming")
        }

        function onConnectionChanged() {
            if (Streaming.connecting)
                return
            if (root._wasConnected !== Streaming.connected) {
                root._wasConnected = Streaming.connected
                root.selectKey("home")
            }
            if (root.remoteNavigation)
                _focusAfterPopup.restart()
        }

        function onLastErrorChanged() {
            if (Streaming.lastError.length > 0 && Streaming.canConnect)
                _toast.show(Streaming.lastError)
        }
    }

    property string detailsHandle: ""

    readonly property bool showDetails:
        detailsHandle.length > 0 && !showGate

    property bool detailsOpensFixMatch: false

    function openDetails(handle) {
        detailsOpensFixMatch = false
        detailsHandle = handle
    }

    function openFixMatchFor(handle) {
        detailsOpensFixMatch = true
        detailsHandle = handle
    }

    function closeDetails(restore) {
        const wasOpen = detailsHandle.length > 0
        detailsHandle = ""
        detailsOpensFixMatch = false
        if (wasOpen && restore !== false)
            restoreFocus()
    }

    property string currentKey: "home"
    property var collectionId: 0
    property var showMediaId: 0

    function openCollection(id) {
        collectionId = id
    }

    function removeCollection(id, name, custom) {
        Library.hideCollection(id, name)
        _discardedShow = ""
        _removedCollection = id
        _toast.show(custom ? qsTr("%1 removed").arg(name)
                           : qsTr("%1 taken off Collections").arg(name),
                    qsTr("Undo"))
    }

    function discardShow(fileHandles, title, folder) {
        Library.discardShow(fileHandles, title, folder)
        _removedCollection = 0
        _discardedShow = title
        _toast.show(qsTr("%1 is not a show").arg(title), qsTr("Undo"))
    }

    function openCollectionMenu(id, name, custom) {
        if (Streaming.connected)
            return
        _collectionMenuId = id
        _collectionMenuName = name
        _collectionMenuCustom = custom
        _collectionMenu.open()
    }

    property var _removedCollection: 0
    property string _discardedShow: ""
    property var _collectionMenuId: 0
    property string _collectionMenuName: ""
    property bool _collectionMenuCustom: false

    function closeCollection(restore) {
        const wasOpen = collectionId !== 0
        collectionId = 0
        Library.collectionPage.collectionId = 0
        if (wasOpen && restore !== false)
            restoreFocus()
    }
    property string showTitle: ""
    property bool modalDrawerOpen: false

    function openShow(mediaId, title) {
        showMediaId = mediaId
        showTitle = title
    }

    function closeShow(restore) {
        const wasOpen = showMediaId > 0
        showMediaId = 0
        showTitle = ""
        if (wasOpen && restore !== false)
            restoreFocus()
    }

    function restoreFocus() {
        if (!focusPage())
            focusDrawer()
    }

    function claimFocus() {
        if (!remoteNavigation || showGate)
            return

        if (S.PopupRegistry.hasOpen)
            return

        if (width <= 0 || height <= 0)
            return

        restoreFocus()
    }

    function ensureFocus() {
        const item = Window.activeFocusItem
        if (item && item.visible && item !== Window.contentItem)
            return
        claimFocus()
    }

    Connections {
        target: S.PopupRegistry

        function onHasOpenChanged() {
            if (!S.PopupRegistry.hasOpen && root.remoteNavigation)
                _focusAfterPopup.restart()
        }
    }

    Timer {
        id: _focusAfterPopup

        interval: 260
        onTriggered: {
            if (root.visible)
                root.ensureFocus()
        }
    }
    property bool welcomeDismissed: false

    readonly property bool needsPermission:
        System.usesManagedStorageRoots
        && ((!System.videoPermissionGranted
             && !System.videoPartialAccessAccepted
             && !System.videoAccessSkipped)
            || S.DevTools.forcePermissionGate)

    readonly property bool showWelcome:
        !System.usesManagedStorageRoots
        && (Library.folders.count === 0 || S.DevTools.forceWelcome)
        && !welcomeDismissed

    readonly property bool showGate: showWelcome || needsPermission

    readonly property bool pageCanGoBack:
        currentPage !== null && currentPage.canGoBack === true

    function keptLoaderFor(key) {
        switch (key) {
        case "home":
            return _homeKeep
        case "all":
        case "unmatched":
            return _allKeep
        case "movies":
            return _moviesKeep
        case "shows":
            return _showsKeep
        case "collections":
            return _collectionsKeep
        default:
            return null
        }
    }

    readonly property Item currentPage: {
        if (showMediaId > 0 || collectionId !== 0)
            return _pageLoader.item
        const kept = keptLoaderFor(currentKey)
        return kept ? kept.item : _pageLoader.item
    }

    property string menuHandle: ""
    property string menuName: ""
    property var menuInfo: ({})

    property var copies: []
    property bool copiesFromStart: false

    function playTitle(handle, fromStart) {
        const found = Library.filmCopies(handle)
        if (found.length < 2) {
            if (fromStart)
                root.playFromStartRequested(handle)
            else
                root.playRequested(handle)
            return
        }
        copies = found
        copiesFromStart = fromStart
        Library.noteUi("asking which of " + found.length + " copies to play")
        _copiesSheet.open()
    }

    function playCopy(handle) {
        _copiesSheet.close()
        Library.noteUi("playing the copy " + handle)
        if (copiesFromStart)
            root.playFromStartRequested(handle)
        else
            root.playRequested(handle)
    }

    function copyQuality(copy) {
        const parts = []
        const line = S.Format.scanLine(copy.width || 0, copy.height || 0)
        if (line.length > 0)
            parts.push(line)
        if (copy.hdr === true)
            parts.push("HDR")
        if (copy.videoCodec.length > 0)
            parts.push(copy.videoCodec.split(" ")[0].toUpperCase())
        if (copy.sizeText.length > 0)
            parts.push(copy.sizeText)
        if (copy.watched === true)
            parts.push(qsTr("Seen"))
        else if (copy.positionSeconds > 0)
            parts.push(qsTr("Stopped at %1").arg(S.Format.clock(copy.positionSeconds)))
        return parts.join(" · ")
    }

    function openItemMenu(handle, displayName) {
        menuHandle = handle
        menuName = displayName
        menuInfo = Library.fileInfo(handle)
        _itemMenu.open()
    }

    readonly property int drawerMode: {
        if (System.isTelevision)
            return AppShell.Rail
        if (width < S.AppTheme.breakpointCompact)
            return AppShell.Modal
        return width < S.AppTheme.breakpointExpanded ? AppShell.Rail
                                                     : AppShell.Permanent
    }

    readonly property bool modalDrawer: drawerMode === AppShell.Modal

    readonly property var _navDefinitions: [
        {
            key: "home",
            label: qsTr("Home"),
            icon: S.Icons.home
        },
        {
            key: "movies",
            label: qsTr("Movies"),
            icon: S.Icons.movies,
            count: () => Library.movies.count
        },
        {
            key: "shows",
            label: qsTr("TV shows"),
            icon: S.Icons.tvShows,
            count: () => Library.shows.count
        },
        {
            key: "collections",
            label: qsTr("Collections"),
            icon: S.Icons.collections,
            count: () => Library.collections.count
        },
        {
            key: "all",
            label: qsTr("All"),
            icon: S.Icons.all,
            count: () => Library.fileCount
        },
        {
            key: "suggested",
            label: qsTr("Suggested"),
            icon: S.Icons.suggested,
            count: () => Library.suggestions.count,
            hidden: () => Library.suggestions.count === 0
        },
        {
            key: "shows-to-identify",
            label: qsTr("Identify shows"),
            icon: S.Icons.identifyShows,
            count: () => Library.unmatchedShows.count,
            hidden: () => Library.unmatchedShows.count === 0
        },
        {
            key: "unmatched",
            label: qsTr("Unmatched"),
            icon: S.Icons.unmatched,
            count: () => Library.unmatchedCount,
            hidden: () => Library.unmatchedCount === 0
        },
        {
            key: "folders",
            label: qsTr("Folders"),
            icon: S.Icons.folder,
            count: () => Library.folders.count,
            hidden: () => System.isTelevision
        },
        { key: "section", section: true, label: "" },
        {
            key: "search",
            label: qsTr("Search"),
            icon: S.Icons.search,
        },
        {
            key: "streaming",
            label: qsTr("Stream to devices"),
            icon: S.Icons.play,
            hidden: () => !Streaming.canServe
        },
        {
            key: "settings",
            label: qsTr("Settings"),
            icon: S.Icons.settings
        },
        {
            key: "gallery",
            label: qsTr("Gallery"),
            icon: S.Icons.appearance,
            hidden: () => !S.DevTools.showDeveloperSurfaces
        }
    ]

    readonly property var _hiddenWhileConnected: [
        "all", "suggested", "shows-to-identify", "unmatched", "folders"
    ]

    readonly property var _navState: _navDefinitions.map(function (entry) {
        const heldByStreaming = Streaming.serving && entry.key !== "streaming"
        const ownLibraryOnly = Streaming.connected
                               && root._hiddenWhileConnected.indexOf(entry.key) >= 0
        return {
            "count": entry.count ? entry.count() : -1,
            "hidden": heldByStreaming || ownLibraryOnly
                      || (entry.hidden ? entry.hidden() : false),
            "label": entry.label
        }
    })

    on_NavStateChanged: _syncNav()

    ListModel {
        id: _navModel

        Component.onCompleted: root._syncNav()
    }

    function _syncNav() {
        let row = 0
        for (let i = 0; i < _navDefinitions.length; ++i) {
            const entry = _navDefinitions[i]
            const state = _navState[i]
            const present = row < _navModel.count
                            && _navModel.get(row).key === entry.key

            if (state.hidden) {
                if (present)
                    _navModel.remove(row)
                continue
            }

            if (!present) {
                _navModel.insert(row, {
                    "key": entry.key,
                    "label": state.label,
                    "icon": entry.icon === undefined ? "" : String(entry.icon),
                    "heading": entry.section === true,
                    "muted": false,
                    "badge": state.count
                })
            } else {
                if (_navModel.get(row).badge !== state.count)
                    _navModel.setProperty(row, "badge", state.count)
                if (_navModel.get(row).label !== state.label)
                    _navModel.setProperty(row, "label", state.label)
            }
            ++row
        }
    }

    readonly property string currentTitle: {
        switch (currentKey) {
        case "all":
            return qsTr("All")
        case "suggested":
            return qsTr("Suggested matches")
        case "shows-to-identify":
            return qsTr("Identify shows")
        case "unmatched":
            return qsTr("Unmatched")
        case "folders":
            return Library.browsePath.length > 0 && Library.browseName.length > 0
                   ? Library.browseName
                   : qsTr("Folders")
        case "movies":
            return qsTr("Movies")
        case "collections":
            return qsTr("Collections")
        case "shows":
            return root.showTitle.length > 0 ? root.showTitle : qsTr("TV shows")
        case "search":
            return qsTr("Search")
        case "settings":
            return qsTr("Settings")
        case "streaming":
            return qsTr("Stream to devices")
        case "gallery":
            return qsTr("Gallery")
        default:
            return qsTr("Home")
        }
    }

    readonly property bool compactWindow: !System.isTelevision
        && Math.min(Window.width, Window.height) < S.AppTheme.breakpointCompact

    readonly property bool pageOwnsTitle: !compactWindow
        && (currentKey === "movies"
            || currentKey === "collections"
            || (currentKey === "shows" && root.showTitle.length === 0))

    readonly property string currentCount: {
        if (!compactWindow)
            return ""

        switch (currentKey) {
        case "movies":
            return String(Library.movieGrid.count)
        case "shows":
            return root.showTitle.length > 0 ? "" : String(Library.showGrid.count)
        case "collections":
            return String(Library.collections.count)
        default:
            return ""
        }
    }

    readonly property bool remoteNavigation: S.AppTheme.remoteNavigation

    function topLayer() {
        if (showDetails)
            return _detailsLoader.item
        return currentPage
    }

    function focusPage() {
        if (!remoteNavigation)
            return false
        const layer = topLayer()
        if (!layer || layer.acceptsFocus !== true)
            return false
        return layer.takeFocus() !== false
    }

    function focusDrawer() {
        if (!remoteNavigation)
            return
        if (root.modalDrawer) {
            root.modalDrawerOpen = true
            _modalDrawer.takeFocus()
        } else {
            _permanentDrawer.takeFocus()
        }
    }

    Component.onCompleted: root.ensureFocus()
    onWidthChanged: root.ensureFocus()

    onShowGateChanged: {
        if (!showGate)
            Qt.callLater(root.claimFocus)
    }

    focus: remoteNavigation

    Keys.onPressed: (event) => {
        if (!remoteNavigation || showGate || modalDrawer)
            return

        if (S.PopupRegistry.hasOpen)
            return

        if (event.key === Qt.Key_Left) {
            _permanentDrawer.takeFocus()
            event.accepted = true
        }
    }

    function selectKey(key) {
        currentKey = key
        modalDrawerOpen = false
        closeDetails(false)
        closeShow(false)
        closeCollection(false)

        if (key === "all") {
            if (Library.filterMode === Library.FilterUnmatched)
                Library.filterMode = Library.FilterEverything
            Library.showAllFiles()
        } else if (key === "unmatched") {
            Library.filterMode = Library.FilterUnmatched
            Library.showAllFiles()
        } else if (key === "folders")
            Library.browseRootList()
    }

    function handleBack() {
        if (showGate)
            return false
        if (showDetails) {
            closeDetails()
            return true
        }
        if (showMediaId > 0) {
            closeShow()
            return true
        }
        if (collectionId !== 0) {
            closeCollection()
            return true
        }
        if (modalDrawerOpen) {
            modalDrawerOpen = false
            return true
        }
        const page = currentPage
        if (page && page.goBack && page.goBack())
            return true
        return false
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    Ctrl.NavDrawer {
        id: _permanentDrawer

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: !root.modalDrawer && !root.showGate
        mode: root.drawerMode === AppShell.Rail
              ? Ctrl.NavDrawer.Rail
              : Ctrl.NavDrawer.Permanent
        iconsOnly: System.isTelevision
        items: _navModel
        currentKey: root.currentKey

        onItemActivated: (key) => root.selectKey(key)
        onExitRequested: root.focusPage()
    }

    Item {
        id: _content

        anchors.left: root.modalDrawer ? parent.left : _permanentDrawer.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: !root.showGate && !root.showDetails

        Ctrl.AppBar {
            id: _appBar

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            visible: root.showMediaId <= 0 && root.collectionId === 0
            height: visible && !isEmpty ? implicitHeight : 0
            title: root.pageOwnsTitle
                   ? ""
                   : (root.currentCount.length > 0
                      ? qsTr("%1 · %2").arg(root.currentTitle).arg(root.currentCount)
                      : root.currentTitle)
            leading: root.pageCanGoBack
                     ? Ctrl.AppBar.Back
                     : (root.modalDrawer ? Ctrl.AppBar.Menu : Ctrl.AppBar.None)
            titleLeftInset: S.AppTheme.spacing16

            onLeadingTriggered: {
                if (root.pageCanGoBack) {
                    root.currentPage.goBack()
                    return
                }
                root.modalDrawerOpen = true
            }

            Rectangle {
                id: _streamingBadge

                anchors.verticalCenter: parent.verticalCenter
                visible: Streaming.connected && root.currentKey === "home"
                width: visible ? _streamingRow.implicitWidth + S.AppTheme.spacing12 * 2 : 0
                height: _streamingRow.implicitHeight + S.AppTheme.spacing6 * 2
                radius: S.AppTheme.radiusPill
                color: S.AppTheme.surfaceVariant
                border.width: 1
                border.color: S.AppTheme.outline

                Row {
                    id: _streamingRow

                    anchors.centerIn: parent
                    spacing: S.AppTheme.spacing8

                    Rectangle {
                        id: _streamingDot

                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 4
                        color: S.AppTheme.primary

                        SequentialAnimation {
                            running: _streamingDot.visible
                            loops: Animation.Infinite

                            NumberAnimation {
                                target: _streamingDot
                                property: "opacity"
                                from: 1.0
                                to: 0.2
                                duration: 900
                                easing.type: Easing.InOutQuad
                            }

                            NumberAnimation {
                                target: _streamingDot
                                property: "opacity"
                                from: 0.2
                                to: 1.0
                                duration: 900
                                easing.type: Easing.InOutQuad
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Streaming from %1").arg(Streaming.connectedServerName)
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs13
                        font.weight: Font.Medium
                    }
                }
            }

            Ctrl.AppButton {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.currentKey === "collections" && !System.isTelevision
                         && !Streaming.connected && !root.compactWindow
                width: visible ? implicitWidth : 0
                size: Ctrl.AppButton.Medium
                variant: Ctrl.AppButton.Outlined
                iconSource: S.Icons.plus
                iconOnly: root.width < 600
                text: qsTr("New collection")
                accessibleName: qsTr("New collection")

                onClicked: _newCollection.open()
            }

            Ctrl.IconButton {
                iconSource: S.Icons.search
                accessibleName: qsTr("Search")
                visible: !System.isTelevision && !Streaming.serving
                         && !root.pageOwnsTitle
                width: visible ? implicitWidth : 0
                onClicked: root.selectKey("search")
            }
        }

        Loader {
            id: _pageLoader

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            sourceComponent: {
                if (root.showMediaId > 0)
                    return System.isTelevision ? _tvShowScreen : _showScreen

                if (root.collectionId !== 0) {
                    return System.isTelevision ? _tvCollectionScreen
                                               : _collectionScreen
                }

                if (root.keptLoaderFor(root.currentKey) !== null)
                    return null

                switch (root.currentKey) {
                case "suggested":
                    return _suggestedScreen
                case "shows-to-identify":
                    return _identifyShowsScreen
                case "folders":
                    return _foldersScreen
                case "search":
                    return _searchScreen
                case "settings":
                    return _settingsScreen
                case "streaming":
                    return _streamingScreen
                case "gallery":
                    return _galleryScreen
                default:
                    return null
                }
            }
        }

        Loader {
            id: _homeKeep

            readonly property bool current: root.showMediaId <= 0
                                            && root.collectionId === 0
                                            && root.keptLoaderFor(root.currentKey) === _homeKeep
            property bool used: false

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            active: used
            asynchronous: true
            visible: current
            sourceComponent: _homeScreen

            onCurrentChanged: if (current) used = true
            Component.onCompleted: if (current) used = true
        }

        Loader {
            id: _allKeep

            readonly property bool current: root.showMediaId <= 0
                                            && root.collectionId === 0
                                            && root.keptLoaderFor(root.currentKey) === _allKeep
            property bool used: false

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            active: used
            asynchronous: true
            visible: current
            sourceComponent: _allScreen

            onCurrentChanged: if (current) used = true
            Component.onCompleted: if (current) used = true
        }

        Loader {
            id: _moviesKeep

            readonly property bool current: root.showMediaId <= 0
                                            && root.collectionId === 0
                                            && root.keptLoaderFor(root.currentKey) === _moviesKeep
            property bool used: false

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            active: used
            asynchronous: true
            visible: current
            sourceComponent: _moviesScreen

            onCurrentChanged: if (current) used = true
            Component.onCompleted: if (current) used = true
        }

        Loader {
            id: _showsKeep

            readonly property bool current: root.showMediaId <= 0
                                            && root.collectionId === 0
                                            && root.keptLoaderFor(root.currentKey) === _showsKeep
            property bool used: false

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            active: used
            asynchronous: true
            visible: current
            sourceComponent: _showsScreen

            onCurrentChanged: if (current) used = true
            Component.onCompleted: if (current) used = true
        }

        Loader {
            id: _collectionsKeep

            readonly property bool current: root.showMediaId <= 0
                                            && root.collectionId === 0
                                            && root.keptLoaderFor(root.currentKey) === _collectionsKeep
            property bool used: false

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            active: used
            asynchronous: true
            visible: current
            sourceComponent: _collectionsScreen

            onCurrentChanged: if (current) used = true
            Component.onCompleted: if (current) used = true
        }

        Ctrl.PageSkeleton {
            id: _pageSkeleton

            readonly property var kept: root.keptLoaderFor(root.currentKey)
            readonly property bool building:
                root.showMediaId <= 0 && root.collectionId === 0
                && kept !== null && kept.used
                && (kept.status !== Loader.Ready
                    || (kept.item !== null && kept.item.pageReady === false))

            property double startedAt: 0

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _appBar.bottom
            anchors.bottom: parent.bottom
            z: 1
            visible: opacity > 0.01
            opacity: building ? 1 : 0

            onBuildingChanged: {
                if (building) {
                    startedAt = Date.now()
                    Library.noteUi("building " + root.currentKey)
                    return
                }
                if (startedAt > 0) {
                    Library.noteUi(root.currentKey + " ready in "
                                   + (Date.now() - startedAt) + " ms")
                    startedAt = 0
                }
            }

            Behavior on opacity {
                NumberAnimation { duration: 160 }
            }
        }
    }

    Rectangle {
        id: _scrim

        anchors.fill: parent
        visible: opacity > 0.01
        color: S.AppTheme.overlay
        opacity: root.modalDrawerOpen ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.modalDrawerOpen
            onClicked: root.modalDrawerOpen = false
        }
    }

    Ctrl.NavDrawer {
        id: _modalDrawer

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        property real slide: root.modalDrawerOpen ? 0 : 1

        x: -width * slide
        visible: root.modalDrawer && !root.showGate && slide < 1
        mode: Ctrl.NavDrawer.Modal
        items: _navModel
        currentKey: root.currentKey

        onItemActivated: (key) => root.selectKey(key)
        onExitRequested: {
            root.modalDrawerOpen = false
            root.focusPage()
        }

        Behavior on slide {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }
    }

    Ctrl.BottomSheet {
        id: _collectionMenu

        title: root._collectionMenuName

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.play
                title: qsTr("Open")
                onClicked: {
                    _collectionMenu.close()
                    root.openCollection(root._collectionMenuId)
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.close
                destructive: true
                title: root._collectionMenuCustom
                       ? qsTr("Remove this collection")
                       : qsTr("Take off Collections")
                subtitle: root._collectionMenuCustom
                          ? qsTr("Kept in Settings, where it can be put back")
                          : qsTr("Stays off even when a new film of it arrives")
                onClicked: {
                    _collectionMenu.close()
                    root.removeCollection(root._collectionMenuId,
                                          root._collectionMenuName,
                                          root._collectionMenuCustom)
                }
            }
        }
    }

    Ctrl.NewCollectionWizard {
        id: _newCollection

        z: 55

        onCancelled: root.claimFocus()
        onOpenRequested: (collectionId) => {
            root.selectKey("collections")
            root.openCollection(collectionId)
        }
    }

    Rectangle {
        id: _connectingScrim

        anchors.fill: parent
        z: 55
        visible: Streaming.connecting
        color: Qt.rgba(0, 0, 0, 0.45)

        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
            height: _connectingColumn.implicitHeight + 2 * S.AppTheme.spacing24
            radius: S.AppTheme.radiusMedium
            color: S.AppTheme.surface

            Column {
                id: _connectingColumn

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: S.AppTheme.spacing24
                spacing: S.AppTheme.spacing16

                Text {
                    width: parent.width
                    text: Streaming.connectingText
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs16
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }

                Ctrl.LinearProgress {
                    width: parent.width
                    indeterminate: Streaming.connectProgress < 0
                    value: Math.max(0, Streaming.connectProgress)
                    fillColor: S.AppTheme.textPrimary
                }

                Ctrl.AppButton {
                    id: _cancelConnect

                    anchors.right: parent.right
                    variant: Ctrl.AppButton.Outlined
                    size: Ctrl.AppButton.Medium
                    text: qsTr("Cancel")
                    onClicked: Streaming.cancelConnect()
                }
            }
        }

        onVisibleChanged: {
            if (!root.remoteNavigation)
                return
            if (visible) {
                _cancelConnect.forceActiveFocus(Qt.TabFocusReason)
                return
            }
            _focusAfterPopup.restart()
        }
    }

    Ctrl.Snackbar {
        id: _toast

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: S.AppTheme.spacing16
        z: 50

        onActionTriggered: {
            if (root._discardedShow.length > 0) {
                Library.restoreShow(root._discardedShow)
                root._discardedShow = ""
                return
            }
            if (root._removedCollection === 0)
                return
            Library.restoreCollection(root._removedCollection)
            root._removedCollection = 0
        }
    }

    Connections {
        target: Library

        function onErrorRaised(message) { _toast.show(message) }

        function onFolderAdded(displayName) {
            _toast.show(qsTr("Added %1").arg(displayName))
        }

        function onFolderUnavailable(displayName) {
            _toast.show(qsTr("%1 is offline, its files were kept").arg(displayName))
        }

        function onFileMissing(handle, displayName) {
            root.menuHandle = handle
            root.menuName = displayName
            root.missingFocusBefore = root.Window.activeFocusItem
            _missingDialog.open()
        }

        function onStorageVolumeOffered(handle, displayName) {
            if (root.offeredVolumes[handle] === true)
                return
            root.offeredVolumes[handle] = true

            root.volumeQueue.push({ handle: handle, displayName: displayName })
            root.showNextVolumeOffer()
        }
    }

    Connections {
        target: System

        function onLogFileSaved(outcome) {
            if (outcome === 1)
                _toast.show(qsTr("Log saved"))
            else if (outcome === 2)
                _toast.show(qsTr("Log saved to the Download folder as makimedia.log"))
            else if (outcome === -3)
                _toast.show(qsTr("The log is in Android/data/com.topicdev.makimedia/files/logs"))
            else if (outcome < 0)
                _toast.show(qsTr("The log could not be saved"))
        }
    }

    property var missingFocusBefore: null

    function returnFocusAfterMissing() {
        const item = root.missingFocusBefore
        root.missingFocusBefore = null
        if (item && item.visible && item.enabled) {
            item.forceActiveFocus(Qt.TabFocusReason)
            return
        }
        root.claimFocus()
    }

    property var volumeQueue: []
    property var offeredVolumes: ({})
    property string volumeHandle: ""
    property string volumeName: ""

    function showNextVolumeOffer() {
        if (_volumeDialog.opened || volumeQueue.length === 0)
            return
        if (Setup.shown)
            return
        const next = volumeQueue.shift()
        volumeHandle = next.handle
        volumeName = next.displayName
        _volumeDialog.open()
    }

    Connections {
        target: Setup

        function onActiveChanged() {
            if (Setup.shown) {
                if (_volumeDialog.opened) {
                    Library.noteUi("setup came up over the drive question - "
                                   + "asking again once it has gone")
                    root.volumeQueue.unshift({ handle: root.volumeHandle,
                                               displayName: root.volumeName })
                    _volumeDialog.close()
                }
                return
            }

            if (root.volumeQueue.length > 0) {
                Library.noteUi("setup is out of the way - asking about the "
                               + root.volumeQueue.length + " waiting drive(s)")
                Qt.callLater(root.showNextVolumeOffer)
            }
        }
    }

    Ctrl.AppDialog {
        id: _volumeDialog

        z: 60
        title: qsTr("Scan %1?").arg(root.volumeName)
        message: qsTr("Makimedia found %1 on this device. Scanning it adds the videos on it to your library. You can change this later in Settings.")
                 .arg(root.volumeName)
        dismissText: qsTr("Not this one")
        acceptText: qsTr("Scan it")

        onAccepted: Library.adoptStorageVolume(root.volumeHandle)
        onDismissed: Library.declineStorageVolume(root.volumeHandle)
        onClosed: {
            root.showNextVolumeOffer()
            if (!_volumeDialog.opened)
                Qt.callLater(root.claimFocus)
        }
    }

    Connections {
        target: S.DevTools

        function onForceWelcomeChanged() {
            if (S.DevTools.forceWelcome)
                root.welcomeDismissed = false
        }

        function onFileMissingRequested(displayName) {
            root.menuName = displayName
            root.missingFocusBefore = root.Window.activeFocusItem
            _missingDialog.open()
        }

        function onFolderUnavailableRequested(displayName) {
            _toast.show(qsTr("%1 is offline, its files were kept").arg(displayName))
        }

        function onErrorRequested(message) { _toast.show(message) }
    }

    Ctrl.AppDialog {
        id: _missingDialog

        z: 60
        title: qsTr("File not found")
        message: qsTr("%1 is no longer where Makimedia indexed it. The drive may be disconnected, or the file was moved or deleted.")
                 .arg(root.menuName)
        acceptText: qsTr("OK")

        onClosed: Qt.callLater(root.returnFocusAfterMissing)

        Text {
            width: parent.width
            text: qsTr("It stays in your library, with its watch history, until you rescan its folder.")
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs12
            wrapMode: Text.Wrap
        }
    }

    Ctrl.BottomSheet {
        id: _copiesSheet

        z: 70
        title: qsTr("Which copy?")

        Column {
            width: parent.width
            spacing: 0

            Repeater {
                model: root.copies

                delegate: Ctrl.SheetRow {
                    required property var modelData

                    width: parent.width
                    showRadio: false
                    iconSource: S.Icons.play
                    title: modelData.displayName
                    subtitle: root.copyQuality(modelData)
                    onClicked: root.playCopy(modelData.handle)
                }
            }
        }
    }

    Ctrl.BottomSheet {
        id: _itemMenu

        title: root.menuName

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.play
                title: qsTr("Play")
                onClicked: {
                    _itemMenu.close()
                    root.playRequested(root.menuHandle)
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.check
                title: root.menuInfo.watched === true
                       ? qsTr("Mark as unwatched")
                       : qsTr("Mark as watched")
                enabled: root.menuInfo.indexed === true
                onClicked: {
                    Library.setWatched(root.menuInfo.fileId,
                                       root.menuInfo.watched !== true)
                    _itemMenu.close()
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                visible: Metadata.available && !Streaming.connected
                         && root.menuInfo.isEpisode !== true
                iconSource: S.Icons.search
                title: root.menuInfo.matched === true
                       ? qsTr("Fix match")
                       : qsTr("Find this title")
                subtitle: root.menuInfo.matchedTitle !== undefined
                          && root.menuInfo.matchedTitle.length > 0
                          ? root.menuInfo.matchedTitle
                          : qsTr("Nothing matched to this file yet")
                enabled: root.menuInfo.indexed === true
                onClicked: {
                    _itemMenu.close()
                    root.openFixMatchFor(root.menuHandle)
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.close
                title: qsTr("Remove from Continue watching")
                enabled: root.menuInfo.continueWatching === true
                onClicked: {
                    Library.removeFromContinueWatching(root.menuInfo.fileId)
                    _itemMenu.close()
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                visible: !System.isTelevision
                         && root.menuInfo.indexed === true
                         && (root.menuInfo.present === false
                             || (root.menuInfo.matched !== true
                                 && root.menuInfo.suggested !== true))
                iconSource: S.Icons.close
                title: qsTr("Remove from library")
                subtitle: root.menuInfo.present === false
                          ? qsTr("The file is missing. Settings can put it back if it returns.")
                          : qsTr("The file stays on disk. Settings can put it back.")
                onClicked: {
                    _itemMenu.close()
                    Library.removeFromLibrary(root.menuHandle)
                    _toast.show(qsTr("Removed from library"))
                }
            }
        }
    }

    Loader {
        id: _detailsLoader

        anchors.left: root.modalDrawer ? parent.left : _permanentDrawer.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        z: 20
        active: root.showDetails
        visible: root.showDetails

        sourceComponent: System.isTelevision ? _tvDetailsScreen : _detailsScreen
    }

    Component {
        id: _tvDetailsScreen

        TV.TvDetailsRouter {
            handle: root.detailsHandle

            onBackRequested: root.closeDetails()
            onPlayRequested: (handle) => root.playTitle(handle, false)
            onPlayFromStartRequested: (handle) => root.playTitle(handle, true)
            onDetailsRequested: (handle) => root.openDetails(handle)
            onShowRequested: (mediaId) => {
                root.openShow(mediaId, "")
                root.closeDetails(false)
            }
            onDrawerRequested: root.focusDrawer()

            Component.onCompleted: {
                if (root.remoteNavigation)
                    takeFocus()
            }
        }
    }

    Component {
        id: _detailsScreen

        DetailsScreen {
            handle: root.detailsHandle
            autoOpenFixMatch: root.detailsOpensFixMatch

            onBackRequested: root.closeDetails()
            onPlayRequested: (handle) => root.playTitle(handle, false)
            onPlayFromStartRequested: (handle) => root.playTitle(handle, true)
            onDetailsRequested: (handle) => root.openDetails(handle)
            onCollectionRequested: (id) => {
                root.closeDetails()
                root.openCollection(id)
            }

            Component.onCompleted: {
                if (root.remoteNavigation)
                    takeFocus()
            }
        }
    }

    Loader {
        id: _welcomeLoader

        anchors.fill: parent
        z: 40
        active: root.showGate
        visible: root.showGate
        sourceComponent: root.needsPermission ? _permissionScreen : _welcomeScreen
    }

    Component {
        id: _welcomeScreen

        WelcomeScreen {
            onChooseFolderRequested: Library.addFolder()
            onDismissed: {
                S.DevTools.forceWelcome = false
                root.welcomeDismissed = true
            }
        }
    }

    Component {
        id: _permissionScreen

        PermissionScreen {
            onGrantRequested: {
                S.DevTools.forcePermissionGate = false
                System.requestVideoPermission()
            }
            onSettingsRequested: {
                S.DevTools.forcePermissionGate = false
                System.openAppSettings()
            }
            onSkipRequested: {
                S.DevTools.forcePermissionGate = false
                if (System.videoAccessPartial)
                    System.acceptPartialVideoAccess()
                else
                    System.continueWithoutVideos()

                if (root.remoteNavigation)
                    Qt.callLater(root.focusDrawer)
            }
        }
    }

    Component {
        id: _homeScreen

        HomeScreen {
            onDetailsRequested: (handle) => root.openDetails(handle)
            onPlayRequested: (handle) => root.playTitle(handle, false)
            onShowRequested: (mediaId, title) => root.openShow(mediaId, title)
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _allScreen

        AllScreen {
            onDetailsRequested: (handle) => root.openDetails(handle)
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _foldersScreen

        FoldersScreen {
            onDetailsRequested: (handle) => root.openDetails(handle)
            onItemMenuRequested: (handle, name) => root.openItemMenu(handle, name)
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _moviesScreen

        MediaGridScreen {
            model: Library.movieGrid
            toolbar: true
            seenAsCheck: true
            titleText: root.pageOwnsTitle ? qsTr("Movies") : ""
            showsSearch: root.pageOwnsTitle && !System.isTelevision && !Streaming.serving

            onSearchRequested: root.selectKey("search")
            sortValue: Library.movieSort
            filterValue: Library.movieFilter
            countText: Library.movieGrid.count === Library.movies.count
                       ? qsTr("%n movie(s)", "", Library.movies.count)
                       : qsTr("%1 of %n movie(s)", "", Library.movies.count)
                         .arg(Library.movieGrid.count)
            emptyTitle: qsTr("No films matched yet")
            emptyMessage: Metadata.available
                          ? qsTr("Films appear here once they are matched. Check Settings if nothing arrives.")
                          : qsTr("Matching needs a TMDB key. Everything still plays from All and Folders.")

            onDetailsRequested: (handle) => root.openDetails(handle)
            onDrawerRequested: root.focusDrawer()
            onSortChosen: (sort) => Library.movieSort = sort
            onFilterChosen: (filter) => Library.movieFilter = filter
            gridSize: Library.movieGridSize
            genres: Library.movieGenres
            genreValue: Library.movieGenre
            onGridSizeChosen: (size) => Library.movieGridSize = size
            onGenreChosen: (genre) => Library.movieGenre = genre
        }
    }

    Component {
        id: _showsScreen

        MediaGridScreen {
            model: Library.showGrid
            toolbar: true
            seenAsCheck: true
            titleText: root.pageOwnsTitle ? qsTr("TV shows") : ""
            showsSearch: root.pageOwnsTitle && !System.isTelevision && !Streaming.serving

            onSearchRequested: root.selectKey("search")
            sortValue: Library.showSort
            filterValue: Library.showFilter
            countText: Library.showGrid.count === Library.shows.count
                       ? qsTr("%n show(s)", "", Library.shows.count)
                       : qsTr("%1 of %n show(s)", "", Library.shows.count)
                         .arg(Library.showGrid.count)
            onSortChosen: (sort) => Library.showSort = sort
            onFilterChosen: (filter) => Library.showFilter = filter
            gridSize: Library.showGridSize
            genres: Library.showGenres
            genreValue: Library.showGenre
            onGridSizeChosen: (size) => Library.showGridSize = size
            onGenreChosen: (genre) => Library.showGenre = genre
            emptyTitle: qsTr("No shows matched yet")
            emptyMessage: Metadata.available
                          ? qsTr("A show appears here once one of its episodes is matched.")
                          : qsTr("Matching needs a TMDB key. Everything still plays from All and Folders.")

            onShowRequested: (mediaId, title) => root.openShow(mediaId, title)
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _collectionsScreen

        CollectionsScreen {
            showsTitle: root.pageOwnsTitle
            showsSearch: root.pageOwnsTitle && !System.isTelevision && !Streaming.serving

            onSearchRequested: root.selectKey("search")
            onDrawerRequested: root.focusDrawer()
            onCollectionRequested: (collectionId) => root.openCollection(collectionId)
            onMenuRequested: (collectionId, name, custom) => {
                root.openCollectionMenu(collectionId, name, custom)
            }
        }
    }

    Component {
        id: _collectionScreen

        CollectionScreen {
            collectionId: root.collectionId

            onBackRequested: root.closeCollection()
            onPlayRequested: (handle) => root.playTitle(handle, false)
            onDetailsRequested: (handle) => root.openDetails(handle)
        }
    }

    Component {
        id: _tvCollectionScreen

        TV.TvCollectionScreen {
            collectionId: root.collectionId

            onBackRequested: root.closeCollection()
            onPlayRequested: (handle) => root.playTitle(handle, false)
            onDetailsRequested: (handle) => root.openDetails(handle)

            Component.onCompleted: takeFocus()
        }
    }

    Component {
        id: _suggestedScreen

        MediaGridScreen {
            model: Library.suggestions
            emptyTitle: qsTr("Nothing to confirm")
            emptyMessage: qsTr("Titles TMDB was unsure about appear here. There are none.")

            onDetailsRequested: (handle) => root.openDetails(handle)
            onShowRequested: (mediaId, title) => root.openShow(mediaId, title)
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _identifyShowsScreen

        UnmatchedShowsScreen {
            onDrawerRequested: root.focusDrawer()
            onDiscardRequested: (fileHandles, title, folder) =>
                                root.discardShow(fileHandles, title, folder)

            Component.onCompleted: {
                if (root.remoteNavigation)
                    takeFocus()
            }
        }
    }

    Component {
        id: _tvShowScreen

        TV.TvShowScreen {
            mediaId: root.showMediaId

            onBackRequested: root.closeShow()
            onPlayRequested: (handle) => root.playRequested(handle)
            onDetailsRequested: (handle) => root.openDetails(handle)
            onDrawerRequested: root.focusDrawer()

            Component.onCompleted: {
                if (root.remoteNavigation)
                    takeFocus()
            }
        }
    }

    Component {
        id: _showScreen

        TvShowDetailsScreen {
            mediaId: root.showMediaId

            onBackRequested: root.closeShow()
            onPlayRequested: (handle) => root.playRequested(handle)
            onDetailsRequested: (handle) => root.openDetails(handle)

            Component.onCompleted: {
                if (root.remoteNavigation)
                    takeFocus()
            }
        }
    }

    Component {
        id: _searchScreen

        SearchScreen {
            onDetailsRequested: (handle) => root.openDetails(handle)
            onShowRequested: (mediaId, title) => root.openShow(mediaId, title)
            onCollectionRequested: (id) => root.openCollection(id)
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _settingsScreen

        SettingsScreen {
            onScanFoldersRequested: root.selectKey("folders")
            onDrawerRequested: root.focusDrawer()
            onRemotePlayRequested: (url) => root.remotePlayRequested(url)
        }
    }

    Component {
        id: _streamingScreen

        StreamingScreen {
            onDrawerRequested: root.focusDrawer()
        }
    }

    Component {
        id: _galleryScreen

        GalleryScreen {}
    }
}
