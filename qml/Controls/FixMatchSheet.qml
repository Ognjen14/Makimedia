pragma ComponentBehavior: Bound

import QtQuick
import com.topicdev.makimedia 1.0
import "../Singletons" as S

BottomSheet {
    id: root

    property string searchText: ""
    property bool searchTv: false
    property bool pinned: false
    property bool matched: false
    property var currentTmdbId: 0

    property var candidates: []
    property bool searching: false

    property string fileName: ""
    property string folderPath: ""
    property var fileNames: []
    property int fileTotal: 0
    property string guessTitle: ""
    property bool guessConfirmable: false

    readonly property var shownNames:
        root.fileNames.length > 0 ? root.fileNames.slice(0, 3)
                                  : (root.fileName.length > 0 ? [root.fileName] : [])
    readonly property int hiddenNames:
        Math.max(0, (root.fileTotal > 0 ? root.fileTotal : root.fileNames.length)
                    - root.shownNames.length)
    readonly property bool describesFiles: root.shownNames.length > 0

    signal picked(var candidate)
    signal unpinRequested()
    signal notMediaRequested()
    signal searchRequested(string text, bool tv)
    signal guessConfirmed()

    function beginWith(text, tv) {
        root.searchTv = tv
        _searchBar.text = text
        root.open()
        root.runSearch()
    }

    function runSearch() {
        _pending.restart()
    }

    RemoteKeyboard {
        id: _keyboard

        parent: root
        z: 10

        onAccepted: (text) => {
            _searchBar.text = text
            root.runSearchNow()
            Qt.callLater(_searchBar.forceFocus)
        }
        onCancelled: Qt.callLater(_searchBar.forceFocus)
    }

    function runSearchNow() {
        _pending.stop()
        root.searchRequested(_searchBar.text, root.searchTv)
    }

    Timer {
        id: _pending

        interval: 600
        onTriggered: {
            if (_searchBar.text.trim().length >= 2)
                root.searchRequested(_searchBar.text, root.searchTv)
        }
    }

    z: 60
    title: qsTr("Fix match")

    Column {
        width: parent.width
        spacing: 0

        Column {
            id: _found

            width: parent.width
            visible: root.describesFiles
            spacing: S.AppTheme.spacing2

            Text {
                width: parent.width
                text: root.fileTotal > 1
                      ? qsTr("We found %1 files").arg(root.fileTotal)
                      : qsTr("We found this file")
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs12
                font.weight: Font.Medium
                font.letterSpacing: 0.06 * S.AppTheme.fs12
                font.capitalization: Font.AllUppercase
                bottomPadding: S.AppTheme.spacing6
            }

            Repeater {
                model: root.shownNames

                delegate: Text {
                    required property string modelData

                    width: _found.width
                    text: modelData
                    color: S.AppTheme.textPrimary
                    font.family: S.AppTheme.monoFontFamily
                    font.pixelSize: S.AppTheme.fs14
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
            }

            Text {
                width: parent.width
                visible: root.hiddenNames > 0
                text: qsTr("and %1 more").arg(root.hiddenNames)
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs12
            }

            Text {
                width: parent.width
                visible: root.folderPath.length > 0
                topPadding: S.AppTheme.spacing4
                text: qsTr("in %1").arg(root.folderPath)
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs13
                elide: Text.ElideMiddle
            }

            Text {
                width: parent.width
                visible: root.guessTitle.length > 0
                topPadding: S.AppTheme.spacing12
                text: root.fileTotal > 1
                      ? qsTr("We think these are episodes of %1.").arg(root.guessTitle)
                      : qsTr("We think this is %1.").arg(root.guessTitle)
                color: S.AppTheme.textPrimary
                font.pixelSize: S.AppTheme.fs15
                wrapMode: Text.Wrap
            }

            Item {
                id: _confirmRow

                width: parent.width
                visible: root.guessConfirmable && root.guessTitle.length > 0
                height: visible ? _confirm.height + S.AppTheme.spacing8 : 0

                AppButton {
                    id: _confirm

                    y: S.AppTheme.spacing8
                    text: qsTr("Yes, that is right")
                    variant: AppButton.Tonal
                    size: AppButton.Medium

                    onClicked: {
                        root.guessConfirmed()
                        root.close()
                    }
                }
            }

            Text {
                width: parent.width
                topPadding: S.AppTheme.spacing8
                bottomPadding: S.AppTheme.spacing12
                text: {
                    if (root.guessTitle.length === 0)
                        return qsTr("Search below for the title it should have.")
                    return root.guessConfirmable
                           ? qsTr("If we have it wrong, search below for the right one.")
                           : qsTr("If that is wrong, search below for the right one.")
                }
                color: S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs12
                wrapMode: Text.Wrap
            }

            Divider {
                width: parent.width
            }
        }

        SearchBar {
            id: _searchBar

            width: parent.width
            placeholder: qsTr("Title to search for")
            onAccepted: root.runSearch()
            onDownRequested: root.stepVertical(true)
            onTypingRequested: _keyboard.open(qsTr("Title to search for"),
                                             _searchBar.text)
        }

        Row {
            topPadding: S.AppTheme.spacing8
            bottomPadding: S.AppTheme.spacing8
            spacing: S.AppTheme.spacing6

            Chip {
                text: qsTr("Movie")
                selected: !root.searchTv
                onClicked: {
                    root.searchTv = false
                    root.runSearchNow()
                }
            }

            Chip {
                text: qsTr("TV show")
                selected: root.searchTv
                onClicked: {
                    root.searchTv = true
                    root.runSearchNow()
                }
            }

            Chip {
                text: qsTr("Search")
                onClicked: root.runSearchNow()
            }
        }

        Divider { width: parent.width }

        SheetRow {
            width: parent.width
            showRadio: false
            visible: root.pinned
            destructive: true
            iconSource: S.Icons.close
            title: qsTr("Stop pinning this one")
            subtitle: qsTr("Makimedia goes back to matching it automatically")
            onClicked: {
                root.unpinRequested()
                root.close()
            }
        }

        SheetRow {
            width: parent.width
            showRadio: false
            visible: root.matched
            destructive: true
            iconSource: S.Icons.close
            title: qsTr("This is not a film or an episode")
            subtitle: qsTr("Keeps the file and leaves it with no title")
            onClicked: {
                root.notMediaRequested()
                root.close()
            }
        }

        Text {
            width: parent.width
            topPadding: S.AppTheme.spacing12
            visible: root.searching || _pending.running
                     || root.candidates.length === 0
            text: root.searching || _pending.running
                  ? qsTr("Searching…")
                  : qsTr("Nothing found. Try a shorter title, or switch between Movie and TV show.")
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs12
            wrapMode: Text.Wrap
        }

        Repeater {
            model: root.candidates

            delegate: SheetRow {
                required property var modelData

                width: parent.width
                showRadio: false
                title: modelData.year > 0
                       ? qsTr("%1 (%2)").arg(modelData.title).arg(modelData.year)
                       : modelData.title
                subtitle: modelData.overview.length > 0
                          ? modelData.overview
                          : qsTr("No summary")
                selected: root.currentTmdbId === modelData.tmdbId

                onClicked: {
                    root.picked(modelData)
                    root.close()
                }
            }
        }
    }
}
