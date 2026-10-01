pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

FocusScope {
    id: root

    property int size: 1
    property int cursor: size

    readonly property var icons: [Icons.gridSmall, Icons.gridMedium, Icons.gridLarge]
    readonly property var names: [qsTr("Small posters"), qsTr("Medium posters"), qsTr("Large posters")]

    signal chosen(int size)
    signal leftEdgeReached()
    signal downRequested()

    activeFocusOnTab: AppTheme.remoteNavigation
    implicitWidth: _row.implicitWidth + 2 * AppTheme.spacing4
    implicitHeight: AppTheme.controlHeightSmall + 2 * AppTheme.spacing4

    onActiveFocusChanged: if (activeFocus) cursor = size

    Keys.onLeftPressed: {
        if (cursor === 0)
            root.leftEdgeReached()
        else
            cursor = cursor - 1
    }
    Keys.onRightPressed: cursor = Math.min(2, cursor + 1)
    Keys.onDownPressed: root.downRequested()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Select
            || event.key === Qt.Key_Return
            || event.key === Qt.Key_Enter
            || event.key === Qt.Key_Space) {
            root.chosen(cursor)
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: AppTheme.radiusPill
        color: "transparent"
        border.width: 1
        border.color: AppTheme.outlineStrong
    }

    Row {
        id: _row

        anchors.centerIn: parent
        spacing: AppTheme.spacing2

        Repeater {
            model: 3

            delegate: Rectangle {
                id: _segment

                required property int index

                readonly property bool picked: root.size === index
                readonly property bool cursorHere: root.activeFocus && root.cursor === index

                width: 44
                height: AppTheme.controlHeightSmall
                radius: AppTheme.radiusPill
                color: picked ? AppTheme.primary
                              : (_hover.hovered ? AppTheme.hover : "transparent")
                border.width: cursorHere ? 2 : 0
                border.color: AppTheme.focus

                Accessible.role: Accessible.RadioButton
                Accessible.name: root.names[index]
                Accessible.checked: picked

                ThemedIcon {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    source: root.icons[_segment.index]
                    tintColor: _segment.picked ? AppTheme.onPrimaryStrong
                                               : AppTheme.textSecondary
                    showPlaceholder: false
                }

                HoverHandler {
                    id: _hover
                }

                TapHandler {
                    onTapped: root.chosen(_segment.index)
                }
            }
        }
    }
}
