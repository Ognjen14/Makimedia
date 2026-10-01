pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    property url source
    property int fillMode: Image.PreserveAspectCrop

    readonly property bool ready: _current.status === Image.Ready
        || (_current.status === Image.Loading && _held.status === Image.Ready)

    Image {
        id: _held

        anchors.fill: parent
        fillMode: root.fillMode
        asynchronous: true
        cache: true
        visible: status === Image.Ready && _current.status === Image.Loading
    }

    Image {
        id: _current

        anchors.fill: parent
        source: root.source
        fillMode: root.fillMode
        asynchronous: true
        cache: true
        visible: status === Image.Ready

        onStatusChanged: {
            if (status === Image.Ready)
                _held.source = source
            else if (status !== Image.Loading)
                _held.source = ""
        }
    }
}
