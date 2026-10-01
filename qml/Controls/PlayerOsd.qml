pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Rectangle {
    id: root

    enum Placement {
        Centre,
        Left,
        Right
    }

    property int placement: PlayerOsd.Centre
    property real panelOpacity: 0.78
    property string bigText
    property string captionText
    property real meterValue: -1
    property int holdMs: 900

    readonly property bool showMeter: meterValue >= 0

    function flash(big, caption, meter) {
        bigText = big
        captionText = caption === undefined ? "" : caption
        meterValue = meter === undefined ? -1 : meter
        opacity = 1
        _timer.restart()
    }

    function show(big, caption, meter) {
        bigText = big
        captionText = caption === undefined ? "" : caption
        meterValue = meter === undefined ? -1 : meter
        _timer.stop()
        opacity = 1
    }

    function hide() {
        _timer.stop()
        opacity = 0
    }

    width: Math.max(132, _column.implicitWidth + 2 * AppTheme.spacing24)
    height: _column.implicitHeight + 2 * AppTheme.spacing16
    radius: AppTheme.radiusLarge
    color: Qt.rgba(0, 0, 0, root.panelOpacity)
    opacity: 0
    visible: opacity > 0.01

    Behavior on opacity {
        NumberAnimation { duration: 140 }
    }

    Timer {
        id: _timer

        interval: root.holdMs
        onTriggered: root.opacity = 0
    }

    Column {
        id: _column

        anchors.centerIn: parent
        spacing: AppTheme.spacing6

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.bigText
            color: "#FFFFFF"
            font.pixelSize: AppTheme.fs28
            font.weight: Font.Bold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.captionText.length > 0
            text: root.captionText
            color: "#FFFFFF"
            opacity: 0.7
            font.pixelSize: AppTheme.fs11
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.showMeter
            width: 120
            height: 4
            radius: AppTheme.radiusPill
            color: Qt.rgba(1, 1, 1, 0.25)

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.meterValue))
                height: parent.height
                radius: parent.radius
                color: AppTheme.primary
            }
        }
    }
}
