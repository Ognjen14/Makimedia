pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal detailsRequested(string handle)
    signal drawerRequested()

    readonly property bool acceptsFocus: Library.files.count > 0

    function takeFocus() {
        if (_grid.currentIndex < 0)
            _grid.currentIndex = 0
        _grid.forceActiveFocus()
    }

    readonly property real gridWidth: width - 2 * S.AppTheme.spacing16
    readonly property int columnCount:
        S.AppTheme.gridColumns(width, 1)
    readonly property real tileWidth:
        (gridWidth - S.AppTheme.scrollBarWidth
         - (columnCount - 1) * S.AppTheme.spacing10) / columnCount

    Row {
        id: _chips

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.topMargin: S.AppTheme.spacing4
        spacing: S.AppTheme.spacing6

        Ctrl.Chip {
            text: _sortFilter.sortOptions[Library.sortMode]
            selected: Library.sortMode !== 0
            onClicked: _sortFilter.openOptions()
        }

        Ctrl.Chip {
            visible: Library.filterMode !== 0
            text: _sortFilter.filterOptions[Library.filterMode]
            selected: true
            onClicked: _sortFilter.openOptions()
        }

        Ctrl.Chip {
            text: qsTr("%n file(s)", "", Library.files.count)
            enabled: false
        }

        Ctrl.Chip {
            visible: !System.isTelevision
                     && Library.filterMode === Library.FilterUnmatched
                     && Library.files.count > 0
            text: qsTr("Hide all unmatched")
            onClicked: {
                _hideAll.hideable = Library.hideableUnmatchedCount()
                _hideAll.open()
            }
        }
    }

    Ctrl.AppDialog {
        id: _hideAll

        property int hideable: 0

        z: 60
        title: qsTr("Hide all unmatched?")
        message: hideable > 0
                 ? qsTr("%n file(s) with no title behind them leave the library. The files stay on disk, and Settings can put them back.", "", hideable)
                 : qsTr("Every unmatched file here is waiting in Identify shows, so there is nothing to hide.")
        dismissText: hideable > 0 ? qsTr("Cancel") : ""
        acceptText: hideable > 0 ? qsTr("Hide them") : qsTr("OK")

        onAccepted: {
            if (hideable > 0)
                Library.hideAllUnmatched()
        }

        Text {
            width: parent.width
            visible: _hideAll.hideable > 0
            text: qsTr("Suggested matches and files waiting in Identify shows stay.")
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs12
            wrapMode: Text.Wrap
        }
    }

    GridView {
        id: _grid

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _chips.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: S.AppTheme.spacing12
        clip: true
        visible: Library.files.count > 0
        cellWidth: root.tileWidth + S.AppTheme.spacing10
        cellHeight: root.tileWidth * S.AppTheme.posterAspectRatio
                    + S.AppTheme.spacing8 + 34 + S.AppTheme.spacing12
                    + S.AppTheme.focusHeadroom
        model: Library.files
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: cellHeight
        reuseItems: true

        currentIndex: 0

        Keys.onLeftPressed: (event) => {
            if (_grid.currentIndex % root.columnCount === 0) {
                root.drawerRequested()
                event.accepted = true
            } else {
                event.accepted = false
            }
        }

        Keys.onPressed: (event) => {
            if (S.AppTheme.isActivateKey(event.key)) {
                if (_grid.currentItem)
                    _grid.currentItem.activate()
                event.accepted = true
            }
        }

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        delegate: Item {
            id: _cell

            required property string handle
            required property string displayName
            required property bool watched
            required property bool missing
            required property real watchProgress
            required property string thumbnail
            required property string posterPath
            required property bool isUnmatched
            required property int matchedYear
            required property bool matchSuggested
            required property string episodeLabel
            required property string titleText
            required property int artworkStamp

            width: _grid.cellWidth
            height: _grid.cellHeight

            function activate() {
                root.detailsRequested(_cell.handle)
            }

            Ctrl.MediaTile {
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.tileWidth
                shape: Ctrl.MediaTile.Poster
                highlighted: _cell.GridView.isCurrentItem && _grid.activeFocus
                unmatched: _cell.isUnmatched
                artSource: {
                    void _cell.artworkStamp
                    return _cell.posterPath.length > 0
                           ? Metadata.posterUrl(_cell.posterPath, 342)
                           : _cell.thumbnail
                }
                title: _cell.titleText
                meta: {
                    if (_cell.missing)
                        return qsTr("Missing")
                    if (_cell.isUnmatched)
                        return qsTr("Unmatched")
                    if (_cell.matchSuggested)
                        return qsTr("Suggested")
                    if (_cell.episodeLabel.length > 0)
                        return _cell.episodeLabel
                    return _cell.matchedYear > 0 ? String(_cell.matchedYear) : ""
                }
                progress: _cell.watchProgress
                badge: _cell.missing
                       ? Ctrl.MediaTile.Negative
                       : (_cell.watched ? Ctrl.MediaTile.Positive : Ctrl.MediaTile.None)
                badgeText: _cell.missing
                           ? qsTr("Missing")
                           : (_cell.watched ? qsTr("Seen") : "")

                onClicked: _cell.activate()
            }
        }
    }

    Ctrl.SortFilterSheet {
        id: _sortFilter

        z: 50
        sortMode: Library.sortMode
        filterMode: Library.filterMode

        onApplied: (sortMode, filterMode) => {
            Library.sortMode = sortMode
            Library.filterMode = filterMode
        }

        onCleared: Library.resetOptions()
    }

    Ctrl.EmptyState {
        readonly property bool filteredOut:
            Library.filterMode !== 0 && Library.fileCount > 0

        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: Library.files.count === 0 && !Library.scanning
        iconSource: filteredOut || Library.folders.count > 0
                    ? S.Icons.noVideo : S.Icons.folder
        title: filteredOut
               ? qsTr("Nothing matches")
               : qsTr("Nothing indexed yet")
        message: {
            if (filteredOut)
                return qsTr("No file in the library is %1.")
                       .arg(_sortFilter.filterOptions[Library.filterMode].toLowerCase())
            if (Library.folders.count === 0)
                return qsTr("Add a folder and Makimedia will index what is inside it.")
            return qsTr("That folder had no video files in it.")
        }
        actionText: {
            if (filteredOut)
                return qsTr("Show everything")
            return Library.folders.count === 0 && !System.usesManagedStorageRoots
                   ? qsTr("Add folder") : ""
        }
        onActionTriggered: {
            if (filteredOut)
                Library.resetOptions()
            else
                Library.addFolder()
        }
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: Library.files.count === 0 && Library.scanning
        busy: true
        title: qsTr("Nothing indexed yet")
        message: qsTr("The first results appear here as they are found.")
    }
}
