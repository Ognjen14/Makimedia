pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Flow {
    id: root

    property var cast: []

    readonly property int columns:
        Math.max(3, Math.floor((width + spacing) / (112 + spacing)))
    readonly property real itemWidth:
        (width - (columns - 1) * spacing) / columns

    visible: cast.length > 0
    spacing: AppTheme.spacing16

    Repeater {
        model: root.cast

        delegate: CastFace {
            required property var modelData

            width: root.itemWidth
            person: modelData
        }
    }
}
