pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

BottomSheet {
    id: root

    property int sortMode: 0
    property int filterMode: 0

    property int pendingSort: 0
    property int pendingFilter: 0

    readonly property var sortOptions: [
        qsTr("Recently added"),
        qsTr("Title"),
        qsTr("Last played"),
        qsTr("File size")
    ]

    readonly property var filterOptions: [
        qsTr("Everything"),
        qsTr("Unwatched"),
        qsTr("In progress"),
        qsTr("Watched"),
        qsTr("Unmatched"),
        qsTr("Suggested")
    ]

    signal applied(int sortMode, int filterMode)
    signal cleared()

    title: qsTr("Sort and filter")

    function openOptions() {
        pendingSort = sortMode
        pendingFilter = filterMode
        open()
    }

    Column {
        width: parent.width
        spacing: 0

        SectionLabel {
            text: qsTr("Sort by")
            topPadding: 0
        }

        Repeater {
            model: root.sortOptions

            delegate: SheetRow {
                required property int index
                required property string modelData

                width: parent.width
                title: modelData
                selected: root.pendingSort === index

                onClicked: root.pendingSort = index
            }
        }

        SectionLabel { text: qsTr("Show") }

        Flow {
            width: parent.width
            spacing: AppTheme.spacing6

            Repeater {
                model: root.filterOptions

                delegate: Chip {
                    required property int index
                    required property string modelData

                    text: modelData
                    selected: root.pendingFilter === index

                    onClicked: root.pendingFilter = index
                }
            }
        }

        Item {
            width: 1
            height: AppTheme.spacing20
        }

        AppButton {
            width: parent.width
            text: qsTr("Apply")
            variant: AppButton.Filled

            onClicked: {
                root.applied(root.pendingSort, root.pendingFilter)
                root.close()
            }
        }

        Item {
            width: 1
            height: AppTheme.spacing8
        }

        AppButton {
            width: parent.width
            visible: root.pendingSort !== 0 || root.pendingFilter !== 0
            text: qsTr("Reset to default")
            variant: AppButton.Plain

            onClicked: {
                root.pendingSort = 0
                root.pendingFilter = 0
                root.cleared()
                root.close()
            }
        }
    }
}
