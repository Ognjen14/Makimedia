import QtQuick

QtObject {
    id: root

    property real source: 0
    property bool live: true
    property int delay: 150
    property real value: 0

    readonly property Timer _settle: Timer {
        interval: root.delay
        onTriggered: root.value = root.source
    }

    onSourceChanged: {
        if (root.value <= 0 || !root.live) {
            root._settle.stop()
            root.value = root.source
        } else {
            root._settle.restart()
        }
    }

    Component.onCompleted: root.value = root.source
}
