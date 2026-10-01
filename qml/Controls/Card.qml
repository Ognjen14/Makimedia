import QtQuick
import "../Singletons"

Rectangle {
    id: root

    property bool variant: false

    default property alias content: _content.data

    radius: AppTheme.radiusLarge
    color: variant ? AppTheme.surfaceVariant : AppTheme.surface
    implicitHeight: _content.childrenRect.height + 2 * AppTheme.spacing14
    implicitWidth: 200

    Item {
        id: _content

        anchors.fill: parent
        anchors.margins: AppTheme.spacing14
    }
}
