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

    readonly property bool shortWindow: !System.isTelevision
        && Window.height < S.AppTheme.breakpointCompact

    signal detailsRequested(string handle)
    signal showRequested(var mediaId, string title)
    signal drawerRequested()

    readonly property bool acceptsFocus: (model && model.count > 0) || toolbar

    function takeFocus() {
        if (model && model.count > 0) {
            if (_grid.currentIndex < 0)
                _grid.currentIndex = 0
            _grid.forceActiveFocus()
        } else if (toolbar) {
            _sortField.forceActiveFocus()
        }
    }

    property var model: null
    property string emptyTitle: ""
    property string emptyMessage: ""

    property bool toolbar: false
    property string titleText: ""
    property string countText: ""
    property int sortValue: 0
    property int filterValue: 0
    property bool seenAsCheck: false

    readonly property var sortOptions: [
        qsTr("Title A-Z"),
        qsTr("Recently added"),
        qsTr("Year newest"),
        qsTr("Year oldest"),
        qsTr("Rating"),
        qsTr("Shortest"),
        qsTr("Longest")
    ]

    readonly property var filterOptions: [
        qsTr("All"),
        qsTr("Unwatched"),
        qsTr("Watched"),
        qsTr("In progress")
    ]

    property int gridSize: 1
    property var genres: []
    property string genreValue: ""

    readonly property string activeGenre: genres.indexOf(genreValue) >= 0 ? genreValue : ""

    property bool showsSearch: false

    signal searchRequested()
    signal sortChosen(int sort)
    signal filterChosen(int filter)
    signal gridSizeChosen(int size)
    signal genreChosen(string genre)

    function focusAboveGrid() {
        if (_genreChips.visible)
            _genreChips.forceActiveFocus()
        else
            _sortField.forceActiveFocus()
    }

    readonly property real gridWidth: width - 2 * S.AppTheme.spacing16
    readonly property int columnCount:
        S.AppTheme.gridColumns(width, toolbar ? gridSize : 1)
    readonly property real tileWidth:
        (gridWidth - S.AppTheme.scrollBarWidth
         - (columnCount - 1) * S.AppTheme.spacing10) / columnCount

    Column {
        id: _toolbar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: 10
        visible: root.toolbar
        height: visible ? implicitHeight : 0
        spacing: S.AppTheme.spacing10

        Item {
            id: _controls

            readonly property real rowHeight:
                Math.max(_fields.implicitHeight, _sizeSwitch.implicitHeight,
                         _heading.visible ? _heading.implicitHeight : 0)

            width: parent.width
            height: root.shortWindow
                    ? rowHeight
                    : rowHeight + (_genreChips.visible
                                   ? S.AppTheme.spacing10 + _genreChips.height : 0)

            Column {
                id: _heading

                anchors.left: parent.left
                y: (_controls.rowHeight - height) / 2
                visible: !root.compact
                width: visible ? Math.max(_title.implicitWidth, _count.implicitWidth) : 0
                spacing: S.AppTheme.spacing2

                Text {
                    id: _title

                    visible: root.titleText.length > 0
                    height: visible ? implicitHeight : 0
                    text: root.titleText
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs22
                    font.weight: Font.Medium
                }

                Text {
                    id: _count

                    visible: root.countText.length > 0
                    height: visible ? implicitHeight : 0
                    text: root.countText
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs14
                }
            }

            Row {
                id: _fields

                x: _heading.visible ? _heading.width + S.AppTheme.spacing16 : 0
                y: (_controls.rowHeight - height) / 2
                spacing: S.AppTheme.spacing10

                Ctrl.SelectField {
                    id: _sortField

                    label: root.compact ? "" : qsTr("Sort")
                    value: root.sortOptions[root.sortValue] || root.sortOptions[0]
                    onClicked: _sortSheet.open()

                    Keys.onLeftPressed: root.drawerRequested()
                    Keys.onRightPressed: _filterField.forceActiveFocus()
                    Keys.onDownPressed: root.focusBelowFields()
                }

                Ctrl.SelectField {
                    id: _filterField

                    label: root.compact ? "" : qsTr("Show")
                    value: root.filterOptions[root.filterValue] || root.filterOptions[0]
                    onClicked: _filterSheet.open()

                    Keys.onLeftPressed: _sortField.forceActiveFocus()
                    Keys.onRightPressed: _sizeSwitch.forceActiveFocus()
                    Keys.onDownPressed: root.focusBelowFields()
                }
            }

            Ctrl.IconButton {
                id: _search

                anchors.right: parent.right
                y: (_controls.rowHeight - height) / 2
                visible: root.showsSearch
                width: visible ? implicitWidth : 0
                iconSource: S.Icons.search
                accessibleName: qsTr("Search")

                onClicked: root.searchRequested()
            }

            Ctrl.GridSizeSwitch {
                id: _sizeSwitch

                anchors.right: _search.left
                anchors.rightMargin: _search.visible ? S.AppTheme.spacing8 : 0
                y: (_controls.rowHeight - height) / 2
                size: root.gridSize

                onChosen: (size) => root.gridSizeChosen(size)
                onLeftEdgeReached: _filterField.forceActiveFocus()
                onDownRequested: root.focusBelowFields()
            }

            Ctrl.Rail {
                id: _genreChips

                x: root.shortWindow
                   ? _fields.x + _fields.width + S.AppTheme.spacing10
                   : 0
                y: root.shortWindow
                   ? (_controls.rowHeight - height) / 2
                   : _controls.rowHeight + S.AppTheme.spacing10
                width: root.shortWindow
                       ? Math.max(0, _sizeSwitch.x - S.AppTheme.spacing10 - x)
                       : parent.width
                height: visible ? S.AppTheme.controlHeightSmall + S.AppTheme.spacing4 : 0
                visible: root.genres.length > 0
                spacing: S.AppTheme.spacing8
                model: [""].concat(root.genres)

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const at = model.indexOf(root.activeGenre)
                        currentIndex = at >= 0 ? at : 0
                    }
                }

                onLeftEdgeReached: root.drawerRequested()
                onActivated: (index) => root.genreChosen(_genreChips.model[index])
                Keys.onUpPressed: _sortField.forceActiveFocus()
                Keys.onDownPressed: root.takeFocus()

                delegate: Ctrl.PillChip {
                    id: _chip

                    required property int index
                    required property string modelData

                    y: S.AppTheme.spacing2
                    text: _chip.modelData.length > 0 ? _chip.modelData : qsTr("All")
                    selected: root.activeGenre === _chip.modelData
                    highlighted: _genreChips.activeFocus && _chip.ListView.isCurrentItem

                    onClicked: root.genreChosen(_chip.modelData)
                }
            }
        }
    }

    function focusBelowFields() {
        if (_genreChips.visible)
            _genreChips.forceActiveFocus()
        else
            takeFocus()
    }

    GridView {
        id: _grid

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _toolbar.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: S.AppTheme.spacing12
        clip: true
        visible: root.model && root.model.count > 0
        cellWidth: root.tileWidth + S.AppTheme.spacing10
        cellHeight: root.tileWidth * S.AppTheme.posterAspectRatio
                    + S.AppTheme.spacing8 + 34 + S.AppTheme.spacing12
                    + S.AppTheme.focusHeadroom
        model: root.model
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

        Keys.onUpPressed: (event) => {
            if (root.toolbar && _grid.currentIndex < root.columnCount) {
                root.focusAboveGrid()
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

            required property var mediaId
            required property string kind
            required property string title
            required property string posterPath
            required property string summary
            required property string handle
            required property bool watched
            required property real watchProgress
            required property int artworkStamp

            width: _grid.cellWidth
            height: _grid.cellHeight

            function activate() {
                if (_cell.kind === "tv")
                    root.showRequested(_cell.mediaId, _cell.title)
                else
                    root.detailsRequested(_cell.handle)
            }

            Ctrl.MediaTile {
                anchors.horizontalCenter: parent.horizontalCenter
                width: root.tileWidth
                shape: Ctrl.MediaTile.Poster
                highlighted: _cell.GridView.isCurrentItem && _grid.activeFocus
                artSource: {
                    void _cell.artworkStamp
                    return Metadata.posterUrl(_cell.posterPath, 342)
                }
                title: _cell.title
                meta: _cell.summary
                progress: _cell.watchProgress
                badge: _cell.watched ? Ctrl.MediaTile.Positive : Ctrl.MediaTile.None
                badgeText: _cell.watched && !root.seenAsCheck ? qsTr("Seen") : ""
                badgeIcon: _cell.watched && root.seenAsCheck ? S.Icons.check : ""
                badgeIconBackground: "white"
                badgeIconForeground: "black"

                onClicked: _cell.activate()
            }
        }
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: !root.model || root.model.count === 0
        iconSource: S.Icons.noVideo
        title: root.toolbar && (root.filterValue !== 0 || root.activeGenre.length > 0)
               ? qsTr("Nothing here with this filter")
               : root.emptyTitle
        message: root.toolbar && (root.filterValue !== 0 || root.activeGenre.length > 0)
                 ? qsTr("Choose another genre, or another option under Show.")
                 : root.emptyMessage
    }

    Ctrl.ChoiceSheet {
        id: _sortSheet

        title: qsTr("Sort by")
        options: root.sortOptions
        selectedIndex: root.sortValue
        onChosen: (index) => root.sortChosen(index)
    }

    Ctrl.ChoiceSheet {
        id: _filterSheet

        title: qsTr("Show")
        options: root.filterOptions
        selectedIndex: root.filterValue
        onChosen: (index) => root.filterChosen(index)
    }
}
