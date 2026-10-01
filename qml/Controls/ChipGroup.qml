pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Flow {
    id: root

    property var options: []
    property var value
    property string valueRole: ""
    property string textRole: ""
    property var textFor: null

    signal chosen(var value)

    function valueOf(option) {
        return root.valueRole.length > 0 ? option[root.valueRole] : option
    }

    function textOf(option) {
        if (root.textFor)
            return root.textFor(option)
        return root.textRole.length > 0 ? option[root.textRole] : String(option)
    }

    function matches(option) {
        const candidate = root.valueOf(option)
        if (typeof candidate === "number" && typeof root.value === "number")
            return Math.abs(candidate - root.value) < 0.01
        return candidate === root.value
    }

    spacing: AppTheme.spacing6

    Repeater {
        model: root.options

        delegate: Chip {
            required property var modelData

            text: root.textOf(modelData)
            selected: root.matches(modelData)
            onClicked: root.chosen(root.valueOf(modelData))
        }
    }
}
