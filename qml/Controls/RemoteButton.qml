import QtQuick
import QtQuick.Controls
import "../Singletons"

AbstractButton {
    id: root

    property bool holdable: false
    property bool _holdFired: false

    signal held()

    hoverEnabled: true
    focusPolicy: AppTheme.remoteNavigation ? Qt.StrongFocus : Qt.NoFocus

    Timer {
        id: _hold

        interval: 600
        onTriggered: {
            root._holdFired = true
            root.held()
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Select
            || event.key === Qt.Key_Return
            || event.key === Qt.Key_Enter) {
            event.accepted = true
            if (event.isAutoRepeat && !root.holdable)
                return
            if (root.holdable) {
                if (!event.isAutoRepeat) {
                    root._holdFired = false
                    _hold.restart()
                }
                return
            }
            if (root.checkable)
                root.toggle()
            root.clicked()
        }
    }

    Keys.onReleased: (event) => {
        if (!root.holdable)
            return
        if (event.key !== Qt.Key_Select
            && event.key !== Qt.Key_Return
            && event.key !== Qt.Key_Enter)
            return
        event.accepted = true
        if (event.isAutoRepeat || !_hold.running)
            return
        _hold.stop()
        root.clicked()
    }
}
