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

    property bool showsTitle: false

    property bool showsSearch: false

    signal searchRequested()
    signal drawerRequested()
    signal collectionRequested(var collectionId)
    signal menuRequested(var collectionId, string name, bool custom)

    readonly property var model: Library.collectionGrid
    readonly property bool acceptsFocus: true

    readonly property var filterOptions: [
        qsTr("All"),
        qsTr("In progress"),
        qsTr("Completed"),
        qsTr("Not started")
    ]

    function takeFocus() {
        if (model.count > 0) {
            if (_grid.currentIndex < 0)
                _grid.currentIndex = 0
            _grid.forceActiveFocus()
        } else {
            _chips.forceActiveFocus()
        }
    }

    readonly property real gridWidth: width - 2 * S.AppTheme.spacing16
    readonly property int columnCount:
        S.AppTheme.gridColumns(System.isTelevision ? Math.round(width / 1.3) : width,
                               Library.collectionGridSize)
    readonly property real gapRatio: 0.3
    readonly property real cellWidth:
        Math.floor((gridWidth - S.AppTheme.scrollBarWidth) / columnCount)
    readonly property real tileWidth: Math.floor(cellWidth / (1 + gapRatio))

    Column {
        id: _toolbar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: 10
        spacing: S.AppTheme.spacing10

        Item {
            width: parent.width
            height: Math.max(S.AppTheme.controlHeightSmall + S.AppTheme.spacing4,
                             _sizeSwitch.implicitHeight,
                             _heading.visible ? _heading.implicitHeight : 0)

            Column {
                id: _heading

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.compact
                width: visible ? Math.max(_title.implicitWidth, _count.implicitWidth) : 0
                spacing: S.AppTheme.spacing2

                Text {
                    id: _title

                    visible: root.showsTitle
                    height: visible ? implicitHeight : 0
                    text: qsTr("Collections")
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs22
                    font.weight: Font.Medium
                }

                Text {
                    id: _count

                    text: {
                        const all = Library.collections.count
                        const noun = all === 1 ? qsTr("collection") : qsTr("collections")
                        return Library.collectionGrid.count === all
                               ? all + " " + noun
                               : qsTr("%1 of %2 %3").arg(Library.collectionGrid.count).arg(all).arg(noun)
                    }
                    color: S.AppTheme.textSecondary
                    font.pixelSize: S.AppTheme.fs14
                }
            }

            Ctrl.Rail {
                id: _chips

                anchors.left: _heading.visible ? _heading.right : parent.left
                anchors.leftMargin: _heading.visible ? S.AppTheme.spacing16 : 0
                anchors.right: _sizeSwitch.left
                anchors.rightMargin: S.AppTheme.spacing10
                anchors.verticalCenter: parent.verticalCenter
                height: S.AppTheme.controlHeightSmall + S.AppTheme.spacing4
                spacing: S.AppTheme.spacing8
                model: root.filterOptions

                onActiveFocusChanged: {
                    if (activeFocus)
                        currentIndex = Library.collectionFilter
                }

                onLeftEdgeReached: root.drawerRequested()
                onRightEdgeReached: _sizeSwitch.forceActiveFocus()
                onActivated: (index) => Library.collectionFilter = index
                Keys.onDownPressed: {
                    if (root.model.count > 0)
                        root.takeFocus()
                }

                delegate: Ctrl.PillChip {
                    id: _chip

                    required property int index
                    required property string modelData

                    y: S.AppTheme.spacing2
                    text: _chip.modelData
                    selected: Library.collectionFilter === _chip.index
                    highlighted: _chips.activeFocus && _chip.ListView.isCurrentItem

                    onClicked: Library.collectionFilter = _chip.index
                }
            }

            Ctrl.IconButton {
                id: _search

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
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
                anchors.verticalCenter: parent.verticalCenter
                size: Library.collectionGridSize

                onChosen: (size) => Library.collectionGridSize = size
                onLeftEdgeReached: _chips.forceActiveFocus()
                onDownRequested: {
                    if (root.model.count > 0)
                        root.takeFocus()
                }
            }
        }
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
        visible: root.model.count > 0
        cellWidth: root.cellWidth
        cellHeight: _probe.implicitHeight + S.AppTheme.spacing24
        topMargin: S.AppTheme.spacing8
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
            if (_grid.currentIndex < root.columnCount) {
                _chips.forceActiveFocus()
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

            width: _grid.cellWidth
            height: _grid.cellHeight
            z: _tile.fanned ? 2 : 0

            function activate() {
                root.collectionRequested(_cell.collectionId)
            }

            Ctrl.CollectionTile {
                id: _tile

                x: Math.round((_cell.width - width) / 2)
                width: root.tileWidth
                highlighted: _cell.GridView.isCurrentItem && _grid.activeFocus
                artSource: {
                    void _cell.artworkStamp
                    const path = _cell.posterPath.length > 0
                                 ? _cell.posterPath
                                 : (_cell.behindPosters.length > 0 ? _cell.behindPosters[0] : "")
                    return Metadata.posterUrl(path, 342)
                }
                behindSources: {
                    void _cell.artworkStamp
                    const urls = []
                    for (let i = 0; i < _cell.behindPosters.length; ++i)
                        urls.push(Metadata.posterUrl(_cell.behindPosters[i], 342))
                    return urls
                }
                title: _cell.name
                meta: _cell.summary
                watchedCount: _cell.watchedCount
                totalCount: _cell.totalCount
                completed: _cell.completed
                custom: _cell.custom
                progress: _cell.watchProgress

                onClicked: root.collectionRequested(_cell.collectionId)

                menuEnabled: !System.isTelevision

                onMenuRequested: root.menuRequested(_cell.collectionId, _cell.name, _cell.custom)
            }
        }
    }

    Ctrl.CollectionTile {
        id: _probe

        visible: false
        width: root.tileWidth
        title: "Probe"
        meta: "Probe"
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: root.model.count === 0
        iconSource: S.Icons.collections
        title: Library.collections.count > 0
               ? qsTr("Nothing here with this filter")
               : qsTr("No collections yet")
        message: Library.collections.count > 0
                 ? qsTr("Choose another option above.")
                 : qsTr("A film series appears here once one of its films is matched.")
    }
}
