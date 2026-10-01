import QtQuick
import "../Singletons"

Item {
    id: root

    property string title
    property string countText

    implicitHeight: _title.implicitHeight + AppTheme.spacing8 + AppTheme.spacing8
    implicitWidth: 200

    Text {
        id: _title

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: AppTheme.spacing8
        text: root.title
        color: AppTheme.textPrimary
        font.pixelSize: AppTheme.fs17
        font.weight: Font.Medium
    }

    Text {
        anchors.left: _title.right
        anchors.leftMargin: AppTheme.spacing8
        anchors.baseline: _title.baseline
        visible: root.countText.length > 0
        text: root.countText
        color: AppTheme.textDisabled
        font.pixelSize: AppTheme.fs12
    }
}
