pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "../Singletons"

Rectangle {
    id: root

    property alias text: _field.text
    property alias placeholder: _field.placeholderText

    signal accepted(string query)
    signal cleared()
    signal leftRequested()
    signal downRequested()

    signal typingRequested()

    readonly property bool typesInPlace: !AppTheme.remoteNavigation

    function forceFocus() {
        if (root.typesInPlace)
            _field.forceActiveFocus()
        else
            root.forceActiveFocus()
    }

    focus: false
    activeFocusOnTab: !root.typesInPlace

    Keys.onPressed: (event) => {
        if (root.typesInPlace)
            return

        switch (event.key) {
        case Qt.Key_Select:
        case Qt.Key_Return:
        case Qt.Key_Enter:
            root.typingRequested()
            event.accepted = true
            return
        case Qt.Key_Down:
            root.downRequested()
            event.accepted = true
            return
        case Qt.Key_Left:
            root.leftRequested()
            event.accepted = true
            return
        case Qt.Key_Back:
        case Qt.Key_Escape:
            if (_field.text.length > 0) {
                _field.text = ""
                root.cleared()
                event.accepted = true
            }
            return
        }
    }

    implicitHeight: AppTheme.controlHeightLarge
    implicitWidth: 240
    radius: AppTheme.radiusPill
    color: AppTheme.surfaceVariant
    border.width: (root.typesInPlace ? _field.activeFocus : root.activeFocus) ? 2 : 0
    border.color: AppTheme.focus

    ThemedIcon {
        id: _icon

        anchors.left: parent.left
        anchors.leftMargin: AppTheme.spacing16
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        source: Icons.search
        tintColor: AppTheme.textSecondary
        showPlaceholder: false
    }

    TextField {
        id: _field

        anchors.left: _icon.right
        anchors.right: _clear.left
        anchors.leftMargin: AppTheme.spacing10
        anchors.rightMargin: AppTheme.spacing8
        anchors.verticalCenter: parent.verticalCenter
        color: AppTheme.textPrimary
        placeholderTextColor: AppTheme.textSecondary
        font.pixelSize: AppTheme.fs14
        selectByMouse: true
        background: Item {}

        activeFocusOnPress: root.typesInPlace
        activeFocusOnTab: root.typesInPlace

        onTextChanged: root.accepted(text)
        Keys.onEscapePressed: {
            text = ""
            root.cleared()
        }

        Keys.onDownPressed: root.downRequested()
        Keys.onPressed: (event) => {
            if (event.key !== Qt.Key_Return && event.key !== Qt.Key_Enter
                    && event.key !== Qt.Key_Select) {
                return
            }
            event.accepted = true

            if (AppTheme.remoteNavigation)
                root.typingRequested()
            else
                root.downRequested()
        }

        Keys.onLeftPressed: (event) => {
            if (_field.cursorPosition === 0) {
                root.leftRequested()
                event.accepted = true
            } else {
                event.accepted = false
            }
        }
    }

    IconButton {
        id: _clear

        anchors.right: parent.right
        anchors.rightMargin: AppTheme.spacing4
        anchors.verticalCenter: parent.verticalCenter
        compact: true
        visible: _field.text.length > 0
        iconSource: Icons.close
        accessibleName: qsTr("Clear search")

        onClicked: {
            _field.text = ""
            root.cleared()
            _field.forceActiveFocus()
        }
    }
}
