pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property string label
    property string value
    property bool mono: false

    readonly property real implicitLabelWidth: _key.implicitWidth

    property real labelWidth: parent && parent.labelColumn !== undefined
                              ? parent.labelColumn
                              : Math.max(74, _key.implicitWidth)

    implicitHeight: Math.max(_key.implicitHeight, _value.implicitHeight) + 6
    implicitWidth: 240

    Text {
        id: _key

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: 3
        width: root.labelWidth
        text: root.label
        color: AppTheme.textDisabled
        font.pixelSize: AppTheme.fs12
    }

    Text {
        id: _value

        anchors.left: _key.right
        anchors.leftMargin: AppTheme.spacing12
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 3
        text: root.value
        color: AppTheme.textSecondary
        font.pixelSize: root.mono ? AppTheme.fs11 : AppTheme.fs12
        font.family: root.mono ? AppTheme.monoFontFamily : AppTheme.fontFamily
        wrapMode: Text.WrapAnywhere
    }
}
