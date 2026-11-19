pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

FocusScope {
    id: root

    property string title
    property string text: ""
    property bool opened: false

    property var suggestions: []
    property string suggestionsTitle: ""

    signal accepted(string text)
    signal cancelled()
    signal suggestionChosen(var item)

    readonly property bool hasSuggestions: !!root.suggestions && root.suggestions.length > 0
    readonly property bool inStrip: root.row >= root.layout.length
    property int stripIndex: 0

    onSuggestionsChanged: {
        if (!root.hasSuggestions) {
            if (root.inStrip)
                root.row = root.layout.length - 1
            root.stripIndex = 0
            return
        }
        root.stripIndex = Math.min(root.stripIndex, root.suggestions.length - 1)
    }

    function letters(text) {
        const cells = []
        for (let i = 0; i < text.length; ++i)
            cells.push({ label: text.charAt(i), what: "" })
        return cells
    }

    readonly property var layout: [
        letters("ABCDEFGHIJ"),
        letters("KLMNOPQRST"),
        letters("UVWXYZ0123"),
        letters("456789'-.:"),
        [
            { label: qsTr("Space"), what: "space" },
            { label: qsTr("Delete"), what: "delete" },
            { label: qsTr("Clear"), what: "clear" },
            { label: qsTr("Search"), what: "done" }
        ]
    ]

    property int row: 0
    property int column: 0

    readonly property var cursorCell: {
        const cells = layout[Math.min(row, layout.length - 1)]
        return cells[Math.min(column, cells.length - 1)]
    }

    function open(withTitle, withText) {
        title = withTitle
        text = withText || ""
        row = 0
        column = 0
        stripIndex = 0
        opened = true
        PopupRegistry.register(root)
        forceActiveFocus()
        Qt.callLater(root.forceActiveFocus)
    }

    onOpenedChanged: {
        if (opened)
            Qt.callLater(root.forceActiveFocus)
    }

    function close() {
        opened = false
        PopupRegistry.unregister(root)
    }

    function cancel() {
        root.close()
        root.cancelled()
    }

    function press() {
        if (root.inStrip) {
            const item = root.suggestions[root.stripIndex]
            if (!item)
                return
            root.close()
            root.suggestionChosen(item)
            return
        }

        const cell = root.cursorCell
        switch (cell.what) {
        case "space":
            root.text += " "
            return
        case "delete":
            root.text = root.text.slice(0, -1)
            return
        case "clear":
            root.text = ""
            return
        case "done":
            root.close()
            root.accepted(root.text)
            return
        default:
            root.text += cell.label
        }
    }

    function step(dRow, dColumn) {
        if (root.inStrip) {
            if (dRow < 0) {
                row = layout.length - 1
                column = Math.min(layout[row].length - 1, column)
            } else if (dColumn !== 0) {
                stripIndex = Math.max(0, Math.min(root.suggestions.length - 1,
                                                  stripIndex + dColumn))
                _strip.positionViewAtIndex(stripIndex, ListView.Contain)
            }
            return
        }

        if (dRow > 0 && row === layout.length - 1 && root.hasSuggestions) {
            row = layout.length
            stripIndex = Math.min(stripIndex, root.suggestions.length - 1)
            _strip.positionViewAtIndex(stripIndex, ListView.Contain)
            return
        }

        if (dRow !== 0) {
            const nextRow = Math.max(0, Math.min(layout.length - 1, row + dRow))
            if (nextRow !== row) {
                const from = layout[row].length
                const to = layout[nextRow].length
                column = from === to
                         ? Math.min(to - 1, column)
                         : Math.min(to - 1,
                                    Math.round(column * (to - 1)
                                               / Math.max(1, from - 1)))
                row = nextRow
            }
            return
        }

        column = Math.max(0, Math.min(layout[row].length - 1, column + dColumn))
    }

    anchors.fill: parent
    visible: opened
    focus: opened

    Component.onDestruction: PopupRegistry.unregister(root)

    Keys.onLeftPressed: root.step(0, -1)
    Keys.onRightPressed: root.step(0, 1)
    Keys.onUpPressed: root.step(-1, 0)
    Keys.onDownPressed: root.step(1, 0)
    Keys.onEscapePressed: root.cancel()
    Keys.onBackPressed: (event) => {
        root.cancel()
        event.accepted = true
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Select:
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            root.press()
            event.accepted = true
            return
        case Qt.Key_Backspace:
            root.text = root.text.slice(0, -1)
            event.accepted = true
            return

        case Qt.Key_Left:
            root.step(0, -1)
            event.accepted = true
            return
        case Qt.Key_Right:
            root.step(0, 1)
            event.accepted = true
            return
        case Qt.Key_Up:
            root.step(-1, 0)
            event.accepted = true
            return
        case Qt.Key_Down:
            root.step(1, 0)
            event.accepted = true
            return
        }

        if (event.text.length === 1 && event.text >= " ") {
            root.text += event.text
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: AppTheme.overlay

        MouseArea {
            anchors.fill: parent
            onClicked: root.cancel()
        }
    }

    Rectangle {
        id: _panel

        anchors.centerIn: parent
        width: Math.min(parent.width * 0.68, 600)
        height: _column.implicitHeight + 2 * AppTheme.spacing14
        radius: AppTheme.radiusLarge

        readonly property real naturalKeyHeight:
            Math.round((_column.width - 9 * AppTheme.spacing4) / 10 * 0.85)
        readonly property real fixedHeight:
            2 * AppTheme.spacing14
            + _titleText.implicitHeight
            + AppTheme.controlHeightMedium
            + AppTheme.controlHeightMedium + AppTheme.spacing4
            + (root.hasSuggestions ? _stripTitle.implicitHeight + _strip.height : 0)
            + _column.spacing * (root.hasSuggestions ? 8 : 6)
        readonly property real letterKeyHeight:
            Math.max(24, Math.min(naturalKeyHeight,
                                  Math.floor((root.height - 2 * AppTheme.spacing16
                                              - fixedHeight) / 4)))
        color: AppTheme.surface
        border.width: 1
        border.color: AppTheme.outline

        Column {
            id: _column

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppTheme.spacing14
            spacing: AppTheme.spacing8

            Text {
                id: _titleText

                width: parent.width
                text: root.title
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs12
                elide: Text.ElideRight
            }

            Rectangle {
                width: parent.width
                height: AppTheme.controlHeightMedium
                radius: AppTheme.radiusPill
                color: AppTheme.surfaceVariant

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: AppTheme.spacing16
                    anchors.rightMargin: AppTheme.spacing16
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.text + "▏"
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fs17
                    elide: Text.ElideLeft
                }
            }

            Repeater {
                model: root.layout

                delegate: Row {
                    id: _keyRow

                    required property int index
                    required property var modelData

                    readonly property bool actionRow:
                        _keyRow.modelData.length > 0
                        && _keyRow.modelData[0].what !== ""
                    readonly property real cell:
                        (_column.width - (_keyRow.modelData.length - 1)
                         * AppTheme.spacing4) / _keyRow.modelData.length

                    width: _column.width
                    spacing: AppTheme.spacing4
                    topPadding: _keyRow.actionRow ? AppTheme.spacing4 : 0

                    Repeater {
                        model: _keyRow.modelData

                        delegate: Rectangle {
                            id: _key

                            required property int index
                            required property var modelData

                            readonly property bool here:
                                root.row === _keyRow.index
                                && root.column === _key.index

                            width: _keyRow.cell
                            height: _keyRow.actionRow
                                    ? AppTheme.controlHeightMedium
                                    : _panel.letterKeyHeight
                            radius: _keyRow.actionRow ? AppTheme.radiusPill
                                                      : AppTheme.radiusSmall
                            color: {
                                if (_key.here)
                                    return AppTheme.primary
                                if (_key.modelData.what === "done")
                                    return AppTheme.primaryContainer
                                return AppTheme.surfaceVariant
                            }
                            border.width: _key.here ? 2 : 0
                            border.color: AppTheme.focus

                            Text {
                                anchors.centerIn: parent
                                text: _key.modelData.label
                                color: {
                                    if (_key.here)
                                        return AppTheme.onPrimaryStrong
                                    if (_key.modelData.what === "done")
                                        return AppTheme.onPrimaryContainerStrong
                                    return AppTheme.textPrimary
                                }
                                font.pixelSize: _keyRow.actionRow ? AppTheme.fs14
                                                                  : AppTheme.fs17
                                font.weight: _key.here || _keyRow.actionRow
                                             ? Font.Medium : Font.Normal
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.row = _keyRow.index
                                    root.column = _key.index
                                    root.press()
                                }
                            }
                        }
                    }
                }
            }

            Text {
                id: _stripTitle

                width: parent.width
                visible: root.hasSuggestions
                topPadding: AppTheme.spacing2
                text: root.suggestionsTitle
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs12
                elide: Text.ElideRight
            }

            ListView {
                id: _strip

                readonly property int cardWidth: 190
                readonly property int cardHeight: 52

                width: parent.width
                height: root.hasSuggestions ? cardHeight + 6 : 0
                visible: root.hasSuggestions
                orientation: ListView.Horizontal
                spacing: AppTheme.spacing8
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: !AppTheme.remoteNavigation
                keyNavigationEnabled: false
                model: root.suggestions
                topMargin: 3
                leftMargin: 3
                rightMargin: 3

                delegate: SuggestionCard {
                    id: _suggestion

                    required property int index
                    required property var modelData

                    compact: true
                    cardWidth: _strip.cardWidth
                    compactHeight: _strip.cardHeight
                    item: _suggestion.modelData
                    highlighted: root.inStrip && root.stripIndex === _suggestion.index

                    onClicked: {
                        root.row = root.layout.length
                        root.stripIndex = _suggestion.index
                        root.press()
                    }
                }
            }
        }
    }
}
