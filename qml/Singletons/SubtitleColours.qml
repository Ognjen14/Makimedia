pragma Singleton

import QtQuick

QtObject {
    readonly property var white:    ({ value: "#FFFFFFFF", label: qsTr("White") })
    readonly property var offWhite: ({ value: "#FFF5F5F5", label: qsTr("Off white") })
    readonly property var yellow:   ({ value: "#FFFFE082", label: qsTr("Yellow") })
    readonly property var amber:    ({ value: "#FFFFC107", label: qsTr("Amber") })
    readonly property var cyan:     ({ value: "#FF80DEEA", label: qsTr("Cyan") })
    readonly property var sky:      ({ value: "#FF81D4FA", label: qsTr("Sky") })
    readonly property var green:    ({ value: "#FFA5D6A7", label: qsTr("Green") })
    readonly property var pink:     ({ value: "#FFF48FB1", label: qsTr("Pink") })
    readonly property var lilac:    ({ value: "#FFCE93D8", label: qsTr("Lilac") })
    readonly property var orange:   ({ value: "#FFFF8A65", label: qsTr("Orange") })
    readonly property var black:    ({ value: "#FF000000", label: qsTr("Black") })

    readonly property var all: [
        white, offWhite, yellow, amber, cyan, sky, green, pink, lilac, orange, black
    ]

    readonly property var quick: [white, yellow, cyan, black]
}
