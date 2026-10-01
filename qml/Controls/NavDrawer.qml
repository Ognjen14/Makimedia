pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../Singletons"

Rectangle {
    id: root

    enum Mode {
        Modal,
        Rail,
        Permanent
    }

    property int mode: NavDrawer.Permanent
    property string currentKey: "home"
    property var items: null

    property bool iconsOnly: false

    readonly property bool rail: mode === NavDrawer.Rail
    readonly property bool modal: mode === NavDrawer.Modal
    readonly property int expandedWidth: 236
    readonly property int railWidth: iconsOnly ? 72 : 80
    readonly property int modalWidth: 300

    signal itemActivated(string key)
    signal exitRequested()

    readonly property int cursor: _list.currentIndex

    function selectable(i) {
        if (!items || i < 0 || i >= items.count)
            return false
        const entry = items.get(i)
        return entry.heading !== true && entry.muted !== true
    }

    function step(delta) {
        if (!items)
            return
        let i = _list.currentIndex + delta
        while (i >= 0 && i < items.count && !selectable(i))
            i += delta
        if (selectable(i))
            _list.currentIndex = i
    }

    function indexOfKey(key) {
        if (!items)
            return -1
        for (let i = 0; i < items.count; ++i) {
            if (items.get(i).key === key)
                return i
        }
        return -1
    }

    function takeFocus() {
        const at = indexOfKey(currentKey)
        if (at >= 0)
            _list.currentIndex = at
        _list.forceActiveFocus()
    }

    width: modal ? modalWidth : (rail ? railWidth : expandedWidth)
    color: AppTheme.surface

    onCurrentKeyChanged: {
        const at = indexOfKey(currentKey)
        if (at >= 0)
            _list.currentIndex = at
    }

    Component.onCompleted: {
        const at = indexOfKey(currentKey)
        if (at >= 0)
            _list.currentIndex = at
    }

    Connections {
        target: root.items

        function onCountChanged() {
            if (_list.activeFocus)
                return
            const at = root.indexOfKey(root.currentKey)
            if (at >= 0)
                _list.currentIndex = at
        }
    }

    topLeftRadius: modal ? 0 : 0
    topRightRadius: modal ? AppTheme.radiusSheet : 0
    bottomRightRadius: modal ? AppTheme.radiusSheet : 0

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        visible: !root.modal
        color: AppTheme.outline
    }

    Item {
        id: _brand

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.rail ? 48 : 60

        Item {
            id: _brandIcon

            anchors.left: root.rail ? undefined : parent.left
            anchors.leftMargin: AppTheme.spacing20
            anchors.horizontalCenter: root.rail ? parent.horizontalCenter
                                                : undefined
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28

            Image {
                id: _brandArt

                anchors.fill: parent
                source: Icons.appIcon
                sourceSize.width: 56
                sourceSize.height: 56
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: false
            }

            Rectangle {
                id: _brandMask

                anchors.fill: parent
                radius: AppTheme.radiusSmall
                color: AppTheme.textPrimary
                visible: false
            }

            OpacityMask {
                anchors.fill: parent
                source: _brandArt
                maskSource: _brandMask
            }
        }

        Text {
            anchors.left: _brandIcon.right
            anchors.leftMargin: AppTheme.spacing12
            anchors.right: parent.right
            anchors.rightMargin: AppTheme.spacing12
            anchors.verticalCenter: parent.verticalCenter
            visible: !root.rail
            text: qsTr("Makimedia")
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fs18
            font.weight: Font.Bold
            elide: Text.ElideRight
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: root.rail ? AppTheme.spacing16
                                          : AppTheme.spacing12
            anchors.rightMargin: root.rail ? AppTheme.spacing16
                                           : AppTheme.spacing12
            height: 1
            color: AppTheme.outline
        }
    }

    ListView {
        id: _list

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _brand.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: root.modal ? 0 : AppTheme.spacing12
        anchors.bottomMargin: AppTheme.spacing12
        clip: true
        interactive: contentHeight > height
        spacing: root.rail ? AppTheme.spacing4 : 0
        model: root.items

        keyNavigationEnabled: false
        currentIndex: 0

        Keys.onUpPressed: root.step(-1)
        Keys.onDownPressed: root.step(1)
        Keys.onRightPressed: root.exitRequested()

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Select
                || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter
                || event.key === Qt.Key_Space) {
                if (root.selectable(_list.currentIndex))
                    root.itemActivated(root.items.get(_list.currentIndex).key)
                event.accepted = true
            }
        }

        delegate: Item {
            id: _entry

            required property int index
            required property var model

            readonly property bool isSection: _entry.model.heading === true

            width: ListView.view.width
            height: isSection
                    ? (root.rail ? AppTheme.spacing8 : _section.implicitHeight)
                    : _item.implicitHeight

            SectionLabel {
                id: _section

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: AppTheme.spacing20
                anchors.rightMargin: AppTheme.spacing20
                visible: _entry.isSection && !root.rail
                text: _entry.model.label
                topPadding: AppTheme.spacing16
                bottomPadding: AppTheme.spacing6
            }

            NavItem {
                id: _item

                anchors.horizontalCenter: parent.horizontalCenter
                width: root.rail
                       ? (root.iconsOnly ? 56 : 64)
                       : parent.width - 2 * AppTheme.spacing12
                visible: !_entry.isSection
                rail: root.rail
                iconOnly: root.rail && root.iconsOnly
                text: _entry.model.label
                iconSource: _entry.model.icon
                countText: _entry.model.badge < 0 ? "" : String(_entry.model.badge)
                muted: _entry.model.muted === true
                enabled: _entry.model.muted !== true
                selected: !_entry.isSection
                          && _entry.model.key === root.currentKey
                highlighted: !_entry.isSection
                             && _entry.index === _list.currentIndex
                             && _list.activeFocus

                onClicked: {
                    _list.currentIndex = _entry.index
                    root.itemActivated(_entry.model.key)
                }
            }
        }
    }
}
