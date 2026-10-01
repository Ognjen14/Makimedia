pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property string text: ""

    property int scalePercent: 100
    property string edgeStyle: "outline"
    property string position: "bottom"
    property color textColor: "#FFFFFFFF"
    property bool bold: false

    visible: text.length > 0

    readonly property int fontSize:
        Math.max(12, Math.round(height * 0.045
                                * Math.max(10, Math.min(400, scalePercent)) / 100))

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter

        anchors.top: root.position === "top" ? parent.top : undefined
        anchors.bottom: root.position === "top" ? undefined : parent.bottom

        anchors.topMargin: Math.round(root.height * 0.06)
        anchors.bottomMargin: root.position === "raised"
                              ? Math.round(root.height * 0.18)
                              : Math.round(root.height * 0.06)

        width: Math.min(_label.paintedWidth + 2 * AppTheme.spacing14, root.width)
        height: _label.paintedHeight + 2 * AppTheme.spacing8
        radius: AppTheme.radiusSmall

        color: root.edgeStyle === "box" ? "#A0000000" : "transparent"

        Text {
            id: _label

            anchors.centerIn: parent

            width: Math.round(root.width * 0.86)

            text: root.text
            color: root.textColor
            font.pixelSize: root.fontSize
            font.weight: root.bold ? Font.Bold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            lineHeight: 1.2

            style: {
                if (root.edgeStyle === "none" || root.edgeStyle === "box")
                    return Text.Normal
                if (root.edgeStyle === "shadow")
                    return Text.Raised
                return Text.Outline
            }
            styleColor: "#000000"
        }
    }
}
