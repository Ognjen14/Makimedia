import QtQuick
import QtQuick.Controls
import "../Singletons"

AbstractButton {
    id: root

    hoverEnabled: true
    focusPolicy: AppTheme.remoteNavigation ? Qt.StrongFocus : Qt.NoFocus

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Select
            || event.key === Qt.Key_Return
            || event.key === Qt.Key_Enter) {
            if (root.checkable)
                root.toggle()
            root.clicked()
            event.accepted = true
        }
    }
}
