import QtQuick
import "../Singletons"

Rectangle {
    id: root

    property bool shown: false
    property int fadeDuration: 140

    signal clicked()

    anchors.fill: parent
    color: AppTheme.overlay
    opacity: root.shown ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: root.fadeDuration }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.shown
        onClicked: root.clicked()
        onWheel: (wheel) => { wheel.accepted = true }
    }
}
