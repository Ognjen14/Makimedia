pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal drawerRequested()
    signal discardRequested(var fileHandles, string title, string folder)

    readonly property bool acceptsFocus: Library.unmatchedShows.count > 0

    property var fixCandidates: []
    property bool fixSearching: false
    property var fixShow: null

    function takeFocus() {
        if (_list.currentIndex < 0)
            _list.currentIndex = 0
        _list.forceActiveFocus()
    }

    property string openTitle: ""

    function describe(show) {
        const files = show.fileCount === 1
                    ? qsTr("1 file")
                    : qsTr("%1 files").arg(show.fileCount)

        if (!show.seasons || show.seasons.length === 0)
            return files
        if (show.seasons.length === 1)
            return qsTr("%1 · Season %2").arg(files).arg(show.seasons[0])
        return qsTr("%1 · Seasons %2").arg(files).arg(show.seasons.join(", "))
    }

    function reasonsFor(show) {
        const reasons = []

        if (show.titleFromFolder && show.titleFolder.length > 0) {
            reasons.push(qsTr("The folder is called \"%1\", so that is the name "
                              + "we searched for.").arg(show.titleFolder))
        } else {
            reasons.push(qsTr("The file names carry the name \"%1\" themselves.")
                         .arg(show.title))
        }

        if (show.episodeMarkers)
            reasons.push(qsTr("They are numbered by season and episode, like S01E02."))
        if (show.seasonFolders)
            reasons.push(qsTr("They sit in a season folder."))
        if (show.numberedEpisodes)
            reasons.push(qsTr("They carry episode numbers with no season."))
        if (!show.episodeMarkers && !show.seasonFolders && !show.numberedEpisodes)
            reasons.push(qsTr("The folder name carries a season number."))

        if (show.fileCount === 1) {
            reasons.push(qsTr("There is only one file here, and a show usually "
                              + "has more, so this one is worth checking."))
        }

        return reasons
    }

    property var menuShow: null

    function openCardMenu(show) {
        root.menuShow = show
        _cardMenu.open()
    }

    function beginFix(show) {
        root.fixShow = show
        root.fixCandidates = []
        _fixSheet.beginWith(show.title, true)
    }

    ListView {
        id: _list

        anchors.fill: parent
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: S.AppTheme.spacing12
        clip: true
        visible: Library.unmatchedShows.count > 0
        spacing: S.AppTheme.spacing8
        model: Library.unmatchedShows
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: 0

        Keys.onLeftPressed: (event) => {
            root.drawerRequested()
            event.accepted = true
        }

        Keys.onPressed: (event) => {
            if (S.AppTheme.isActivateKey(event.key)) {
                if (_list.currentItem)
                    _list.currentItem.activate()
                event.accepted = true
            }
        }

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        delegate: Item {
            id: _row

            required property int index
            required property var model

            function asShow() {
                return {
                    "title": _row.model.title,
                    "fileCount": _row.model.fileCount,
                    "seasons": _row.model.seasons,
                    "fileHandles": _row.model.fileHandles,
                    "fileNames": _row.model.fileNames,
                    "folderPath": _row.model.folderPath,
                    "titleFromFolder": _row.model.titleFromFolder,
                    "titleFolder": _row.model.titleFolder,
                    "episodeMarkers": _row.model.episodeMarkers,
                    "numberedEpisodes": _row.model.numberedEpisodes,
                    "seasonFolders": _row.model.seasonFolders
                }
            }

            function activate() {
                if (S.AppTheme.remoteNavigation)
                    root.openCardMenu(_row.asShow())
                else
                    root.beginFix(_row.asShow())
            }

            readonly property bool open:
                S.AppTheme.remoteNavigation || root.openTitle === _row.model.title

            readonly property var names:
                _row.model.fileNames === undefined ? [] : _row.model.fileNames
            readonly property var shownNames: _row.names.slice(0, 6)
            readonly property int hiddenNames:
                Math.max(0, _row.model.fileCount - _row.shownNames.length)

            width: ListView.view.width - S.AppTheme.scrollBarWidth
            height: _card.implicitHeight

            Ctrl.Card {
                id: _card

                width: parent.width
                border.width: _row.ListView.isCurrentItem && _list.activeFocus ? 2 : 0
                border.color: S.AppTheme.primary

                Column {
                    width: parent.width
                    spacing: S.AppTheme.spacing2

                    Item {
                        width: parent.width
                        height: Math.max(44, _heading.implicitHeight)

                        Rectangle {
                            id: _icon

                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 44
                            height: 44
                            radius: S.AppTheme.radiusPill
                            color: S.AppTheme.primary

                            Ctrl.ThemedIcon {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: S.Icons.tvShows
                                tintColor: S.AppTheme.onPrimaryStrong
                                showPlaceholder: false
                            }
                        }

                        Column {
                            id: _heading

                            anchors.left: _icon.right
                            anchors.leftMargin: S.AppTheme.spacing12
                            anchors.right: _toggle.left
                            anchors.rightMargin: S.AppTheme.spacing8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2

                            Text {
                                width: parent.width
                                text: _row.model.title
                                color: S.AppTheme.textPrimary
                                font.pixelSize: S.AppTheme.fs17
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                text: root.describe(_row.model)
                                color: S.AppTheme.textSecondary
                                font.pixelSize: S.AppTheme.fs13
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                visible: !_row.open
                                topPadding: S.AppTheme.spacing2
                                text: root.reasonsFor(_row.model)[0]
                                color: S.AppTheme.textDisabled
                                font.pixelSize: S.AppTheme.fs12
                                elide: Text.ElideRight
                            }
                        }

                        Ctrl.IconButton {
                            id: _toggle

                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !S.AppTheme.remoteNavigation
                            compact: true
                            iconSource: S.Icons.chevronDown
                            rotation: _row.open ? 180 : 0
                            accessibleName: _row.open
                                            ? qsTr("Hide the details")
                                            : qsTr("Why this looks like a show")

                            onClicked: root.openTitle =
                                       _row.open ? "" : _row.model.title
                        }

                        MouseArea {
                            anchors.left: parent.left
                            anchors.right: _toggle.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            enabled: !S.AppTheme.remoteNavigation

                            onClicked: {
                                _list.currentIndex = _row.index
                                root.openTitle = _row.open ? "" : _row.model.title
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        visible: _row.open
                        spacing: S.AppTheme.spacing2

                        Ctrl.SectionLabel {
                            text: qsTr("Why we think this is a show")
                            topPadding: S.AppTheme.spacing16
                            bottomPadding: S.AppTheme.spacing6
                        }

                        Repeater {
                            model: root.reasonsFor(_row.model)

                            delegate: Text {
                                required property string modelData

                                width: _card.width - 2 * S.AppTheme.spacing14
                                text: "· " + modelData
                                color: S.AppTheme.textSecondary
                                font.pixelSize: S.AppTheme.fs13
                                lineHeight: 1.3
                                lineHeightMode: Text.ProportionalHeight
                                wrapMode: Text.Wrap
                            }
                        }

                        Ctrl.SectionLabel {
                            visible: _row.model.folderPath.length > 0
                            text: qsTr("Where they are")
                            topPadding: S.AppTheme.spacing16
                            bottomPadding: S.AppTheme.spacing6
                        }

                        Text {
                            width: parent.width
                            visible: _row.model.folderPath.length > 0
                            text: _row.model.folderPath
                            color: S.AppTheme.textSecondary
                            font.family: S.AppTheme.monoFontFamily
                            font.pixelSize: S.AppTheme.fs13
                            wrapMode: Text.WrapAnywhere
                        }

                        Ctrl.SectionLabel {
                            visible: _row.shownNames.length > 0
                            text: _row.model.fileCount === 1
                                  ? qsTr("The file")
                                  : qsTr("The files")
                            topPadding: S.AppTheme.spacing16
                            bottomPadding: S.AppTheme.spacing6
                        }

                        Repeater {
                            model: _row.shownNames

                            delegate: Text {
                                required property string modelData

                                width: _card.width - 2 * S.AppTheme.spacing14
                                text: modelData
                                color: S.AppTheme.textPrimary
                                font.family: S.AppTheme.monoFontFamily
                                font.pixelSize: S.AppTheme.fs13
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }

                        Text {
                            width: parent.width
                            visible: _row.hiddenNames > 0
                            text: qsTr("and %1 more").arg(_row.hiddenNames)
                            color: S.AppTheme.textSecondary
                            font.pixelSize: S.AppTheme.fs12
                        }

                        Item {
                            width: 1
                            height: S.AppTheme.spacing16
                        }

                        Flow {
                            width: parent.width
                            visible: !S.AppTheme.remoteNavigation
                            spacing: S.AppTheme.spacing8

                            Ctrl.AppButton {
                                text: qsTr("Match the show")
                                variant: Ctrl.AppButton.Filled
                                size: Ctrl.AppButton.Medium

                                onClicked: {
                                    _list.currentIndex = _row.index
                                    _row.activate()
                                }
                            }

                            Ctrl.AppButton {
                                text: qsTr("Not a show")
                                variant: Ctrl.AppButton.Outlined
                                size: Ctrl.AppButton.Medium

                                onClicked: {
                                    const show = _row.asShow()
                                    root.openTitle = ""
                                    root.discardRequested(show.fileHandles,
                                                          show.title,
                                                          show.folderPath)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: Library.unmatchedShows.count === 0
        iconSource: S.Icons.noVideo
        title: qsTr("Nothing to identify")
        message: qsTr("A folder of episodes TMDB could not name appears here, "
                      + "one entry per show rather than one per file.")
    }

    Connections {
        target: Metadata

        function onCandidatesReady(candidates) {
            root.fixSearching = false
            root.fixCandidates = candidates
        }
    }

    Ctrl.BottomSheet {
        id: _cardMenu

        title: root.menuShow ? root.menuShow.title : ""

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.search
                title: qsTr("Match the show")
                subtitle: qsTr("Find it on TMDB and name every file at once")

                onClicked: {
                    const show = root.menuShow
                    _cardMenu.close()
                    if (show)
                        root.beginFix(show)
                }
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                destructive: true
                iconSource: S.Icons.close
                title: qsTr("Not a show")
                subtitle: qsTr("The files stay in your library and are never "
                               + "grouped again")

                onClicked: {
                    const show = root.menuShow
                    _cardMenu.close()
                    if (show) {
                        root.discardRequested(show.fileHandles, show.title,
                                              show.folderPath)
                    }
                }
            }
        }
    }

    Ctrl.FixMatchSheet {
        id: _fixSheet

        searchTv: true
        candidates: root.fixCandidates
        searching: root.fixSearching

        fileNames: root.fixShow ? root.fixShow.fileNames : []
        fileTotal: root.fixShow ? root.fixShow.fileCount : 0
        folderPath: root.fixShow ? root.fixShow.folderPath : ""
        guessTitle: root.fixShow ? root.fixShow.title : ""
        guessConfirmable: false

        onSearchRequested: (text, tv) => {
            root.fixSearching = true
            Metadata.searchCandidates(text, tv)
        }

        onPicked: (candidate) => {
            if (root.fixShow)
                Metadata.pinMatchForShow(root.fixShow.fileHandles, candidate)
        }
    }
}
