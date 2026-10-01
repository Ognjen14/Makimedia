import QtQuick
import "../Singletons"

ListView {
    id: root

    property int leadInset: 0
    property int tailInset: 0

    signal leftEdgeReached()
    signal rightEdgeReached()
    signal activated(int index)

    function step(delta) {
        const next = root.currentIndex + delta
        if (next < 0) {
            root.leftEdgeReached()
            return
        }
        if (next >= root.count) {
            root.rightEdgeReached()
            return
        }
        root.currentIndex = next
        if (next === 0)
            root.positionViewAtBeginning()
        else
            root.positionViewAtIndex(next, ListView.Contain)
    }

    orientation: ListView.Horizontal
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    keyNavigationEnabled: false

    header: Item {
        width: root.leadInset
        height: 1
    }
    footer: Item {
        width: root.tailInset
        height: 1
    }

    Keys.onLeftPressed: root.step(-1)
    Keys.onRightPressed: root.step(1)
    Keys.onPressed: (event) => {
        if (AppTheme.isActivateKey(event.key)) {
            if (root.currentIndex >= 0)
                root.activated(root.currentIndex)
            event.accepted = true
        }
    }
}
