pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons" as S
import "../Controls" as Ctrl

Item {
    id: root
    readonly property var iconNames: [
        "menu", "search", "continueWatching", "all", "folder", "movies",
        "tvShows", "settings", "chevronLeft", "chevronRight", "moreVertical",
        "play", "pause", "check", "close", "plus", "lock", "volume",
        "skipBack", "skipForward", "subtitles", "appearance", "alertCircle",
        "noVideo", "recentlyAdded", "rescan", "makimediaMark",
        "suggested", "unmatched", "identifyShows"
    ]

    function iconFor(name) {
        return S.Icons[name]
    }

    Flickable {
        anchors.fill: parent
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        contentHeight: _column.implicitHeight + S.AppTheme.spacing32
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Column {
            id: _column

            width: parent.width - S.AppTheme.scrollBarWidth
            spacing: 0

            Ctrl.SectionLabel {
                text: qsTr("AppButton, Large")
                topPadding: S.AppTheme.spacing12
            }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton { text: "Standard" }
                Ctrl.AppButton { text: "Filled"; variant: Ctrl.AppButton.Filled }
                Ctrl.AppButton { text: "Tonal"; variant: Ctrl.AppButton.Tonal }
                Ctrl.AppButton { text: "Outlined"; variant: Ctrl.AppButton.Outlined }
                Ctrl.AppButton { text: "Plain"; variant: Ctrl.AppButton.Plain }
                Ctrl.AppButton { text: "Disabled"; enabled: false }
            }

            Ctrl.SectionLabel { text: qsTr("AppButton, Medium and icons") }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton {
                    text: "Medium"
                    size: Ctrl.AppButton.Medium
                }

                Ctrl.AppButton {
                    text: "With icon"
                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Filled
                    iconSource: S.Icons.play
                }

                Ctrl.AppButton {
                    text: "Outlined icon"
                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Outlined
                    iconSource: S.Icons.check
                }

                Ctrl.AppButton {
                    iconOnly: true
                    size: Ctrl.AppButton.Medium
                    variant: Ctrl.AppButton.Outlined
                    iconSource: S.Icons.check
                    accessibleName: "Icon only"
                }
            }

            Ctrl.SectionLabel { text: qsTr("IconButton") }

            Row {
                spacing: S.AppTheme.spacing8

                Ctrl.IconButton {
                    iconSource: S.Icons.play
                    accessibleName: "Default"
                }

                Ctrl.IconButton {
                    compact: true
                    iconSource: S.Icons.play
                    accessibleName: "Compact"
                }

                Ctrl.IconButton {
                    compact: true
                    iconSource: S.Icons.close
                    tintColor: S.AppTheme.error
                    accessibleName: "Tinted"
                }

                Ctrl.IconButton {
                    iconSource: S.Icons.play
                    enabled: false
                    accessibleName: "Disabled"
                }
            }

            Ctrl.SectionLabel { text: qsTr("Chip") }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing6

                Ctrl.Chip { text: "Default" }
                Ctrl.Chip { text: "Selected"; selected: true }
                Ctrl.Chip { text: "Disabled"; enabled: false }
                Ctrl.Chip { compact: true; text: "Compact" }
                Ctrl.Chip { compact: true; text: "Compact selected"; selected: true }
            }

            Ctrl.SectionLabel { text: qsTr("ToolChip, on the player bar") }

            Rectangle {
                width: parent.width
                height: _toolChips.implicitHeight + 2 * S.AppTheme.spacing12
                radius: S.AppTheme.radiusMedium
                color: "black"

                Row {
                    id: _toolChips

                    anchors.centerIn: parent
                    spacing: S.AppTheme.spacing8

                    Ctrl.ToolChip { text: "Default" }
                    Ctrl.ToolChip { text: "Active"; active: true }
                    Ctrl.ToolChip { text: "Disabled"; enabled: false }
                }
            }

            Ctrl.SectionLabel { text: qsTr("ToggleSwitch and RadioIndicator") }

            Row {
                spacing: S.AppTheme.spacing16

                Ctrl.ToggleSwitch { checked: true }
                Ctrl.ToggleSwitch { checked: false }
                Ctrl.ToggleSwitch { checked: true; enabled: false }

                Ctrl.RadioIndicator {
                    anchors.verticalCenter: parent.verticalCenter
                    selected: true
                }

                Ctrl.RadioIndicator {
                    anchors.verticalCenter: parent.verticalCenter
                    selected: false
                }
            }

            Ctrl.SectionLabel { text: qsTr("SectionLabel, Divider, RowHeader") }

            Ctrl.RowHeader {
                width: parent.width
                title: "Row header"
                countText: "12"
            }

            Ctrl.Divider { width: parent.width }

            Ctrl.Divider {
                width: parent.width
                inset: 88
            }

            Ctrl.SectionLabel { text: qsTr("Card and KeyValueRow") }

            Ctrl.Card {
                width: parent.width
                implicitHeight: _cardA.implicitHeight + 2 * S.AppTheme.spacing14

                Column {
                    id: _cardA

                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: 0

                    Ctrl.KeyValueRow {
                        width: parent.width
                        label: "Plain card"
                        value: "surface"
                    }

                    Ctrl.KeyValueRow {
                        width: parent.width
                        label: "Mono value"
                        value: "F:/Downloads/Some.File.Name.mkv"
                        mono: true
                    }
                }
            }

            Item {
                width: 1
                height: S.AppTheme.spacing8
            }

            Ctrl.Card {
                width: parent.width
                variant: true
                implicitHeight: _cardB.implicitHeight + 2 * S.AppTheme.spacing14

                Ctrl.KeyValueRow {
                    id: _cardB

                    anchors.left: parent.left
                    anchors.right: parent.right
                    label: "Variant card"
                    value: "surfaceVariant"
                }
            }

            Ctrl.SectionLabel { text: qsTr("ListRow") }

            Ctrl.ListRow {
                width: parent.width
                title: "No leading"
                subtitle: "Plain row with a subtitle"
            }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.folder
                title: "Icon leading"
                subtitle: "With a trailing value"
                trailingText: "12"
            }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Thumbnail
                title: "Some.Release.Name.1080p.mkv"
                monoTitle: true
                subtitle: "Thumbnail leading, part watched"
                trailingText: "42:11"
                progress: 0.45
            }

            Ctrl.ListRow {
                width: parent.width
                leading: Ctrl.ListRow.Icon
                iconSource: S.Icons.folder
                title: "Dimmed"
                subtitle: "Unavailable, files kept"
                dimmed: true
            }

            Ctrl.SectionLabel { text: qsTr("SettingsRow") }

            Ctrl.SettingsRow {
                width: parent.width
                iconSource: S.Icons.appearance
                title: "Switch trailing"
                subtitle: "With an icon"
                trailing: Ctrl.SettingsRow.Switch
                switchChecked: true
            }

            Ctrl.SettingsRow {
                width: parent.width
                title: "Chevron trailing"
                trailing: Ctrl.SettingsRow.Chevron
            }

            Ctrl.SettingsRow {
                width: parent.width
                title: "Value trailing"
                subtitle: "Shows the current setting"
                trailing: Ctrl.SettingsRow.Value
                valueText: "10s"
            }

            Ctrl.SectionLabel { text: qsTr("SheetRow") }

            Ctrl.SheetRow {
                width: parent.width
                title: "Radio, selected"
                subtitle: "AAC · embedded"
                selected: true
            }

            Ctrl.SheetRow {
                width: parent.width
                title: "Radio, unselected"
                subtitle: "EAC3 · external file"
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.play
                title: "Icon row"
            }

            Ctrl.SheetRow {
                width: parent.width
                showRadio: false
                iconSource: S.Icons.close
                title: "Destructive"
                destructive: true
            }

            Ctrl.SectionLabel { text: qsTr("SearchBar") }

            Ctrl.SearchBar {
                width: parent.width
                placeholder: "Search filenames"
            }

            Ctrl.SectionLabel { text: qsTr("LinearProgress") }

            Column {
                width: parent.width
                spacing: S.AppTheme.spacing12

                Ctrl.LinearProgress { width: parent.width; value: 0.15 }
                Ctrl.LinearProgress { width: parent.width; value: 0.6 }
                Ctrl.LinearProgress { width: parent.width; value: 1.0 }
                Ctrl.LinearProgress { width: parent.width; indeterminate: true }
            }

            Ctrl.SectionLabel { text: qsTr("SeekBar") }

            Rectangle {
                width: parent.width
                height: 60
                radius: S.AppTheme.radiusMedium
                color: "black"

                Ctrl.SeekBar {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: S.AppTheme.spacing16
                    anchors.verticalCenter: parent.verticalCenter
                    position: 620
                    duration: 2400
                }
            }

            Ctrl.SectionLabel { text: qsTr("MediaTile, poster") }

            Row {
                spacing: S.AppTheme.spacing12

                Ctrl.MediaTile {
                    unmatched: true
                    title: "Unmatched"
                    meta: "Unmatched"
                }

                Ctrl.MediaTile {
                    unmatched: true
                    title: "Part watched"
                    meta: "18:20 left"
                    progress: 0.4
                }

                Ctrl.MediaTile {
                    unmatched: true
                    title: "Seen"
                    meta: "Unmatched"
                    badge: Ctrl.MediaTile.Positive
                    badgeText: "Seen"
                }

                Ctrl.MediaTile {
                    unmatched: true
                    title: "Missing"
                    meta: "Missing"
                    badge: Ctrl.MediaTile.Negative
                    badgeText: "Missing"
                }

                Ctrl.MediaTile {
                    unmatched: true
                    title: "Accent badge"
                    meta: "Unmatched"
                    badge: Ctrl.MediaTile.Accent
                    badgeText: "4K"
                }
            }

            Ctrl.SectionLabel { text: qsTr("MediaTile, wide") }

            Row {
                spacing: S.AppTheme.spacing12

                Ctrl.MediaTile {
                    shape: Ctrl.MediaTile.Wide
                    unmatched: true
                    title: "Wide unmatched"
                    meta: "Unmatched"
                }

                Ctrl.MediaTile {
                    shape: Ctrl.MediaTile.Wide
                    unmatched: true
                    title: "Wide, part watched"
                    meta: "15:40 left"
                    progress: 0.62
                    badge: Ctrl.MediaTile.Neutral
                    badgeText: "S02E05"
                }
            }

            Ctrl.SectionLabel { text: qsTr("PlayerButton") }

            Rectangle {
                width: parent.width
                height: 120
                radius: S.AppTheme.radiusMedium
                color: "black"

                Row {
                    anchors.centerIn: parent
                    spacing: S.AppTheme.spacing24

                    Ctrl.PlayerButton {
                        anchors.verticalCenter: parent.verticalCenter
                        iconSource: S.Icons.skipBack
                        accessibleName: "Skip back"
                    }

                    Ctrl.PlayerButton {
                        anchors.verticalCenter: parent.verticalCenter
                        primary: true
                        iconSource: S.Icons.play
                        accessibleName: "Play"
                    }

                    Ctrl.PlayerButton {
                        anchors.verticalCenter: parent.verticalCenter
                        iconSource: S.Icons.skipForward
                        accessibleName: "Skip forward"
                    }
                }
            }

            Ctrl.SectionLabel { text: qsTr("EmptyState") }

            Ctrl.EmptyState {
                width: parent.width
                iconSource: S.Icons.folder
                title: "Nothing indexed yet"
                message: "Add a folder and Makimedia will index what is inside it."
                actionText: "Add folder"
            }

            Ctrl.EmptyState {
                width: parent.width
                busy: true
                title: "Working"
                message: "The busy variant, with no action."
            }

            Ctrl.SectionLabel { text: qsTr("KeyHints") }

            Ctrl.KeyHints {
                width: parent.width
                hints: [
                    { keys: "Space", description: "Play or pause" },
                    { keys: "J L", description: "Seek 10 seconds" },
                    { keys: "[ ]", description: "Playback speed" }
                ]
            }

            Ctrl.SectionLabel { text: qsTr("NavItem") }

            Ctrl.Card {
                width: parent.width
                variant: true
                implicitHeight: _navItems.implicitHeight + 2 * S.AppTheme.spacing14

                Column {
                    id: _navItems

                    anchors.left: parent.left
                    anchors.right: parent.right
                    spacing: S.AppTheme.spacing4

                    Ctrl.NavItem {
                        width: parent.width
                        iconSource: S.Icons.continueWatching
                        text: "Selected"
                        selected: true
                        countText: "5"
                    }

                    Ctrl.NavItem {
                        width: parent.width
                        iconSource: S.Icons.all
                        text: "Normal"
                        countText: "260"
                    }

                    Ctrl.NavItem {
                        width: parent.width
                        iconSource: S.Icons.movies
                        text: "Muted"
                        muted: true
                    }
                }
            }

            Ctrl.SectionLabel { text: qsTr("Overlays, tap to show") }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing8

                Ctrl.AppButton {
                    text: "Snackbar"
                    size: Ctrl.AppButton.Medium
                    onClicked: _snackbar.show("A snackbar message", "Undo")
                }

                Ctrl.AppButton {
                    text: "Dialog"
                    size: Ctrl.AppButton.Medium
                    onClicked: _dialog.open()
                }

                Ctrl.AppButton {
                    text: "Bottom sheet"
                    size: Ctrl.AppButton.Medium
                    onClicked: _sheet.open()
                }

                Ctrl.AppButton {
                    text: "Sort and filter"
                    size: Ctrl.AppButton.Medium
                    onClicked: _sortSheet.openOptions()
                }

                Ctrl.AppButton {
                    text: "Centre OSD"
                    size: Ctrl.AppButton.Medium
                    onClicked: _centreOsd.flash("+10s", "0:42:18")
                }

                Ctrl.AppButton {
                    text: "Meter OSD"
                    size: Ctrl.AppButton.Medium
                    onClicked: _sideOsd.flash("64%", "Volume", 0.64)
                }
            }

            Ctrl.SectionLabel { text: qsTr("Icons") }

            Flow {
                width: parent.width
                spacing: S.AppTheme.spacing12

                Repeater {
                    model: root.iconNames

                    delegate: Column {
                        id: _iconCell

                        required property string modelData

                        width: 76
                        spacing: S.AppTheme.spacing4

                        Ctrl.ThemedIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 24
                            height: 24
                            source: root.iconFor(_iconCell.modelData)
                            tintColor: S.AppTheme.textPrimary
                        }

                        Text {
                            width: parent.width
                            text: _iconCell.modelData
                            color: S.AppTheme.textDisabled
                            font.pixelSize: S.AppTheme.fs9
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            Item {
                width: 1
                height: S.AppTheme.spacing24
            }
        }
    }

    Ctrl.PlayerOsd {
        id: _centreOsd

        anchors.centerIn: parent
        z: 60
    }

    Ctrl.PlayerOsd {
        id: _sideOsd

        anchors.right: parent.right
        anchors.rightMargin: S.AppTheme.spacing24
        anchors.verticalCenter: parent.verticalCenter
        z: 60
    }

    Ctrl.Snackbar {
        id: _snackbar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: S.AppTheme.spacing16
        z: 50
    }

    Ctrl.AppDialog {
        id: _dialog

        z: 70
        title: "Dialog title"
        message: "The body text of a dialog, long enough to wrap onto a second line so the spacing can be judged."
        dismissText: "Cancel"
        acceptText: "Confirm"
    }

    Ctrl.BottomSheet {
        id: _sheet

        z: 70
        title: "Bottom sheet"

        Column {
            width: parent.width
            spacing: 0

            Repeater {
                model: 8

                delegate: Ctrl.SheetRow {
                    required property int index

                    width: parent.width
                    title: "Row " + (index + 1)
                    subtitle: "Scroll to see the sheet cap and the scroll bar"
                    selected: index === 0
                }
            }
        }
    }

    Ctrl.SortFilterSheet {
        id: _sortSheet

        z: 70
    }
}
