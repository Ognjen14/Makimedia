pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property real value: 0
    property bool indeterminate: false
    property color trackColor: AppTheme.surfaceVariant
    property color fillColor: AppTheme.primary
    property real trackHeight: AppTheme.seekBarHeight

    readonly property real fraction: Math.max(0, Math.min(1, value))

    implicitHeight: trackHeight
    implicitWidth: 200

    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Progress")

    Rectangle {
        id: _track

        anchors.fill: parent
        radius: AppTheme.radiusPill
        color: root.trackColor
        clip: true

        Rectangle {
            visible: !root.indeterminate
            width: parent.width * root.fraction
            height: parent.height
            radius: AppTheme.radiusPill
            color: root.fillColor

            Behavior on width {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            id: _sweep

            property real phase: 0

            visible: root.indeterminate
            width: parent.width * 0.35
            height: parent.height
            x: -width + (_track.width + width) * phase
            radius: AppTheme.radiusPill
            color: root.fillColor

            NumberAnimation on phase {
                running: root.indeterminate && root.visible
                loops: Animation.Infinite
                from: 0
                to: 1
                duration: 1100
            }
        }
    }
}
