pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal detailsRequested(string handle)
    signal playRequested(string handle)
    signal menuRequested(string handle, string name)

    function timeLeftText(minutes) {
        if (minutes <= 0)
            return ""
        if (minutes < 60)
            return qsTr("%1 min left").arg(minutes)
        const hours = Math.floor(minutes / 60)
        const rest = minutes % 60
        return rest > 0 ? qsTr("%1 h %2 min left").arg(hours).arg(rest)
                        : qsTr("%1 h left").arg(hours)
    }
    signal drawerRequested()

    readonly property bool acceptsFocus: hasAnything || Streaming.offerVisible
                                         || showsWindowsInvite

    readonly property bool showsWindowsInvite:
        Streaming.canConnect && !Streaming.connected && !Streaming.connecting
        && !Streaming.offerVisible && Streaming.storedLibraries.count === 0
        && !AppSettings.windowsInviteDismissed

    function focusLastRow() {
        for (let i = rows.length - 1; i >= 0; --i) {
            if (focusRow(i))
                return
        }
    }

    property int currentRow: 0

    property var genreLists: []
    readonly property var rows: [_continueRow, _recentRow, _surpriseRow].concat(genreLists)

    function collectGenreLists() {
        const lists = []
        for (let i = 0; i < _genreRows.count; ++i) {
            const row = _genreRows.itemAt(i)
            if (row)
                lists.push(row.list)
        }
        genreLists = lists
    }

    function rowUsable(i) {
        return i >= 0 && i < rows.length && rows[i].visible && rows[i].count > 0
    }

    function adoptRow(list) {
        const i = rows.indexOf(list)
        if (i < 0 || i === currentRow)
            return
        Library.noteUi("home cursor follows focus to " + rowState(i))
        currentRow = i
    }

    function restartContinueRow() {
        _continueRow.forceLayout()
        if (_continueRow.currentIndex === 0) {
            Library.noteUi("home leaves the continue cursor on the first tile")
            return
        }
        Library.noteUi("home puts the continue cursor back on the first tile, from "
                       + _continueRow.currentIndex)
        _continueRow.currentIndex = 0
        _continueRow.positionViewAtBeginning()
    }

    function rowState(i) {
        if (i < 0 || i >= rows.length)
            return "row " + i + " of " + rows.length
        return "row " + i + " of " + rows.length + " (shown "
               + rows[i].visible + ", " + rows[i].count + " in it)"
    }

    function focusRow(i) {
        if (!rowUsable(i)) {
            Library.noteUi("home cannot focus " + rowState(i))
            return false
        }
        Library.noteUi("home focuses " + rowState(i))
        currentRow = i
        if (rows[i].currentIndex < 0)
            rows[i].currentIndex = 0
        rows[i].forceActiveFocus()
        showRow(rows[i])
        return true
    }

    function takeFocus() {
        Library.noteUi("home takes focus with " + rows.length + " rows")
        for (let i = 0; i < rows.length; ++i) {
            if (focusRow(i))
                return true
        }
        return root.focusTop()
    }

    onHasAnythingChanged: {
        if (!hasAnything || !S.AppTheme.remoteNavigation)
            return
        Qt.callLater(root.claimLooseFocus)
    }

    function claimLooseFocus() {
        const holder = Window.activeFocusItem
        if (holder && holder.visible && holder !== Window.contentItem)
            return
        Library.noteUi("home takes the focus nothing was holding")
        root.takeFocus()
    }

    function moveRow(delta) {
        Library.noteUi("home steps " + (delta < 0 ? "up" : "down")
                       + " from row " + currentRow)
        let i = currentRow + delta
        while (i >= 0 && i < rows.length && !rowUsable(i))
            i += delta
        if (i < 0 && root.focusTop())
            return
        focusRow(i)
    }

    function focusTop() {
        if (Streaming.offerVisible) {
            Library.noteUi("home steps up onto the offer to connect")
            _connectOffer.forceActiveFocus(Qt.TabFocusReason)
            _scroll.contentY = 0
            return true
        }
        if (root.showsWindowsInvite) {
            Library.noteUi("home steps up onto the invitation to stream from a PC")
            _inviteMain.forceActiveFocus(Qt.TabFocusReason)
            _scroll.contentY = 0
            return true
        }
        return false
    }

    function showRow(list) {
        const top = list.mapToItem(_column, 0, 0).y
        const bottom = top + list.height
        const header = 40

        const maxY = Math.max(0, _scroll.contentHeight - _scroll.height)

        if (top - header < _scroll.contentY)
            _scroll.contentY = Math.max(0, top - header)
        else if (bottom > _scroll.contentY + _scroll.height)
            _scroll.contentY = Math.min(maxY, bottom - _scroll.height)
    }
    signal showRequested(var mediaId, string title)

    readonly property bool tabletSized:
        !System.usesWindowGeometry
        && Math.min(Window.width, Window.height) >= S.AppTheme.breakpointCompact

    readonly property int homePosterWidth:
        tabletSized ? Math.round(S.AppTheme.posterTileWidth * 1.1)
                    : S.AppTheme.posterTileWidth

    readonly property real wideRowHeight:
        S.AppTheme.wideTileWidth * S.AppTheme.backdropAspectRatio
        + S.AppTheme.tileLabelHeight + S.AppTheme.focusHeadroom
    readonly property real continueRowHeight: wideRowHeight + S.AppTheme.fs11 + 6
    readonly property real posterRowHeight:
        homePosterWidth * S.AppTheme.posterAspectRatio
        + S.AppTheme.tileLabelHeight + S.AppTheme.focusHeadroom

    readonly property int rowInset: S.AppTheme.focusHeadroom
                                    + (S.AppTheme.remoteNavigation ? S.AppTheme.spacing24 : 0)

    readonly property bool hasAnything: Library.continueFiles.count > 0
                                        || Library.recentTitles.count > 0
                                        || Library.genreRows.count > 0

    property bool settled: false

    readonly property bool pageReady: settled

    readonly property int settlingCounts: Library.continueFiles.count
                                          + Library.recentTitles.count
                                          + Library.genreRows.count

    onSettlingCountsChanged: {
        if (!root.settled) {
            _reveal.stop()
            _settle.restart()
        }
    }

    Timer {
        id: _settle

        interval: 150
        onTriggered: {
            Library.noteUi("home lists settled with " + Library.genreRows.count
                           + " genre rows")
            _reveal.restart()
        }
    }

    Timer {
        id: _reveal

        interval: 250
        onTriggered: root.settled = true
    }

    Component.onCompleted: _settle.start()

    Connections {
        target: Library.continueFiles

        function onRowsInserted(parent, first, last) {
            if (first === 0)
                Qt.callLater(root.restartContinueRow)
        }

        function onRowsMoved(source, start, end, destination, row) {
            if (row === 0)
                Qt.callLater(root.restartContinueRow)
        }

        function onModelReset() {
            Qt.callLater(root.restartContinueRow)
        }
    }

    Rectangle {
        id: _connectCard

        onVisibleChanged: {
            if (!visible && !Streaming.connecting
                    && (_notNow.activeFocus || _connectOffer.activeFocus)) {
                Qt.callLater(root.takeFocus)
            }
        }

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: visible ? S.AppTheme.spacing8 : 0
        visible: Streaming.offerVisible
        height: visible ? _connectRow.implicitHeight + 2 * S.AppTheme.spacing16 : 0
        radius: S.AppTheme.radiusMedium
        color: S.AppTheme.surfaceVariant
        z: 2

        Item {
            id: _connectRow

            anchors.fill: parent
            anchors.margins: S.AppTheme.spacing16
            implicitHeight: Math.max(_connectText.implicitHeight, _connectButtons.implicitHeight)

            Column {
                id: _connectText

                anchors.left: parent.left
                anchors.right: _connectButtons.left
                anchors.rightMargin: S.AppTheme.spacing12
                anchors.verticalCenter: parent.verticalCenter
                spacing: S.AppTheme.spacing2

                Text {
                    width: parent.width
                    text: qsTr("%1 is streaming").arg(Streaming.offerServerName)
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs16
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: qsTr("Watch its films and shows on this device")
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs13
                    wrapMode: Text.Wrap
                }
            }

            Row {
                id: _connectButtons

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton {
                    id: _notNow

                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Plain
                    text: qsTr("Not now")
                    Keys.onLeftPressed: root.drawerRequested()
                    Keys.onRightPressed: _connectOffer.forceActiveFocus(Qt.TabFocusReason)
                    Keys.onDownPressed: root.takeFocus()
                    onClicked: {
                        Streaming.dismissOffer()
                        Qt.callLater(root.takeFocus)
                    }
                }

                Ctrl.AppButton {
                    id: _connectOffer

                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Filled
                    text: qsTr("Connect")
                    Keys.onLeftPressed: _notNow.forceActiveFocus(Qt.TabFocusReason)
                    Keys.onDownPressed: root.takeFocus()
                    onClicked: Streaming.connectToOffer()
                }
            }
        }
    }

    Rectangle {
        id: _inviteCard

        onVisibleChanged: {
            if (!visible && (_inviteMain.activeFocus || _inviteLater.activeFocus))
                Qt.callLater(root.takeFocus)
        }

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: visible ? S.AppTheme.spacing8 : 0
        visible: root.showsWindowsInvite
        height: visible ? _inviteRow.implicitHeight + 2 * S.AppTheme.spacing16 : 0
        radius: S.AppTheme.radiusMedium
        color: S.AppTheme.surfaceVariant
        z: 2

        Item {
            id: _inviteRow

            anchors.fill: parent
            anchors.margins: S.AppTheme.spacing16
            implicitHeight: Math.max(_inviteText.implicitHeight, _inviteButtons.implicitHeight)

            Column {
                id: _inviteText

                anchors.left: parent.left
                anchors.right: _inviteButtons.left
                anchors.rightMargin: S.AppTheme.spacing12
                anchors.verticalCenter: parent.verticalCenter
                spacing: S.AppTheme.spacing2

                Text {
                    width: parent.width
                    text: qsTr("Films and shows on your PC? Watch them here")
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs16
                    font.weight: Font.DemiBold
                    wrapMode: Text.Wrap
                }

                Text {
                    width: parent.width
                    text: System.isTelevision
                          ? qsTr("Install Makimedia for Windows from the Microsoft Store on your PC (or visit makimedia.org there), start streaming, and this TV finds it.")
                          : qsTr("Install Makimedia for Windows from the Microsoft Store, start streaming, and this device finds your PC.")
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs13
                    wrapMode: Text.Wrap
                }
            }

            Row {
                id: _inviteButtons

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton {
                    id: _inviteLater

                    visible: !System.isTelevision
                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Plain
                    text: qsTr("Not now")
                    onClicked: AppSettings.windowsInviteDismissed = true
                }

                Ctrl.AppButton {
                    id: _inviteMain

                    size: Ctrl.AppButton.Medium
                    variant: System.isTelevision ? Ctrl.AppButton.Outlined
                                                 : Ctrl.AppButton.Filled
                    text: System.isTelevision ? qsTr("Got it") : qsTr("Get it")
                    Keys.onLeftPressed: root.drawerRequested()
                    Keys.onDownPressed: root.takeFocus()
                    onClicked: {
                        if (!System.isTelevision)
                            Qt.openUrlExternally(Streaming.windowsAppUrl)
                        AppSettings.windowsInviteDismissed = true
                    }
                }
            }
        }
    }

    Flickable {
        id: _scroll

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _connectCard.visible ? _connectCard.bottom
                                          : (_inviteCard.visible ? _inviteCard.bottom : parent.top)
        anchors.topMargin: _connectCard.visible || _inviteCard.visible ? S.AppTheme.spacing8 : 0
        anchors.bottom: parent.bottom
        contentHeight: _column.implicitHeight + 2 * S.AppTheme.spacing16
        clip: true
        visible: root.hasAnything
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Column {
            id: _column

            x: S.AppTheme.spacing16
            width: parent.width - 2 * S.AppTheme.spacing16
                   - S.AppTheme.scrollBarWidth
            spacing: S.AppTheme.spacing6

            Ctrl.RowHeader {
                width: parent.width
                visible: Library.continueFiles.count > 0
                height: visible ? implicitHeight : 0
                title: qsTr("Continue watching")
                countText: String(Library.continueFiles.count)
            }

            Ctrl.Rail {
                id: _continueRow

                width: parent.width
                height: visible ? root.continueRowHeight : 0
                visible: Library.continueFiles.count > 0
                spacing: S.AppTheme.spacing12
                model: Library.continueFiles
                cacheBuffer: 0
                reuseItems: true
                currentIndex: 0
                leadInset: root.rowInset
                tailInset: S.AppTheme.focusHeadroom

                onLeftEdgeReached: root.drawerRequested()
                onActivated: {
                    if (_continueRow.currentItem)
                        _continueRow.currentItem.activate()
                }
                Keys.onUpPressed: root.moveRow(-1)
                Keys.onDownPressed: root.moveRow(1)
                onActiveFocusChanged: {
                    if (activeFocus)
                        root.adoptRow(_continueRow)
                }

                delegate: Ctrl.MediaTile {
                    required property string handle
                    required property string displayName
                    required property real watchProgress
                    required property string remainingText
                    required property string titleText
                    required property int artworkStamp
                    required property string thumbnail
                    required property string posterPath
                    required property string backdropPath
                    required property string stillPath
                    required property bool isUnmatched
                    required property string episodeLabel
                    required property int season
                    required property int episode
                    required property string episodeTitle
                    required property int matchedYear
                    required property bool upNext
                    required property int minutesLeft

                    readonly property var art: {
                        void artworkStamp
                        if (stillPath.length > 0) {
                            const still = Metadata.stillUrl(stillPath, 342)
                            if (still.length > 0)
                                return { url: still, portrait: false }
                        }
                        if (backdropPath.length > 0) {
                            const back = Metadata.backdropUrl(backdropPath, 342)
                            if (back.length > 0)
                                return { url: back, portrait: false }
                        }
                        if (posterPath.length > 0) {
                            const poster = Metadata.posterUrl(posterPath, 342)
                            if (poster.length > 0)
                                return { url: poster, portrait: true }
                        }
                        return { url: thumbnail, portrait: false }
                    }

                    shape: Ctrl.MediaTile.Wide
                    width: S.AppTheme.wideTileWidth
                    unmatched: isUnmatched
                    artSource: art.url
                    artPortrait: art.portrait
                    title: titleText
                    meta: {
                        if (episode > 0) {
                            const code = season > 0
                                ? qsTr("S%1 · E%2").arg(season).arg(episode)
                                : qsTr("Special %1").arg(episode)
                            return episodeTitle.length > 0 ? code + "  ·  " + episodeTitle : code
                        }
                        if (isUnmatched)
                            return qsTr("Unmatched")
                        return matchedYear > 0 ? String(matchedYear) : ""
                    }
                    detail: {
                        const status = upNext ? qsTr("Next episode")
                                              : root.timeLeftText(minutesLeft)
                        if (!S.AppTheme.remoteNavigation || !highlighted)
                            return status
                        const hint = qsTr("Hold OK for details")
                        return status.length > 0 ? status + "  ·  " + hint : hint
                    }
                    detailAccent: upNext
                    badge: upNext ? Ctrl.MediaTile.Accent : Ctrl.MediaTile.None
                    badgeText: upNext ? qsTr("NEXT") : ""
                    playHint: true
                    holdable: true
                    progress: watchProgress
                    highlighted: ListView.isCurrentItem && _continueRow.activeFocus

                    function activate() {
                        root.playRequested(handle)
                    }

                    function openMenu() {
                        root.menuRequested(handle, titleText)
                    }

                    onClicked: activate()
                    onContextRequested: openMenu()
                    onHeld: openMenu()
                }
            }

            Ctrl.RowHeader {
                width: parent.width
                visible: Library.recentTitles.count > 0
                height: visible ? implicitHeight : 0
                title: qsTr("Recently added")
                countText: String(Library.recentTitles.count)
            }

            Ctrl.Rail {
                id: _recentRow

                width: parent.width
                height: visible ? root.posterRowHeight + 8 : 0
                visible: Library.recentTitles.count > 0
                spacing: S.AppTheme.spacing12
                model: Library.recentTitles
                cacheBuffer: 0
                reuseItems: true
                currentIndex: 0
                leadInset: root.rowInset
                tailInset: S.AppTheme.focusHeadroom

                onLeftEdgeReached: root.drawerRequested()
                onActivated: {
                    if (_recentRow.currentItem)
                        _recentRow.currentItem.activate()
                }
                Keys.onUpPressed: root.moveRow(-1)
                Keys.onDownPressed: root.moveRow(1)
                onActiveFocusChanged: {
                    if (activeFocus)
                        root.adoptRow(_recentRow)
                }

                delegate: Item {
                    id: _recent

                    required property var mediaId
                    required property string kind
                    required property string handle
                    required property string title
                    required property int year
                    required property int seasonCount
                    required property string posterPath
                    required property bool watched
                    required property real watchProgress
                    required property int artworkStamp
                    required property int newCount

                    readonly property bool isShow: _recent.kind === "tv"

                    width: _recentTile.width
                    height: _recentTile.implicitHeight

                    function activate() {
                        if (_recent.isShow)
                            root.showRequested(_recent.mediaId, _recent.title)
                        else
                            root.detailsRequested(_recent.handle)
                    }

                    Ctrl.StackedShowTile {
                        id: _recentTile

                        width: root.homePosterWidth
                        stacked: _recent.isShow
                        artSource: {
                            void _recent.artworkStamp
                            return Metadata.posterUrl(_recent.posterPath, 342)
                        }
                        title: _recent.title
                        meta: {
                            if (_recent.isShow)
                                return qsTr("%n season(s)", "", Math.max(1, _recent.seasonCount))
                            return _recent.year > 0 ? String(_recent.year) : ""
                        }
                        newCount: _recent.isShow ? _recent.newCount : 0
                        watched: _recent.watched
                        progress: _recent.watchProgress
                        highlighted: _recent.ListView.isCurrentItem && _recentRow.activeFocus

                        onClicked: _recent.activate()
                    }
                }
            }

            Connections {
                target: _surpriseRow

                function onActiveFocusChanged() {
                    if (_surpriseRow.activeFocus)
                        root.adoptRow(_surpriseRow)
                }
            }

            Ctrl.SurpriseMe {
                id: _surpriseRow

                width: parent.width
                visible: Library.movies.count > 0 || Library.shows.count > 0
                height: visible ? implicitHeight : 0

                onSurpriseRequested: {
                    const previous = _surpriseRow.hasPick ? _surpriseRow.pick.mediaId : 0
                    _surpriseRow.pick = Library.surpriseMe(previous)
                    _surpriseRow.searched = true
                }
                onPlayRequested: (chosen) => root.playRequested(chosen.playHandle)
                onDetailsRequested: (chosen) => {
                    if (chosen.kind === "tv")
                        root.showRequested(chosen.mediaId, chosen.title)
                    else
                        root.detailsRequested(chosen.handle)
                }
                onLeftEdgeReached: root.drawerRequested()
                onUpRequested: root.moveRow(-1)
                onDownRequested: root.moveRow(1)
            }

            Repeater {
                id: _genreRows

                model: Library.genreRows

                onItemAdded: Qt.callLater(root.collectGenreLists)
                onItemRemoved: Qt.callLater(root.collectGenreLists)

                delegate: Column {
                    id: _genre

                    required property string genre
                    required property int titleCount
                    required property var titles

                    readonly property alias list: _genreList

                    readonly property bool near: {
                        const top = _genre.y
                        const bottom = top + _genre.height
                        const from = _scroll.contentY - root.posterRowHeight
                        const to = _scroll.contentY + _scroll.height
                                   + root.posterRowHeight
                        return bottom > from && top < to
                    }

                    property bool wanted: false

                    onNearChanged: if (near && _laidOut.hasRun) wanted = true

                    Timer {
                        id: _laidOut

                        property bool hasRun: false

                        interval: 50
                        running: !_laidOut.hasRun

                        onTriggered: {
                            hasRun = true
                            if (_genre.near)
                                _genre.wanted = true
                        }
                    }

                    width: _column.width
                    spacing: _column.spacing

                    Ctrl.RowHeader {
                        width: parent.width
                        title: _genre.genre
                        countText: String(_genre.titleCount)
                    }

                    Ctrl.Rail {
                        id: _genreList

                        width: parent.width
                        height: root.posterRowHeight
                        spacing: S.AppTheme.spacing12
                        model: _genre.wanted ? _genre.titles : 0
                        cacheBuffer: 0
                        reuseItems: true
                        currentIndex: 0
                        leadInset: root.rowInset
                        tailInset: S.AppTheme.focusHeadroom

                        onLeftEdgeReached: root.drawerRequested()
                        onActivated: {
                            if (_genreList.currentItem)
                                _genreList.currentItem.activate()
                        }
                        Keys.onUpPressed: root.moveRow(-1)
                        Keys.onDownPressed: root.moveRow(1)
                        onActiveFocusChanged: {
                            if (activeFocus)
                                root.adoptRow(_genreList)
                        }

                        delegate: Item {
                            id: _title

                            required property var mediaId
                            required property string kind
                            required property string handle
                            required property string title
                            required property string posterPath
                            required property string summary
                            required property bool watched
                            required property real watchProgress
                            required property int artworkStamp

                            readonly property bool isShow: _title.kind === "tv"

                            width: _titleTile.width
                            height: _titleTile.implicitHeight

                            function activate() {
                                if (_title.isShow)
                                    root.showRequested(_title.mediaId, _title.title)
                                else
                                    root.detailsRequested(_title.handle)
                            }

                            Ctrl.MediaTile {
                                id: _titleTile

                                shape: Ctrl.MediaTile.Poster
                                width: root.homePosterWidth
                                artSource: {
                                    void _title.artworkStamp
                                    return Metadata.posterUrl(_title.posterPath, 342)
                                }
                                title: _title.title
                                meta: _title.summary
                                progress: _title.watchProgress
                                watched: _title.watched
                                highlighted: _title.ListView.isCurrentItem
                                             && _genreList.activeFocus

                                onClicked: _title.activate()
                            }
                        }
                    }
                }
            }

            Item {
                width: 1
                height: S.AppTheme.spacing24
            }
        }
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: root.settled && !root.hasAnything && !Library.scanning
        iconSource: Library.folders.count === 0 ? S.Icons.folder : S.Icons.noVideo
        title: Library.folders.count === 0
               ? qsTr("Nothing indexed yet")
               : qsTr("Nothing here yet")
        message: Library.folders.count > 0
                 ? qsTr("Play something and it will show up here.")
                 : (System.usesManagedStorageRoots
                    ? qsTr("Makimedia indexes your device storage once it can read your videos.")
                    : qsTr("Add a folder and Makimedia will index what is inside it."))
        actionText: Library.folders.count === 0 && !System.usesManagedStorageRoots
                    ? qsTr("Add folder") : ""
        onActionTriggered: Library.addFolder()
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: root.settled && !root.hasAnything && Library.scanning
        busy: true
        title: qsTr("Nothing indexed yet")
        message: qsTr("The first results appear here as they are found.")
    }

}
