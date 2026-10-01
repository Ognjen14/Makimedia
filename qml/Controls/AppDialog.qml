pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property string title
    property string message
    property bool opened: false
    property string acceptText: qsTr("OK")
    property string dismissText: ""
    property real maxWidth: 420
    property bool closeOnScrim: true

    default property alias content: _content.data

    signal accepted()
    signal dismissed()
    signal closed()

    function _claim(item) {
        item.forceActiveFocus(AppTheme.remoteNavigation ? Qt.TabFocusReason
                                                        : Qt.OtherFocusReason)
    }

    function open() {
        opened = true
        PopupRegistry.register(root)
        root._claim(_accept)
    }

    function close() {
        if (!opened)
            return
        opened = false
        PopupRegistry.unregister(root)
        closed()
    }

    function accept() {
        accepted()
        close()
    }

    function dismiss() {
        dismissed()
        close()
    }

    anchors.fill: parent
    visible: opened || _panel.opacity > 0.01

    Keys.onPressed: (event) => {
        if (!root.opened)
            return
        switch (event.key) {
        case Qt.Key_Up:
        case Qt.Key_Down:
        case Qt.Key_Left:
        case Qt.Key_Right:
            event.accepted = true
            break
        }
    }

    Component.onDestruction: PopupRegistry.unregister(root)

    ModalScrim {
        id: _scrim

        shown: root.opened
        onClicked: {
            if (root.closeOnScrim)
                root.close()
        }
    }

    Rectangle {
        id: _panel

        anchors.centerIn: parent
        width: Math.min(root.width - 2 * AppTheme.spacing20, root.maxWidth)
        height: _column.implicitHeight + 2 * AppTheme.spacing24
        radius: AppTheme.radiusSheet
        color: AppTheme.surfaceRaised
        border.width: 1
        border.color: AppTheme.outline
        opacity: root.opened ? 1 : 0
        scale: root.opened ? 1 : 0.96

        Accessible.role: Accessible.Dialog
        Accessible.name: root.title
        Accessible.description: root.message

        Behavior on opacity {
            NumberAnimation { duration: 140 }
        }

        Behavior on scale {
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            onWheel: (wheel) => { wheel.accepted = true }
        }

        Column {
            id: _column

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppTheme.spacing24
            spacing: AppTheme.spacing10

            Text {
                width: parent.width
                visible: root.title.length > 0
                text: root.title
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fs22
                font.weight: Font.Medium
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                visible: root.message.length > 0
                text: root.message
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs13
                lineHeight: 1.5
                lineHeightMode: Text.ProportionalHeight
                wrapMode: Text.Wrap
            }

            Item {
                id: _content

                width: parent.width
                height: childrenRect.height
                visible: childrenRect.height > 0
            }

            Item {
                width: 1
                height: AppTheme.spacing10
            }

            Row {
                anchors.right: parent.right
                spacing: AppTheme.spacing8

                AppButton {
                    id: _dismiss

                    visible: root.dismissText.length > 0
                    text: root.dismissText
                    variant: AppButton.Outlined
                    size: AppButton.Medium
                    Keys.onRightPressed: root._claim(_accept)
                    onClicked: root.dismiss()
                }

                AppButton {
                    id: _accept

                    text: root.acceptText
                    variant: AppButton.Filled
                    size: AppButton.Medium
                    focus: root.opened
                    Keys.onLeftPressed: {
                        if (_dismiss.visible)
                            root._claim(_dismiss)
                    }
                    onClicked: root.accept()
                }
            }
        }
    }
}
