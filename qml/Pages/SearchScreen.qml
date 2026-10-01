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
    signal showRequested(var mediaId, string title)
    signal collectionRequested(var collectionId)
    signal drawerRequested()

    readonly property bool hasQuery: Library.searchQuery.trim().length > 0

    readonly property bool acceptsFocus: true

    property int kind: 0

    readonly property int movieCount: Library.searchMovies.count
    readonly property int showCount: Library.searchShows.count
    readonly property int collectionCount: Library.searchCollections.count
    readonly property int episodeCount: Library.searchEpisodes.count
    readonly property int fileCount: Library.searchResults.count

    readonly property bool showsMovies: (kind === 0 || kind === 1) && movieCount > 0
    readonly property bool showsShows: (kind === 0 || kind === 2) && showCount > 0
    readonly property bool showsCollections: (kind === 0 || kind === 3) && collectionCount > 0
    readonly property bool showsEpisodes: (kind === 0 || kind === 4) && episodeCount > 0
    readonly property bool showsFiles: (kind === 0 || kind === 5) && fileCount > 0

    readonly property bool foundNothing: hasQuery && Library.searchCount === 0

    function countText(count, capped) {
        return capped ? count + "+" : String(count)
    }

    readonly property string needle: Library.searchQuery.trim().toLowerCase()

    function plain(text) {
        return (text || "").toLowerCase()
                           .normalize("NFD")
                           .replace(/[̀-ͯ]/g, "")
    }

    function carriesNeedle(text) {
        return root.needle.length > 0
               && root.plain(text).indexOf(root.plain(root.needle)) >= 0
    }

    function whyMatched(title, genres) {
        if (root.needle.length === 0 || root.carriesNeedle(title))
            return ""

        const list = (genres || "").split(", ")
        for (let i = 0; i < list.length; ++i) {
            if (root.plain(list[i]).indexOf(root.plain(root.needle)) === 0)
                return list[i]
        }
        return qsTr("in the filename")
    }

    function metaWith(first, why) {
        if (why.length === 0)
            return first
        return first.length > 0 ? first + " · " + why : why
    }

    readonly property var chips: {
        const kinds = []
        if (movieCount > 0)
            kinds.push({ kind: 1, label: qsTr("Movies"),
                         text: countText(movieCount, Library.searchMoviesCapped) })
        if (showCount > 0)
            kinds.push({ kind: 2, label: qsTr("TV shows"),
                         text: countText(showCount, Library.searchShowsCapped) })
        if (collectionCount > 0)
            kinds.push({ kind: 3, label: qsTr("Collections"),
                         text: String(collectionCount) })
        if (episodeCount > 0)
            kinds.push({ kind: 4, label: qsTr("Episodes"),
                         text: countText(episodeCount, Library.searchEpisodesCapped) })
        if (fileCount > 0)
            kinds.push({ kind: 5, label: qsTr("Unmatched files"),
                         text: countText(fileCount, Library.searchFilesCapped) })

        if (kinds.length < 2)
            return []
        return [{ kind: 0, label: qsTr("All"),
                  text: String(Library.searchCount) }].concat(kinds)
    }

    function takeFocus() {
        _bar.forceFocus()
    }

    function searchNow() {
        _pendingSearch.stop()
        if (_bar.text.trim().length >= 2)
            Library.search(_bar.text)
        else
            Library.clearSearch()
    }

    function keepQuery() {
        if (root.hasQuery && Library.searchCount > 0)
            AppSettings.noteSearch(Library.searchQuery)
    }

    function searchFor(text) {
        _bar.text = text
        root.searchNow()
        root.kind = 0
    }

    function sectionsInOrder() {
        const sections = []
        if (showsMovies)
            sections.push(_movieRow)
        if (showsShows)
            sections.push(_showRow)
        if (showsCollections)
            sections.push(_collectionRow)
        if (showsEpisodes)
            sections.push(_episodes)
        if (showsFiles)
            sections.push(_files)
        return sections
    }

    function focusFirstSection() {
        const sections = root.sectionsInOrder()
        if (sections.length > 0) {
            sections[0].forceActiveFocus()
            root.showSection(sections[0])
        }
    }

    function showSection(section) {
        if (!section)
            return

        const top = section.mapToItem(_column, 0, 0).y
        const bottom = top + section.height
        const margin = S.AppTheme.spacing24
        const maxY = Math.max(0, _scroll.contentHeight - _scroll.height)

        if (top - margin < _scroll.contentY)
            _scroll.contentY = Math.max(0, top - margin)
        else if (bottom + margin > _scroll.contentY + _scroll.height)
            _scroll.contentY = Math.min(maxY, bottom + margin - _scroll.height)
    }

    function stepSection(from, delta) {
        const sections = root.sectionsInOrder()
        const at = sections.indexOf(from)
        if (at < 0)
            return

        const next = at + delta
        if (next < 0) {
            if (_chips.visible)
                _chips.forceActiveFocus()
            else
                _bar.forceFocus()
            _scroll.contentY = 0
            return
        }
        if (next < sections.length) {
            sections[next].forceActiveFocus()
            root.showSection(sections[next])
        }
    }

    Timer {
        id: _pendingSearch

        interval: 250
        onTriggered: {
            root.searchNow()
            _keepQuery.restart()
        }
    }

    Timer {
        id: _keepQuery

        interval: 1500
        onTriggered: root.keepQuery()
    }

    Component.onCompleted: {
        _bar.text = Library.searchQuery
        _bar.forceFocus()
    }

    Ctrl.SearchBar {
        id: _bar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing16
        placeholder: qsTr("Search your library")

        onAccepted: _pendingSearch.restart()
        onCleared: {
            _pendingSearch.stop()
            _keepQuery.stop()
            Library.clearSearch()
            root.kind = 0
        }

        onDownRequested: {
            root.searchNow()
            if (Library.searchCount > 0)
                root.focusFirstSection()
            else if (_recent.visible)
                _recent.forceActiveFocus()
            else
                root.drawerRequested()
        }
        onLeftRequested: root.drawerRequested()

        onTypingRequested: _keyboard.open(qsTr("Search your library"), _bar.text)
    }

    Ctrl.Rail {
        id: _chips

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _bar.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: visible ? S.AppTheme.spacing4 : 0
        height: visible ? S.AppTheme.controlHeightSmall + S.AppTheme.spacing4 : 0
        visible: root.chips.length > 0
        spacing: S.AppTheme.spacing8
        model: root.chips

        onLeftEdgeReached: root.drawerRequested()
        onActivated: (index) => root.kind = _chips.model[index].kind
        Keys.onUpPressed: _bar.forceFocus()
        Keys.onDownPressed: root.focusFirstSection()

        delegate: Ctrl.PillChip {
            id: _chip

            required property int index
            required property var modelData

            y: S.AppTheme.spacing2
            text: _chip.modelData.label + "  " + _chip.modelData.text
            selected: root.kind === _chip.modelData.kind
            highlighted: _chips.activeFocus && _chip.ListView.isCurrentItem

            onClicked: root.kind = _chip.modelData.kind
        }
    }

    readonly property int posterWidth: S.AppTheme.posterTileWidth
    readonly property real posterRowHeight:
        posterWidth * S.AppTheme.posterAspectRatio
        + S.AppTheme.tileLabelHeight + S.AppTheme.focusHeadroom
    readonly property int rowInset: S.AppTheme.focusHeadroom
                                    + (S.AppTheme.remoteNavigation ? S.AppTheme.spacing24 : 0)

    Flickable {
        id: _scroll

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _chips.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: S.AppTheme.spacing4
        clip: true
        visible: root.hasQuery && Library.searchCount > 0
        contentHeight: _column.height + S.AppTheme.spacing24
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Column {
            id: _column

            x: S.AppTheme.spacing16
            width: _scroll.width - 2 * S.AppTheme.spacing16
                   - S.AppTheme.scrollBarWidth
            spacing: S.AppTheme.spacing6

            Ctrl.RowHeader {
                width: parent.width
                visible: root.showsMovies
                title: qsTr("Movies")
                countText: root.countText(root.movieCount, Library.searchMoviesCapped)
            }

            Ctrl.Rail {
                id: _movieRow

                width: parent.width
                height: visible ? root.posterRowHeight : 0
                visible: root.showsMovies
                spacing: S.AppTheme.spacing12
                model: root.showsMovies ? Library.searchMovies : 0
                cacheBuffer: 0
                reuseItems: true
                currentIndex: 0
                leadInset: root.rowInset
                tailInset: S.AppTheme.focusHeadroom

                onLeftEdgeReached: root.drawerRequested()
                onActivated: {
                    if (_movieRow.currentItem)
                        _movieRow.currentItem.activate()
                }
                Keys.onUpPressed: root.stepSection(_movieRow, -1)
                Keys.onDownPressed: root.stepSection(_movieRow, 1)

                delegate: Item {
                    id: _movie

                    required property var mediaId
                    required property string title
                    required property int year
                    required property string genres
                    required property string posterPath
                    required property string handle
                    required property bool watched
                    required property real watchProgress
                    required property int artworkStamp

                    width: _movieTile.width
                    height: _movieTile.implicitHeight

                    function activate() {
                        root.detailsRequested(_movie.handle)
                    }

                    Ctrl.StackedShowTile {
                        id: _movieTile

                        width: root.posterWidth
                        stacked: false
                        artSource: {
                            void _movie.artworkStamp
                            return Metadata.posterUrl(_movie.posterPath, 342)
                        }
                        title: _movie.title
                        meta: root.metaWith(_movie.year > 0 ? String(_movie.year) : "",
                                            root.whyMatched(_movie.title, _movie.genres))
                        watched: _movie.watched
                        progress: _movie.watchProgress
                        highlighted: _movie.ListView.isCurrentItem && _movieRow.activeFocus

                        onClicked: _movie.activate()
                    }
                }
            }

            Ctrl.RowHeader {
                width: parent.width
                visible: root.showsShows
                title: qsTr("TV shows")
                countText: root.countText(root.showCount, Library.searchShowsCapped)
            }

            Ctrl.Rail {
                id: _showRow

                width: parent.width
                height: visible ? root.posterRowHeight : 0
                visible: root.showsShows
                spacing: S.AppTheme.spacing12
                model: root.showsShows ? Library.searchShows : 0
                cacheBuffer: 0
                reuseItems: true
                currentIndex: 0
                leadInset: root.rowInset
                tailInset: S.AppTheme.focusHeadroom

                onLeftEdgeReached: root.drawerRequested()
                onActivated: {
                    if (_showRow.currentItem)
                        _showRow.currentItem.activate()
                }
                Keys.onUpPressed: root.stepSection(_showRow, -1)
                Keys.onDownPressed: root.stepSection(_showRow, 1)

                delegate: Item {
                    id: _show

                    required property var mediaId
                    required property string title
                    required property int year
                    required property string genres
                    required property string posterPath
                    required property int seasonCount
                    required property int fileCount
                    required property bool watched
                    required property real watchProgress
                    required property int artworkStamp

                    width: _showTile.width
                    height: _showTile.implicitHeight

                    function activate() {
                        root.showRequested(_show.mediaId, _show.title)
                    }

                    Ctrl.StackedShowTile {
                        id: _showTile

                        width: root.posterWidth
                        stacked: true
                        artSource: {
                            void _show.artworkStamp
                            return Metadata.posterUrl(_show.posterPath, 342)
                        }
                        title: _show.title
                        meta: root.metaWith(qsTr("%n episode(s)", "", _show.fileCount),
                                            root.whyMatched(_show.title, _show.genres))
                        watched: _show.watched
                        progress: _show.watchProgress
                        highlighted: _show.ListView.isCurrentItem && _showRow.activeFocus

                        onClicked: _show.activate()
                    }
                }
            }

            Ctrl.RowHeader {
                width: parent.width
                visible: root.showsCollections
                title: qsTr("Collections")
                countText: String(root.collectionCount)
            }

            Ctrl.Rail {
                id: _collectionRow

                readonly property real tileWidth: Math.round(root.posterWidth * 1.15)
                readonly property real gapRatio: 0.3
                readonly property int fanGap: Math.round(tileWidth * gapRatio)

                width: parent.width
                height: visible ? _collectionProbe.implicitHeight
                                  + S.AppTheme.focusHeadroom : 0
                visible: root.showsCollections
                spacing: fanGap
                model: root.showsCollections ? Library.searchCollections : 0
                cacheBuffer: 0
                reuseItems: true
                currentIndex: 0
                leadInset: root.rowInset
                tailInset: Math.max(S.AppTheme.focusHeadroom, fanGap)

                onLeftEdgeReached: root.drawerRequested()
                onActivated: {
                    if (_collectionRow.currentItem)
                        _collectionRow.currentItem.activate()
                }
                Keys.onUpPressed: root.stepSection(_collectionRow, -1)
                Keys.onDownPressed: root.stepSection(_collectionRow, 1)

                delegate: Item {
                    id: _collection

                    required property var collectionId
                    required property string name
                    required property string posterPath
                    required property var behindPosters
                    required property string summary
                    required property int watchedCount
                    required property int totalCount
                    required property bool completed
                    required property bool custom
                    required property real watchProgress
                    required property int artworkStamp

                    width: _collectionTile.width
                    height: _collectionTile.implicitHeight
                    z: _collectionTile.fanned ? 2 : 0

                    function activate() {
                        root.collectionRequested(_collection.collectionId)
                    }

                    Ctrl.CollectionTile {
                        id: _collectionTile

                        width: _collectionRow.tileWidth
                        highlighted: _collection.ListView.isCurrentItem
                                     && _collectionRow.activeFocus
                        artSource: {
                            void _collection.artworkStamp
                            const path = _collection.posterPath.length > 0
                                         ? _collection.posterPath
                                         : (_collection.behindPosters.length > 0
                                            ? _collection.behindPosters[0] : "")
                            return Metadata.posterUrl(path, 342)
                        }
                        behindSources: {
                            void _collection.artworkStamp
                            const urls = []
                            for (let i = 0; i < _collection.behindPosters.length; ++i)
                                urls.push(Metadata.posterUrl(_collection.behindPosters[i], 342))
                            return urls
                        }
                        title: _collection.name
                        meta: _collection.summary
                        watchedCount: _collection.watchedCount
                        totalCount: _collection.totalCount
                        completed: _collection.completed
                        custom: _collection.custom
                        progress: _collection.watchProgress
                        menuEnabled: false

                        onClicked: _collection.activate()
                    }
                }
            }

            Ctrl.CollectionTile {
                id: _collectionProbe

                visible: false
                width: _collectionRow.tileWidth
                title: "Probe"
                meta: "Probe"
            }

            Ctrl.SectionLabel {
                visible: root.showsEpisodes
                text: root.countText(root.episodeCount, Library.searchEpisodesCapped)
                      + " " + (root.episodeCount === 1 ? qsTr("episode") : qsTr("episodes"))
            }

            Column {
                id: _episodes

                width: parent.width
                visible: root.showsEpisodes

                property int currentIndex: 0

                onCurrentIndexChanged:
                    root.showSection(_episodeRepeater.itemAt(_episodes.currentIndex))

                Keys.onUpPressed: {
                    if (_episodes.currentIndex > 0)
                        _episodes.currentIndex = _episodes.currentIndex - 1
                    else
                        root.stepSection(_episodes, -1)
                }
                Keys.onDownPressed: {
                    if (_episodes.currentIndex < Library.searchEpisodes.count - 1)
                        _episodes.currentIndex = _episodes.currentIndex + 1
                    else
                        root.stepSection(_episodes, 1)
                }
                Keys.onLeftPressed: root.drawerRequested()
                Keys.onPressed: (event) => {
                    if (S.AppTheme.isActivateKey(event.key)) {
                        const at = _episodeRepeater.itemAt(_episodes.currentIndex)
                        if (at)
                            at.activate()
                        event.accepted = true
                    }
                }

                Repeater {
                    id: _episodeRepeater

                    model: root.showsEpisodes ? Library.searchEpisodes : 0

                    delegate: Column {
                        id: _episode

                        required property int index
                        required property string handle
                        required property string displayName
                        required property string titleText
                        required property string episodeTitle
                        required property string episodeLabel
                        required property string durationText
                        required property string stillPath
                        required property string thumbnail
                        required property bool watched
                        required property real watchProgress
                        required property int artworkStamp

                        width: _episodes.width

                        function activate() {
                            root.detailsRequested(_episode.handle)
                        }

                        Ctrl.ListRow {
                            width: parent.width
                            highlighted: _episodes.activeFocus
                                         && _episodes.currentIndex === _episode.index
                            leading: Ctrl.ListRow.Thumbnail
                            thumbnailSource: {
                                void _episode.artworkStamp
                                return _episode.stillPath.length > 0
                                       ? Metadata.stillUrl(_episode.stillPath, 300)
                                       : _episode.thumbnail
                            }
                            title: _episode.episodeTitle.length > 0
                                   ? _episode.episodeTitle
                                   : _episode.displayName
                            subtitle: [_episode.titleText, _episode.episodeLabel,
                                       _episode.durationText,
                                       root.carriesNeedle(_episode.episodeTitle)
                                       ? "" : qsTr("in the filename")]
                                      .filter(function (part) { return part.length > 0 })
                                      .join("  ·  ")
                            trailingText: _episode.watched ? qsTr("Seen") : ""
                            progress: _episode.watchProgress

                            onClicked: _episode.activate()
                        }

                        Ctrl.Divider {
                            width: parent.width
                            inset: 88
                        }
                    }
                }
            }

            Ctrl.SectionLabel {
                visible: root.showsFiles
                text: qsTr("Files with no match")
            }

            Column {
                id: _files

                width: parent.width
                visible: root.showsFiles

                property int currentIndex: 0

                onCurrentIndexChanged:
                    root.showSection(_fileRepeater.itemAt(_files.currentIndex))

                Keys.onUpPressed: {
                    if (_files.currentIndex > 0)
                        _files.currentIndex = _files.currentIndex - 1
                    else
                        root.stepSection(_files, -1)
                }
                Keys.onDownPressed: {
                    if (_files.currentIndex < Library.searchResults.count - 1)
                        _files.currentIndex = _files.currentIndex + 1
                }
                Keys.onLeftPressed: root.drawerRequested()
                Keys.onPressed: (event) => {
                    if (S.AppTheme.isActivateKey(event.key)) {
                        const at = _fileRepeater.itemAt(_files.currentIndex)
                        if (at)
                            at.activate()
                        event.accepted = true
                    }
                }

                Repeater {
                    id: _fileRepeater

                    model: root.showsFiles ? Library.searchResults : 0

                    delegate: Column {
                        id: _hit

                        required property int index
                        required property string handle
                        required property string displayName
                        required property string locationText
                        required property string sizeText
                        required property string thumbnail

                        width: _files.width

                        function activate() {
                            root.detailsRequested(_hit.handle)
                        }

                        Ctrl.ListRow {
                            width: parent.width
                            highlighted: _files.activeFocus
                                         && _files.currentIndex === _hit.index
                            leading: Ctrl.ListRow.Thumbnail
                            thumbnailSource: _hit.thumbnail
                            title: _hit.displayName
                            monoTitle: true
                            subtitle: [_hit.locationText, _hit.sizeText]
                                      .filter(function (part) { return part.length > 0 })
                                      .join("  ·  ")
                            trailingText: qsTr("Fix match")

                            onClicked: _hit.activate()
                        }

                        Ctrl.Divider {
                            width: parent.width
                            inset: 88
                        }
                    }
                }
            }
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: _bar.bottom
        anchors.topMargin: S.AppTheme.spacing32
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 440)
        visible: !root.hasQuery
        spacing: S.AppTheme.spacing16

        Ctrl.EmptyState {
            width: parent.width
            iconSource: S.Icons.search
            title: qsTr("Search your library")
            message: qsTr("%n title(s) matched, and every filename.", "",
                          Library.movies.count + Library.shows.count)
        }

        Ctrl.Rail {
            id: _recent

            width: parent.width
            height: visible ? S.AppTheme.controlHeightSmall + S.AppTheme.spacing4 : 0
            visible: AppSettings.recentSearches.length > 0
            spacing: S.AppTheme.spacing8
            model: AppSettings.recentSearches

            onLeftEdgeReached: root.drawerRequested()
            onActivated: (index) => root.searchFor(AppSettings.recentSearches[index])
            Keys.onUpPressed: _bar.forceFocus()

            delegate: Ctrl.PillChip {
                id: _recentChip

                required property int index
                required property string modelData

                y: S.AppTheme.spacing2
                text: _recentChip.modelData
                highlighted: _recent.activeFocus
                             && _recent.currentIndex === _recentChip.index

                onClicked: root.searchFor(_recentChip.modelData)
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: AppSettings.recentSearches.length > 0
                     && !S.AppTheme.remoteNavigation
            text: qsTr("Forget these")
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs12

            MouseArea {
                anchors.fill: parent
                anchors.margins: -S.AppTheme.spacing8
                onClicked: AppSettings.forgetSearches()
            }
        }
    }

    Ctrl.RemoteKeyboard {
        id: _keyboard

        z: 40

        onAccepted: (text) => {
            _bar.text = text
            root.searchNow()
            root.keepQuery()
            Qt.callLater(_bar.forceFocus)
        }
        onCancelled: Qt.callLater(_bar.forceFocus)
    }

    Ctrl.EmptyState {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: _chips.bottom
        anchors.topMargin: S.AppTheme.spacing32
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: root.foundNothing
        iconSource: S.Icons.noVideo
        title: qsTr("Nothing matches \"%1\"").arg(Library.searchQuery)
        message: qsTr("No film, show, episode, collection or file in your "
                      + "library carries those words.")
    }
}
