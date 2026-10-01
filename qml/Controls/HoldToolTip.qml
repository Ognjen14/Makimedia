import QtQuick
import QtQuick.Controls
import "../Singletons"

Item {
    id: root

    property string text: ""

    function hold() {
        if (root.text.length > 0)
            _heldTip.restart()
    }

    anchors.fill: parent

    HoverHandler {
        id: _pointer

        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
    }

    Timer {
        id: _heldTip

        interval: 2500
    }

    ToolTip {
        id: _tip

        x: Math.round((root.width - width) / 2)
        y: -_tip.height - AppTheme.spacing6
        visible: root.text.length > 0
                 && (_pointer.hovered || _heldTip.running)
        delay: _heldTip.running ? 0 : 500
        text: root.text
        closePolicy: Popup.NoAutoClose
        padding: AppTheme.spacing8
        margins: AppTheme.spacing8

        contentItem: Text {
            text: _tip.text
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fs13
            wrapMode: Text.NoWrap
        }

        background: Rectangle {
            color: AppTheme.surfaceVariant
            radius: AppTheme.radiusSmall
            border.width: 1
            border.color: AppTheme.outlineStrong
        }
    }
}
