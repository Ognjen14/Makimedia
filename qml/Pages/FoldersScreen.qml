pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal detailsRequested(string handle)
    signal itemMenuRequested(string handle, string displayName)
    signal drawerRequested()

    readonly property var activeList: atRootList ? _rootList : _browseList

    readonly property bool acceptsFocus:
        atRootList ? Library.folders.count > 0 : Library.browseEntries.count > 0

    function takeFocus() {
        const list = activeList
        if (list.currentIndex < 0)
            list.currentIndex = 0
        list.forceActiveFocus()
    }

    function handleKey(list, event) {
        if (event.key === Qt.Key_Left) {
            root.drawerRequested()
            event.accepted = true
            return
        }
        if (S.AppTheme.isActivateKey(event.key)) {
            if (list.currentItem)
                list.currentItem.activate()
            event.accepted = true
        }
    }

    readonly property bool atRootList: Library.browsePath.length === 0
    readonly property bool canGoBack: !atRootList

    function goBack() {
        if (atRootList)
            return false
        if (Library.canBrowseUp)
            Library.browseUp()
        else
            Library.browseRootList()
        return true
    }

    Column {
        id: _summary

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing16
        spacing: S.AppTheme.spacing12
        visible: !root.atRootList

        Ctrl.Card {
            width: parent.width
            variant: true
            implicitHeight: _kv.implicitHeight + 2 * S.AppTheme.spacing14

            Column {
                id: _kv

                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 0

                readonly property real labelColumn: {
                    let widest = 74
                    for (let i = 0; i < children.length; ++i) {
                        const row = children[i]
                        if (row.visible && row.implicitLabelWidth !== undefined)
                            widest = Math.max(widest, row.implicitLabelWidth)
                    }
                    return widest
                }

                Ctrl.KeyValueRow {
                    width: parent.width
                    label: qsTr("Tree")
                    value: Library.browsePath
                    mono: true
                }

                Ctrl.KeyValueRow {
                    width: parent.width
                    label: qsTr("Contents")
                    value: qsTr("%n folder(s)", "", Library.browseFolderCount)
                           + "  ·  "
                           + qsTr("%n file(s)", "", Library.browseFileCount)
                }
            }
        }

        Row {
            spacing: S.AppTheme.spacing8

            Ctrl.AppButton {
                text: qsTr("Up")
                size: Ctrl.AppButton.Medium
                iconSource: S.Icons.chevronLeft
                visible: Library.canBrowseUp
                onClicked: Library.browseUp()
            }

            Ctrl.AppButton {
                text: qsTr("All folders")
                size: Ctrl.AppButton.Medium
                onClicked: Library.browseRootList()
            }
        }
    }

    Row {
        id: _rootActions

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.topMargin: S.AppTheme.spacing4
        visible: root.atRootList
        spacing: S.AppTheme.spacing8

        Ctrl.AppButton {
            text: qsTr("Add folder")
            size: Ctrl.AppButton.Medium
            variant: Ctrl.AppButton.Filled
            iconSource: S.Icons.plus
            visible: !System.usesManagedStorageRoots
            enabled: Library.ready && !Library.scanning
            onClicked: Library.addFolder()
        }

        Ctrl.AppButton {
            text: qsTr("Rescan all")
            size: Ctrl.AppButton.Medium
            enabled: Library.ready && !Library.scanning && Library.folders.count > 0
            onClicked: Library.rescanAll()
        }
    }

    ListView {
        id: _rootList

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _rootActions.bottom
        anchors.bottom: parent.bottom
        anchors.margins: S.AppTheme.spacing16
        clip: true
        visible: root.atRootList && Library.folders.count > 0
        spacing: 0
        model: Library.folders
        boundsBehavior: Flickable.StopAtBounds

        currentIndex: 0
        Keys.onPressed: (event) => root.handleKey(_rootList, event)

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        delegate: Column {
            id: _rootDelegate

            required property var folderId
            required property string displayName
            required property string lastScanned
            required property bool available

            width: ListView.view.width - S.AppTheme.scrollBarWidth

            function activate() {
                Library.browseRoot(_rootDelegate.folderId)
            }

            Item {
                width: parent.width
                height: _rootRow.implicitHeight

                Ctrl.ListRow {
                    id: _rootRow

                    anchors.left: parent.left
                    anchors.right: _rootRowActions.left
                    anchors.rightMargin: S.AppTheme.spacing4
                    highlighted: _rootDelegate.ListView.isCurrentItem
                                 && _rootList.activeFocus
                    leading: Ctrl.ListRow.Icon
                    iconSource: S.Icons.folder
                    title: _rootDelegate.displayName
                    subtitle: !_rootDelegate.available
                              ? qsTr("Unavailable, files kept")
                              : (_rootDelegate.lastScanned.length > 0
                                 ? qsTr("Scanned %1").arg(_rootDelegate.lastScanned)
                                 : qsTr("Never scanned"))
                    dimmed: !_rootDelegate.available

                    onClicked: _rootDelegate.activate()
                }

                Row {
                    id: _rootRowActions

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: S.AppTheme.spacing4

                    Ctrl.IconButton {
                        compact: true
                        iconSource: S.Icons.rescan
                        accessibleName: qsTr("Rescan this folder")
                        enabled: !Library.scanning
                        onClicked: Library.rescan(_rootDelegate.folderId)
                    }

                    Ctrl.IconButton {
                        compact: true
                        iconSource: S.Icons.close
                        tintColor: S.AppTheme.error
                        accessibleName: qsTr("Remove this folder")
                        visible: !System.usesManagedStorageRoots
                        onClicked: {
                            root._removeName = _rootDelegate.displayName
                            root._removeId = _rootDelegate.folderId
                            _confirmRemove.open()
                        }
                    }
                }
            }

            Ctrl.Divider {
                width: parent.width
                inset: 88
            }
        }
    }

    ListView {
        id: _browseList

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _summary.bottom
        anchors.bottom: parent.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.topMargin: S.AppTheme.spacing12
        clip: true
        visible: !root.atRootList && Library.browseEntries.count > 0
        spacing: 0
        model: Library.browseEntries
        boundsBehavior: Flickable.StopAtBounds

        currentIndex: 0
        Keys.onPressed: (event) => root.handleKey(_browseList, event)

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        delegate: Column {
            id: _entry

            required property string handle
            required property string displayName
            required property bool isFolder
            required property string summary
            required property string durationText
            required property real watchProgress
            required property bool indexed
            required property string thumbnail

            width: ListView.view.width - S.AppTheme.scrollBarWidth

            function activate() {
                if (_entry.isFolder)
                    Library.browseInto(_entry.handle)
                else
                    root.detailsRequested(_entry.handle)
            }

            Ctrl.ListRow {
                width: parent.width
                highlighted: _entry.ListView.isCurrentItem && _browseList.activeFocus
                leading: _entry.isFolder ? Ctrl.ListRow.Icon : Ctrl.ListRow.Thumbnail
                iconSource: _entry.isFolder ? S.Icons.folder : ""
                thumbnailSource: _entry.thumbnail
                title: _entry.displayName
                monoTitle: !_entry.isFolder
                subtitle: _entry.summary
                trailingText: _entry.durationText
                progress: _entry.watchProgress

                onClicked: _entry.activate()

                onContextRequested: {
                    if (!_entry.isFolder)
                        root.itemMenuRequested(_entry.handle, _entry.displayName)
                }
            }

            Ctrl.Divider {
                width: parent.width
                inset: 88
            }
        }
    }

    property string _removeName: ""
    property var _removeId: -1

    Ctrl.AppDialog {
        id: _confirmRemove

        z: 60
        title: qsTr("Remove folder?")
        message: qsTr("%1 stops being scanned. Files already indexed are removed from the library, but nothing on disk is touched.")
                 .arg(root._removeName)
        dismissText: qsTr("Cancel")
        acceptText: qsTr("Remove")

        onAccepted: Library.removeFolder(root._removeId)
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: root.atRootList && Library.folders.count === 0
        iconSource: S.Icons.folder
        title: qsTr("No folders")
        message: System.usesManagedStorageRoots
                 ? qsTr("Storage is added for you once Makimedia can read your videos.")
                 : qsTr("Add a folder and you can browse it here, with or without metadata.")
        actionText: System.usesManagedStorageRoots ? "" : qsTr("Add folder")
        onActionTriggered: Library.addFolder()
    }

    Ctrl.EmptyState {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * S.AppTheme.spacing24, 420)
        visible: !root.atRootList && Library.browseEntries.count === 0
        iconSource: S.Icons.noVideo
        title: qsTr("Nothing in this folder")
        message: qsTr("No subfolders and no video files here.")
    }
}
