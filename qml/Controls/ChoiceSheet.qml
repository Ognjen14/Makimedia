pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

BottomSheet {
    id: root

    property var options: []
    property int selectedIndex: 0

    signal chosen(int index)

    Column {
        width: parent.width
        spacing: 0

        Repeater {
            model: root.options

            delegate: SheetRow {
                required property int index
                required property string modelData

                width: parent.width
                title: modelData
                selected: root.selectedIndex === index

                onClicked: {
                    root.chosen(index)
                    root.close()
                }
            }
        }
    }
}
