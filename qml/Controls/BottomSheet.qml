pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import com.topicdev.makimedia 1.0
import "../Singletons"

Item {
    id: root

    property string title
    property bool opened: false
    property real maxHeightFraction: 0.82

    readonly property real keyboardHeight: {
        const bottom = root.mapToItem(null, 0, root.height).y

        const told = System.keyboardHeight
        if (told > 64 && Window.height > 0)
            return Math.max(0, Math.min(root.height, told - (Window.height - bottom)))

        if (!Qt.inputMethod.visible)
            return 0

        const keyboard = Qt.inputMethod.keyboardRectangle
        if (!keyboard || keyboard.height <= 0)
            return 0

        return Math.max(0, Math.min(root.height, bottom - keyboard.y))
    }

    readonly property real maxPanelHeight:
        keyboardHeight > 0
        ? Math.max(0, height - keyboardHeight - AppTheme.spacing12)
        : height * maxHeightFraction

    onKeyboardHeightChanged: {
        if (!root.opened)
            return

        Library.noteUi("sheet keyboard " + Math.round(System.keyboardHeight)
                       + " qt " + Math.round(Qt.inputMethod.keyboardRectangle.height)
                       + " window " + Math.round(Window.height)
                       + " sheet height " + Math.round(root.height)
                       + " lifted by " + Math.round(root.keyboardHeight))

        if (root.keyboardHeight > 0)
            Qt.callLater(root.showTypedInto)
    }

    function showTypedInto() {
        const item = Window.activeFocusItem
        for (let walk = item; walk; walk = walk.parent) {
            if (walk === _content) {
                root.showFocused(item)
                return
            }
        }
    }

    readonly property real headerTopMargin: AppTheme.spacing12
    readonly property real contentTopMargin: AppTheme.spacing10
    readonly property real contentBottomMargin: AppTheme.spacing16

    default property alias content: _content.data
    property alias pinned: _pinned.data

    signal closed()

    property Item focusBefore: null

    property bool restoresFocus: true

    function open() {
        if (AppTheme.remoteNavigation)
            focusBefore = Window.activeFocusItem

        opened = true
        PopupRegistry.register(root)

        if (AppTheme.remoteNavigation)
            _focusFirst.restart()
    }

    function focusableRows(item, found) {
        if (!item)
            return found

        for (let i = 0; i < item.children.length; ++i) {
            const child = item.children[i]
            if (!child.visible || !child.enabled)
                continue
            if (child.activeFocusOnTab === true)
                found.push(child)
            else
                root.focusableRows(child, found)
        }
        return found
    }

    function focusFirstRow(item) {
        const rows = root.focusableRows(item, [])
        if (rows.length === 0)
            return false
        rows[0].forceActiveFocus(Qt.TabFocusReason)
        _flick.contentY = 0
        return true
    }

    function focusGrid() {
        const items = root.focusableRows(_content, [])
        const rows = []

        for (let i = 0; i < items.length; ++i) {
            const item = items[i]
            const centre = item.mapToItem(_content, item.width / 2, item.height / 2)
            const tolerance = Math.max(8, item.height / 2)

            let row = null
            for (let r = 0; r < rows.length; ++r) {
                if (Math.abs(rows[r].y - centre.y) < tolerance) {
                    row = rows[r]
                    break
                }
            }

            if (!row) {
                row = { y: centre.y, items: [] }
                rows.push(row)
            }
            row.items.push({ item: item, x: centre.x })
        }

        rows.sort((a, b) => a.y - b.y)
        for (let r = 0; r < rows.length; ++r)
            rows[r].items.sort((a, b) => a.x - b.x)

        return rows
    }

    function showFocused(item) {
        _focusScroll.reveal(item)
    }

    FocusScroller {
        id: _focusScroll

        owner: root
        flickable: _flick
        content: _content
        margin: AppTheme.spacing12
    }

    function locateFocus(rows) {
        const current = Window.activeFocusItem
        for (let r = 0; r < rows.length; ++r) {
            for (let c = 0; c < rows[r].items.length; ++c) {
                if (rows[r].items[c].item === current)
                    return { row: r, column: c }
            }
        }
        return null
    }

    function stepVertical(forward) {
        const rows = root.focusGrid()
        if (rows.length === 0)
            return

        const at = root.locateFocus(rows)
        if (!at) {
            rows[0].items[0].item.forceActiveFocus(Qt.TabFocusReason)
            return
        }

        const next = at.row + (forward ? 1 : -1)
        if (next < 0 || next >= rows.length)
            return

        const column = Math.min(at.column, rows[next].items.length - 1)
        const target = rows[next].items[column].item
        target.forceActiveFocus(Qt.TabFocusReason)
        root.showFocused(target)
    }

    function stepHorizontal(forward) {
        const rows = root.focusGrid()
        const at = root.locateFocus(rows)
        if (!at)
            return

        const next = at.column + (forward ? 1 : -1)
        if (next < 0 || next >= rows[at.row].items.length)
            return

        const target = rows[at.row].items[next].item
        target.forceActiveFocus(Qt.TabFocusReason)
        root.showFocused(target)
    }

    Keys.onUpPressed: root.stepVertical(false)
    Keys.onDownPressed: root.stepVertical(true)
    Keys.onLeftPressed: root.stepHorizontal(false)
    Keys.onRightPressed: root.stepHorizontal(true)
    Keys.onEscapePressed: root.close()

    Keys.onBackPressed: (event) => {
        root.close()
        event.accepted = true
    }

    Timer {
        id: _focusFirst

        interval: 180
        onTriggered: root.focusFirstRow(_content)
    }

    function close() {
        opened = false
        PopupRegistry.unregister(root)
        closed()

        if (AppTheme.remoteNavigation && restoresFocus)
            _restoreFocus.restart()
    }

    Timer {
        id: _restoreFocus

        interval: 180
        onTriggered: {
            if (root.focusBefore && root.focusBefore.visible
                    && root.focusBefore.enabled) {
                root.focusBefore.forceActiveFocus(Qt.TabFocusReason)
            }
            root.focusBefore = null
        }
    }

    anchors.fill: parent
    visible: opened || _panel.slide < 1

    Component.onDestruction: PopupRegistry.unregister(root)

    ModalScrim {
        id: _scrim

        shown: root.opened
        fadeDuration: 160
        onClicked: root.close()
    }

    Rectangle {
        id: _panel

        readonly property real contentHeight: _content.childrenRect.height
        property real slide: root.opened ? 0 : 1

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.keyboardHeight
        height: Math.min(root.maxPanelHeight,
                         root.headerTopMargin + _header.height
                         + root.contentTopMargin + contentHeight
                         + root.contentBottomMargin)
        topLeftRadius: AppTheme.radiusSheet
        topRightRadius: AppTheme.radiusSheet
        color: AppTheme.surface

        transform: Translate {
            y: _panel.height * _panel.slide
        }

        Behavior on slide {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            onWheel: (wheel) => { wheel.accepted = true }
        }

        Column {
            id: _header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: AppTheme.spacing16
            anchors.rightMargin: AppTheme.spacing12
            anchors.topMargin: root.headerTopMargin
            spacing: AppTheme.spacing10

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 36
                height: 4
                radius: AppTheme.radiusPill
                color: AppTheme.outlineStrong
            }

            Item {
                width: parent.width
                height: Math.max(_titleLabel.implicitHeight, _close.height)

                Text {
                    id: _titleLabel

                    anchors.left: parent.left
                    anchors.right: _close.left
                    anchors.rightMargin: AppTheme.spacing8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.title.length > 0
                    text: root.title
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fs17
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }

                IconButton {
                    id: _close

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    compact: true
                    iconSource: Icons.close
                    accessibleName: qsTr("Close")
                    onClicked: root.close()
                }
            }

            Column {
                id: _pinned

                width: parent.width
                spacing: AppTheme.spacing8
            }
        }

        Flickable {
            id: _flick

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _header.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppTheme.spacing16
            anchors.rightMargin: AppTheme.spacing16
            anchors.topMargin: root.contentTopMargin
            anchors.bottomMargin: root.contentBottomMargin
            contentHeight: _content.childrenRect.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: AppScrollBar {}

            Item {
                id: _content

                width: _flick.width - AppTheme.scrollBarWidth
                height: childrenRect.height
            }
        }
    }
}
