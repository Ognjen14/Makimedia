pragma ComponentBehavior: Bound

import QtQuick
import com.topicdev.makimedia 1.0
import "../../Singletons" as S
import "../../Controls" as Ctrl

Ctrl.BottomSheet {
    id: root

    property string handle
    property var attached: []

    title: qsTr("Subtitles")

    function reload() {
        attached = root.handle.length > 0
                   ? Library.attachedSubtitles(root.handle)
                   : []
    }

    onHandleChanged: root.reload()
    Component.onCompleted: root.reload()

    onOpenedChanged: {
        if (root.opened)
            root.reload()
    }

    function add() {
        root.awaitingPick = true
        System.pickSubtitleFile()
    }

    function remove(entry) {
        Library.detachSubtitle(root.handle, entry.handle)
        root.reload()
        Qt.callLater(root.focusAddRow)
    }

    function focusAddRow() {
        _addRow.forceActiveFocus(Qt.TabFocusReason)
    }

    property bool awaitingPick: false

    Connections {
        target: System

        function onSubtitleFilePicked(url) {
            if (!root.awaitingPick)
                return
            root.awaitingPick = false
            Library.attachSubtitle(root.handle, url)
            root.reload()
        }

        function onSubtitleFilePickCancelled() {
            root.awaitingPick = false
        }
    }

    Column {
        width: parent.width
        spacing: 0

        Ctrl.SectionLabel {
            visible: root.attached.length > 0
            text: qsTr("Added to this video")
        }

        Repeater {
            model: root.attached

            delegate: Ctrl.SheetRow {
                required property var modelData

                width: parent.width
                showRadio: false
                destructive: true
                iconSource: S.Icons.close
                title: modelData.displayName
                subtitle: qsTr("Loads with this video. Press to remove it.")

                onClicked: root.remove(modelData)
            }
        }

        Ctrl.SectionLabel {
            visible: root.attached.length === 0
            text: qsTr("Nothing added yet")
        }

        Ctrl.Divider { width: parent.width }

        Ctrl.SheetRow {
            id: _addRow

            width: parent.width
            showRadio: false
            iconSource: S.Icons.plus
            title: qsTr("Add a subtitle file")
            subtitle: qsTr("Needs a file manager app installed to browse to the file")

            onClicked: root.add()
        }
    }
}
