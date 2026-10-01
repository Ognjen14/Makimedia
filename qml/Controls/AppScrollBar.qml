pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import "../Singletons"

ScrollBar {
    id: root

    readonly property bool engaged: root.pressed || root.hovered
    readonly property bool scrollable: root.size > 0 && root.size < 1.0

    implicitWidth: AppTheme.scrollBarWidth
    implicitHeight: AppTheme.scrollBarWidth
    padding: 4
    minimumSize: 0.12
    policy: ScrollBar.AsNeeded

    readonly property int trackWidth: engaged ? 6 : 4

    contentItem: Rectangle {
        implicitWidth: root.trackWidth
        implicitHeight: root.trackWidth
        radius: width / 2
        color: root.engaged ? AppTheme.primary : AppTheme.outlineStrong
        opacity: root.scrollable ? (root.engaged ? 1.0 : 0.7) : 0

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Behavior on implicitWidth {
            NumberAnimation { duration: 120 }
        }
    }

    background: Rectangle {
        implicitWidth: AppTheme.scrollBarWidth
        implicitHeight: AppTheme.scrollBarWidth
        radius: width / 2
        color: AppTheme.surfaceVariant
        opacity: root.scrollable && root.engaged ? 0.5 : 0

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }
}
