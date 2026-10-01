pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Column {
    id: root

    property var hints: []
    property int keyColumnWidth: 84
    property bool monoKeys: true

    property int measuredKeyWidth: 0
    readonly property int columnWidth: Math.max(keyColumnWidth, measuredKeyWidth)

    function noteKeyWidth(width) {
        if (width > measuredKeyWidth)
            measuredKeyWidth = width
    }

    onHintsChanged: measuredKeyWidth = 0

    spacing: AppTheme.spacing8

    Repeater {
        model: root.hints

        delegate: Item {
            id: _row

            required property var modelData

            width: root.width
            height: Math.max(_cap.height, _description.implicitHeight)

            Rectangle {
                id: _cap

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: root.columnWidth
                height: _capLabel.implicitHeight + AppTheme.spacing6
                radius: AppTheme.radiusSmall
                color: AppTheme.surfaceVariant
                border.width: 1
                border.color: AppTheme.outlineStrong

                Text {
                    id: _capLabel

                    anchors.centerIn: parent
                    text: _row.modelData.keys
                    color: AppTheme.textPrimary
                    font.family: root.monoKeys
                                 ? AppTheme.monoFontFamily
                                 : AppTheme.fontFamily
                    font.pixelSize: AppTheme.fs11

                    onImplicitWidthChanged:
                        root.noteKeyWidth(implicitWidth + AppTheme.spacing12)
                    Component.onCompleted:
                        root.noteKeyWidth(implicitWidth + AppTheme.spacing12)
                }
            }

            Text {
                id: _description

                anchors.left: _cap.right
                anchors.right: parent.right
                anchors.leftMargin: AppTheme.spacing16
                anchors.verticalCenter: parent.verticalCenter
                text: _row.modelData.description
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs12
                wrapMode: Text.Wrap
            }
        }
    }
}
