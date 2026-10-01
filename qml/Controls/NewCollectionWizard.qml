pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "../Singletons"
import com.topicdev.makimedia 1.0

Item {
    id: root

    property bool opened: false
    property int step: 1
    property string collectionName: ""
    property string description: ""
    property string coverMode: "mosaic"
    property var picks: []
    property var createdId: 0
    property bool reordering: false

    readonly property int lastStep: 3
    readonly property bool canLeaveNameStep: collectionName.trim().length > 0
    readonly property bool canLeavePickStep: picks.length >= 2
    readonly property bool wide: width >= 620

    readonly property int pickedRuntime: {
        let minutes = 0
        for (let i = 0; i < picks.length; ++i)
            minutes += picks[i].runtimeMinutes || 0
        return minutes
    }

    readonly property int pickedShows: {
        let shows = 0
        for (let i = 0; i < picks.length; ++i) {
            if (picks[i].kind === "tv")
                ++shows
        }
        return shows
    }

    signal cancelled()
    signal openRequested(var collectionId)

    function open() {
        step = 1
        createdId = 0
        collectionName = ""
        description = ""
        coverMode = "mosaic"
        picks = []
        Library.pickerQuery = ""
        Library.pickerKind = 0
        opened = true
        PopupRegistry.register(root)
        _nameField.forceActiveFocus()
    }

    function dropFocus() {
        _panel.forceActiveFocus()
    }

    function pickIndex(mediaId) {
        for (let i = 0; i < picks.length; ++i) {
            if (picks[i].mediaId === mediaId)
                return i
        }
        return -1
    }

    function togglePick(title) {
        dropFocus()
        const at = pickIndex(title.mediaId)
        const kept = picks.slice()
        if (at >= 0)
            kept.splice(at, 1)
        else
            kept.push(title)
        picks = kept
    }

    function clearPicks() {
        picks = []
    }

    function movePick(from, to) {
        if (from === to || from < 0 || to < 0 || from >= picks.length || to >= picks.length)
            return
        const kept = picks.slice()
        const moved = kept.splice(from, 1)[0]
        kept.splice(to, 0, moved)
        picks = kept
    }

    function removePick(at) {
        const kept = picks.slice()
        kept.splice(at, 1)
        picks = kept
        if (picks.length < 2)
            step = 2
    }

    function sortPicks(by) {
        const kept = picks.slice()
        kept.sort(function (a, b) {
            if (by === "year")
                return (a.year || 0) - (b.year || 0)
            return a.title.localeCompare(b.title)
        })
        picks = kept
    }

    function create() {
        if (step !== lastStep || picks.length < 2)
            return
        const ids = []
        for (let i = 0; i < picks.length; ++i)
            ids.push(picks[i].mediaId)

        const id = Library.createCollection(root.collectionName.trim(), root.description,
                                            root.coverMode, ids)
        if (id === 0)
            return
        createdId = id
        step = 4
    }

    function close() {
        if (!opened)
            return
        opened = false
        PopupRegistry.unregister(root)
    }

    function cancel() {
        close()
        cancelled()
    }

    function next() {
        if (step === 1 && !canLeaveNameStep)
            return
        if (step === 2 && !canLeavePickStep)
            return
        dropFocus()
        if (step < lastStep)
            step = step + 1
    }

    function back() {
        if (step === 4)
            return
        dropFocus()
        if (step > 1)
            step = step - 1
        else
            cancel()
    }

    function stepTitle(which) {
        switch (which) {
        case 1:
            return qsTr("New collection")
        case 2:
            return qsTr("Pick films")
        case 3:
            return qsTr("Order & review")
        default:
            return root.collectionName
        }
    }

    function stepHint(which) {
        switch (which) {
        case 1:
            return qsTr("Name it and choose a cover.")
        case 2:
            return qsTr("Tap to select. Numbers show the order you picked.")
        case 3:
            return root.collectionName
        default:
            return root.picks.length === 1 ? qsTr("1 film")
                                           : qsTr("%1 films").arg(root.picks.length)
        }
    }

    anchors.fill: parent
    visible: opened || _panel.opacity > 0.01

    Component.onDestruction: PopupRegistry.unregister(root)

    Keys.onPressed: (event) => {
        if (!root.opened)
            return
        if (event.key === Qt.Key_Escape || event.key === Qt.Key_Back) {
            root.back()
            event.accepted = true
        }
    }

    component StepMark: Row {
        id: _mark

        property int number: 1
        property int currentStep: 1
        property string label

        readonly property bool current: _mark.currentStep === _mark.number
        readonly property bool passed: _mark.currentStep > _mark.number

        spacing: AppTheme.spacing6

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 20
            height: 20
            radius: 10
            color: _mark.current ? AppTheme.textPrimary
                                 : (_mark.passed ? AppTheme.outline : "transparent")
            border.width: _mark.current || _mark.passed ? 0 : 1
            border.color: AppTheme.outlineStrong

            Text {
                anchors.centerIn: parent
                visible: !_mark.passed
                text: String(_mark.number)
                color: _mark.current ? AppTheme.background : AppTheme.textSecondary
                font.pixelSize: AppTheme.fs11
                font.weight: Font.Bold
            }

            ThemedIcon {
                anchors.centerIn: parent
                width: 12
                height: 12
                visible: _mark.passed
                source: Icons.check
                tintColor: AppTheme.textPrimary
                showPlaceholder: false
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: _mark.label
            color: _mark.current ? AppTheme.textPrimary : AppTheme.textSecondary
            font.pixelSize: AppTheme.fs12
        }
    }

    component FieldLabel: Text {
        color: AppTheme.textSecondary
        font.pixelSize: AppTheme.fs11
        font.weight: Font.Medium
        font.letterSpacing: 0.6
    }

    component Poster: Rectangle {
        id: _poster

        property string label
        property real labelSize: AppTheme.fs13
        property string posterPath: ""
        property int posterSize: 185
        property int sourceWidth: 185
        property int artworkStamp: 0

        radius: AppTheme.radiusSmall / 2
        color: AppTheme.surfaceVariant
        clip: true

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: AppTheme.surfaceRaised }
                GradientStop { position: 1.0; color: AppTheme.surface }
            }
        }

        Image {
            anchors.fill: parent
            source: {
                void _poster.artworkStamp
                return _poster.posterPath.length > 0
                       ? Metadata.posterUrl(_poster.posterPath, _poster.posterSize) : ""
            }
            sourceSize.width: _poster.sourceWidth
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: status === Image.Ready
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppTheme.spacing6
            anchors.rightMargin: AppTheme.spacing6
            anchors.bottomMargin: AppTheme.spacing8
            z: 1
            visible: _poster.label.length > 0
            text: _poster.label
            color: AppTheme.textPrimary
            font.pixelSize: _poster.labelSize
            font.weight: Font.Bold
            font.capitalization: Font.AllUppercase
            lineHeight: 1.05
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
    }

    component Mosaic: Grid {
        id: _mosaic

        property var picks: []
        property real tileRadius: 0
        property int tileSourceWidth: 185

        columns: 2
        rows: 2
        spacing: 2

        Repeater {
            model: 4

            delegate: Poster {
                id: _tile

                required property int index

                width: (_mosaic.width - 2) / 2
                height: (_mosaic.height - 2) / 2
                radius: _mosaic.tileRadius
                sourceWidth: _mosaic.tileSourceWidth
                posterPath: _mosaic.picks.length > 0
                            ? (_mosaic.picks[_tile.index % _mosaic.picks.length].posterPath || "")
                            : ""
            }
        }
    }

    ModalScrim {
        shown: root.opened
    }

    Rectangle {
        id: _panel

        anchors.centerIn: parent
        width: Math.min(root.width - 2 * AppTheme.spacing20, 760)
        height: Math.min(root.height - 2 * AppTheme.spacing20, 620)
        radius: AppTheme.radiusSheet
        color: AppTheme.surface
        border.width: 1
        border.color: AppTheme.outline
        opacity: root.opened ? 1 : 0
        scale: root.opened ? 1 : 0.96

        Accessible.role: Accessible.Dialog
        Accessible.name: root.stepTitle(root.step)

        Behavior on opacity {
            NumberAnimation { duration: 140 }
        }

        Behavior on scale {
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        MouseArea {
            anchors.fill: parent
            onWheel: (wheel) => { wheel.accepted = true }
            onPressed: (mouse) => {
                root.dropFocus()
                mouse.accepted = false
            }
        }

        Column {
            id: _header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: AppTheme.spacing20
            anchors.rightMargin: AppTheme.spacing20
            anchors.topMargin: AppTheme.spacing16
            spacing: AppTheme.spacing4

            Item {
                width: parent.width
                height: 20

                readonly property real lineWidth: Math.max(
                    AppTheme.spacing12,
                    (width - _one.width - _two.width - _three.width
                     - 4 * AppTheme.spacing10) / 2)

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: AppTheme.spacing10

                    StepMark {
                        id: _one

                        number: 1
                        currentStep: root.step
                        label: qsTr("Name")
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.parent.lineWidth
                        height: 1
                        color: AppTheme.outline
                    }

                    StepMark {
                        id: _two

                        number: 2
                        currentStep: root.step
                        label: qsTr("Pick films")
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.parent.lineWidth
                        height: 1
                        color: AppTheme.outline
                    }

                    StepMark {
                        id: _three

                        number: 3
                        currentStep: root.step
                        label: qsTr("Order")
                    }
                }
            }

            Item {
                width: 1
                height: AppTheme.spacing8
            }

            Text {
                width: parent.width
                text: root.stepTitle(root.step)
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fs20
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.stepHint(root.step)
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs12
                elide: Text.ElideRight
            }

            Item {
                width: 1
                height: AppTheme.spacing12
            }
        }

        Divider {
            id: _headerLine

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _header.bottom
        }

        Flickable {
            id: _body

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _headerLine.bottom
            anchors.bottom: _footerLine.top
            anchors.leftMargin: AppTheme.spacing20
            anchors.rightMargin: AppTheme.spacing20
            anchors.topMargin: AppTheme.spacing16
            anchors.bottomMargin: AppTheme.spacing12
            contentHeight: {
                if (root.step === 1)
                    return _nameStep.implicitHeight
                if (root.step === 3)
                    return _orderStep.implicitHeight
                return _createdStep.implicitHeight
            }
            clip: true
            interactive: !root.reordering
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: AppScrollBar {}

            Item {
                id: _nameStep

                width: _body.width - AppTheme.scrollBarWidth
                implicitHeight: Math.max(_fields.implicitHeight, _preview.implicitHeight)
                visible: root.step === 1

                Column {
                    id: _fields

                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: root.wide ? parent.width - 190 - AppTheme.spacing20 : parent.width
                    spacing: AppTheme.spacing6

                    FieldLabel { text: qsTr("NAME") }

                    Rectangle {
                        width: parent.width
                        height: AppTheme.controlHeightLarge - 8
                        radius: AppTheme.radiusSmall
                        color: AppTheme.surfaceVariant
                        border.width: _nameField.activeFocus ? 2 : 1
                        border.color: _nameField.activeFocus ? AppTheme.focus : AppTheme.outline

                        TextField {
                            id: _nameField

                            anchors.fill: parent
                            leftPadding: AppTheme.spacing12
                            rightPadding: AppTheme.spacing12
                            topPadding: 0
                            bottomPadding: 0
                            verticalAlignment: TextInput.AlignVCenter
                            text: root.collectionName
                            maximumLength: 40
                            color: AppTheme.textPrimary
                            font.pixelSize: AppTheme.fs14
                            selectByMouse: true
                            background: Item {}

                            onTextChanged: root.collectionName = text
                            Keys.onEscapePressed: root.back()
                            Keys.onPressed: (event) => {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    root.next()
                                    event.accepted = true
                                }
                            }
                        }
                    }

                    Item {
                        width: 1
                        height: AppTheme.spacing10
                    }

                    FieldLabel { text: qsTr("DESCRIPTION - SHOWN ON THE COLLECTION PAGE (OPTIONAL)") }

                    Rectangle {
                        width: parent.width
                        height: 84
                        radius: AppTheme.radiusSmall
                        color: AppTheme.surfaceVariant
                        border.width: _descriptionField.activeFocus ? 2 : 1
                        border.color: _descriptionField.activeFocus ? AppTheme.focus
                                                                    : AppTheme.outline

                        TextArea {
                            id: _descriptionField

                            anchors.fill: parent
                            leftPadding: AppTheme.spacing10
                            rightPadding: AppTheme.spacing10
                            topPadding: AppTheme.spacing8
                            bottomPadding: AppTheme.spacing8
                            text: root.description
                            color: AppTheme.textPrimary
                            font.pixelSize: AppTheme.fs13
                            wrapMode: TextArea.Wrap
                            selectByMouse: true
                            background: Item {}

                            onTextChanged: {
                                if (length > 200)
                                    remove(200, length)
                                root.description = text
                            }
                            Keys.onEscapePressed: root.back()
                        }
                    }

                    Item {
                        width: 1
                        height: AppTheme.spacing10
                    }

                    FieldLabel { text: qsTr("COVER") }

                    Row {
                        spacing: AppTheme.spacing8

                        Repeater {
                            model: [{ mode: "mosaic", label: qsTr("Mosaic of picks") },
                                    { mode: "poster", label: qsTr("One film's poster") }]

                            delegate: AbstractButton {
                                id: _coverOption

                                required property var modelData

                                readonly property bool chosen: root.coverMode === _coverOption.modelData.mode

                                readonly property real cardWidth: 96
                                readonly property real artWidth: cardWidth - 2 * AppTheme.spacing8

                                width: cardWidth
                                height: Math.round(artWidth * AppTheme.posterAspectRatio)
                                        + 2 * AppTheme.spacing8 + AppTheme.spacing6 + 32
                                padding: AppTheme.spacing8
                                hoverEnabled: true

                                background: Rectangle {
                                    radius: AppTheme.radiusSmall
                                    color: _coverOption.chosen ? AppTheme.surfaceVariant
                                                               : "transparent"
                                    border.width: 1
                                    border.color: _coverOption.chosen ? AppTheme.textPrimary
                                                                      : AppTheme.outline
                                }

                                contentItem: Column {
                                    id: _coverColumn

                                    spacing: AppTheme.spacing6

                                    Item {
                                        width: _coverOption.artWidth
                                        height: width * AppTheme.posterAspectRatio

                                        Mosaic {
                                            anchors.fill: parent
                                            visible: _coverOption.modelData.mode === "mosaic"
                                            picks: root.picks
                                            tileRadius: 2
                                            tileSourceWidth: 92
                                        }

                                        Poster {
                                            anchors.fill: parent
                                            visible: _coverOption.modelData.mode === "poster"
                                            posterPath: root.picks.length > 0
                                                        ? (root.picks[0].posterPath || "") : ""

                                            Text {
                                                anchors.centerIn: parent
                                                visible: root.picks.length === 0
                                                text: "?"
                                                color: AppTheme.textDisabled
                                                font.pixelSize: AppTheme.fs20
                                                font.weight: Font.Bold
                                            }
                                        }
                                    }

                                    Text {
                                        width: _coverOption.artWidth
                                        text: _coverOption.modelData.label
                                        color: AppTheme.textPrimary
                                        font.pixelSize: AppTheme.fs11
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.Wrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                    }
                                }

                                onClicked: {
                                    root.dropFocus()
                                    root.coverMode = _coverOption.modelData.mode
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        visible: root.coverMode === "poster" && root.picks.length === 0
                        topPadding: AppTheme.spacing6
                        text: qsTr("The first film you pick in step 2 becomes the cover.")
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fs12
                        wrapMode: Text.Wrap
                    }
                }

                Column {
                    id: _preview

                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: 190
                    visible: root.wide
                    spacing: AppTheme.spacing6

                    FieldLabel { text: qsTr("PREVIEW") }

                    Poster {
                        width: parent.width
                        height: width * AppTheme.posterAspectRatio
                        label: root.collectionName.trim().length > 0
                               ? root.collectionName : qsTr("Untitled")
                        labelSize: AppTheme.fs16
                        posterPath: root.coverMode === "poster" && root.picks.length > 0
                                    ? (root.picks[0].posterPath || "") : ""
                        posterSize: 342
                        sourceWidth: 342

                        Mosaic {
                            anchors.fill: parent
                            visible: root.coverMode === "mosaic"
                            picks: root.picks
                        }
                    }
                }
            }

            Item {
                id: _orderStep

                width: _body.width - AppTheme.scrollBarWidth
                implicitHeight: Math.max(_orderCover.implicitHeight, _orderList.implicitHeight)
                visible: root.step === 3

                Column {
                    id: _orderCover

                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: root.wide ? 150 : parent.width
                    spacing: AppTheme.spacing6

                    FieldLabel { text: qsTr("COVER") }

                    Poster {
                        width: parent.width
                        height: width * AppTheme.posterAspectRatio
                        label: root.collectionName
                        labelSize: AppTheme.fs14
                        posterPath: root.coverMode === "poster" && root.picks.length > 0
                                    ? (root.picks[0].posterPath || "") : ""
                        posterSize: 342
                        sourceWidth: 342

                        Mosaic {
                            anchors.fill: parent
                            visible: root.coverMode === "mosaic"
                            picks: root.picks
                            tileSourceWidth: 92
                        }
                    }

                    Row {
                        spacing: AppTheme.spacing16
                        topPadding: AppTheme.spacing6

                        Column {
                            Text {
                                text: String(root.picks.length)
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fs17
                                font.weight: Font.Medium
                            }

                            Text {
                                text: qsTr("films")
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fs11
                            }
                        }

                        Column {
                            visible: root.pickedRuntime > 0

                            Text {
                                text: Format.runtime(root.pickedRuntime)
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fs17
                                font.weight: Font.Medium
                            }

                            Text {
                                text: qsTr("runtime")
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fs11
                            }
                        }
                    }
                }

                Column {
                    id: _orderList

                    anchors.right: parent.right
                    anchors.top: root.wide ? parent.top : _orderCover.bottom
                    anchors.topMargin: root.wide ? 0 : AppTheme.spacing16
                    width: root.wide ? parent.width - _orderCover.width - AppTheme.spacing20
                                     : parent.width
                    spacing: AppTheme.spacing6

                    FieldLabel { text: qsTr("WATCH ORDER - DRAG TO REORDER") }

                    Repeater {
                        model: root.picks

                        delegate: Rectangle {
                            id: _orderRow

                            required property var modelData
                            required property int index

                            readonly property int rowHeight: 56

                            property real dragOffset: 0

                            width: parent.width
                            height: rowHeight
                            radius: AppTheme.radiusSmall
                            color: _rowDrag.pressed ? AppTheme.surfaceRaised
                                                    : AppTheme.surfaceVariant
                            border.width: _rowDrag.pressed ? 1 : 0
                            border.color: AppTheme.warning
                            z: _rowDrag.pressed ? 2 : 0

                            transform: Translate { y: _orderRow.dragOffset }

                            MouseArea {
                                id: _rowDrag

                                property real grabbedAt: 0

                                anchors.fill: parent
                                preventStealing: true
                                cursorShape: pressed ? Qt.ClosedHandCursor
                                                     : Qt.OpenHandCursor

                                onPressed: (mouse) => {
                                    _rowDrag.grabbedAt = mouse.y
                                    root.reordering = true
                                }

                                onPositionChanged: (mouse) => {
                                    if (_rowDrag.pressed)
                                        _orderRow.dragOffset += mouse.y - _rowDrag.grabbedAt
                                }

                                onCanceled: {
                                    _orderRow.dragOffset = 0
                                    root.reordering = false
                                }

                                onReleased: {
                                    const step = Math.round(
                                        _orderRow.dragOffset
                                        / (_orderRow.rowHeight + _orderList.spacing))
                                    _orderRow.dragOffset = 0
                                    root.reordering = false
                                    if (step === 0)
                                        return
                                    root.movePick(_orderRow.index,
                                                  Math.max(0, Math.min(root.picks.length - 1,
                                                                       _orderRow.index + step)))
                                }
                            }

                            Text {
                                id: _grip

                                anchors.left: parent.left
                                anchors.leftMargin: AppTheme.spacing8
                                anchors.verticalCenter: parent.verticalCenter
                                text: "≡"
                                color: AppTheme.textDisabled
                                font.pixelSize: AppTheme.fs16
                            }

                            Text {
                                id: _position

                                anchors.left: _grip.right
                                anchors.leftMargin: AppTheme.spacing8
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                text: String(_orderRow.index + 1)
                                color: AppTheme.textSecondary
                                font.pixelSize: AppTheme.fs13
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Poster {
                                id: _orderPoster

                                anchors.left: _position.right
                                anchors.leftMargin: AppTheme.spacing10
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.round(40 / AppTheme.posterAspectRatio)
                                height: 40
                                radius: 3
                                posterPath: _orderRow.modelData.posterPath || ""
                                sourceWidth: 92
                            }

                            Column {
                                anchors.left: _orderPoster.right
                                anchors.leftMargin: AppTheme.spacing12
                                anchors.right: _removePick.left
                                anchors.rightMargin: AppTheme.spacing8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 1

                                Text {
                                    width: parent.width
                                    text: _orderRow.modelData.title
                                    color: AppTheme.textPrimary
                                    font.pixelSize: AppTheme.fs13
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: {
                                        const bits = []
                                        if (_orderRow.modelData.year > 0)
                                            bits.push(String(_orderRow.modelData.year))
                                        if (_orderRow.modelData.kind === "tv")
                                            bits.push(qsTr("TV show"))
                                        else if (_orderRow.modelData.runtimeMinutes > 0)
                                            bits.push(Format.runtime(_orderRow.modelData.runtimeMinutes))
                                        return bits.join(" · ")
                                    }
                                    color: AppTheme.textSecondary
                                    font.pixelSize: AppTheme.fs11
                                    elide: Text.ElideRight
                                }
                            }

                            IconButton {
                                id: _removePick

                                anchors.right: parent.right
                                anchors.rightMargin: AppTheme.spacing4
                                anchors.verticalCenter: parent.verticalCenter
                                compact: true
                                iconSource: Icons.close
                                accessibleName: qsTr("Remove %1").arg(_orderRow.modelData.title)

                                onClicked: root.removePick(_orderRow.index)
                            }
                        }
                    }

                    Row {
                        topPadding: AppTheme.spacing6
                        spacing: AppTheme.spacing8

                        AppButton {
                            size: AppButton.Medium
                            variant: AppButton.Outlined
                            text: qsTr("Sort by year")

                            onClicked: root.sortPicks("year")
                        }

                        AppButton {
                            size: AppButton.Medium
                            variant: AppButton.Outlined
                            text: qsTr("Sort A-Z")

                            onClicked: root.sortPicks("name")
                        }

                        AppButton {
                            size: AppButton.Medium
                            variant: AppButton.Outlined
                            text: qsTr("+ Add more films")

                            onClicked: root.step = 2
                        }
                    }
                }
            }

            Column {
                id: _createdStep

                width: _body.width - AppTheme.scrollBarWidth
                visible: root.step === 4
                spacing: AppTheme.spacing10
                topPadding: AppTheme.spacing24

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 56
                    height: 56
                    radius: 28
                    color: AppTheme.textPrimary

                    ThemedIcon {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        source: Icons.check
                        tintColor: AppTheme.background
                        showPlaceholder: false
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Collection created")
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fs16
                    font.weight: Font.Medium
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("It now appears on the Collections page.")
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fs13
                }
            }
        }

        Item {
            id: _pickStep

            anchors.fill: _body
            visible: root.step === 2

            readonly property int columnCount: Math.max(2, Math.floor(width / 116))
            readonly property real cellWidth: Math.floor(width / columnCount)

            Row {
                id: _pickTools

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: AppTheme.spacing8

                Rectangle {
                    id: _searchBox

                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - _kinds.width - parent.spacing
                    height: AppTheme.controlHeightSmall + 4
                    radius: AppTheme.radiusPill
                    color: AppTheme.surfaceVariant
                    border.width: _searchField.activeFocus ? 2 : 1
                    border.color: _searchField.activeFocus ? AppTheme.focus : AppTheme.outline

                    ThemedIcon {
                        id: _searchIcon

                        anchors.left: parent.left
                        anchors.leftMargin: AppTheme.spacing12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        source: Icons.search
                        tintColor: AppTheme.textSecondary
                        showPlaceholder: false
                    }

                    Text {
                        anchors.left: _searchIcon.right
                        anchors.leftMargin: AppTheme.spacing8
                        anchors.verticalCenter: parent.verticalCenter
                        visible: _searchField.text.length === 0
                        text: qsTr("Search your library")
                        color: AppTheme.textDisabled
                        font.pixelSize: AppTheme.fs13
                    }

                    TextField {
                        id: _searchField

                        anchors.fill: parent
                        leftPadding: _searchIcon.width + 2 * AppTheme.spacing12
                        rightPadding: AppTheme.spacing12
                        topPadding: 0
                        bottomPadding: 0
                        verticalAlignment: TextInput.AlignVCenter
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fs13
                        selectByMouse: true
                        background: Item {}

                        onTextChanged: Library.pickerQuery = text
                        Keys.onEscapePressed: {
                            if (text.length > 0)
                                text = ""
                            else
                                root.back()
                        }
                    }
                }

                Row {
                    id: _kinds

                    anchors.verticalCenter: parent.verticalCenter
                    spacing: AppTheme.spacing6

                    Repeater {
                        model: [{ kind: 0, label: qsTr("All") },
                                { kind: 1, label: qsTr("Movies") },
                                { kind: 2, label: qsTr("TV shows") }]

                        delegate: PillChip {
                            required property var modelData

                            text: modelData.label
                            selected: Library.pickerKind === modelData.kind

                            onClicked: {
                                root.dropFocus()
                                Library.pickerKind = modelData.kind
                            }
                        }
                    }
                }
            }

            GridView {
                id: _picker

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: _pickTools.bottom
                anchors.bottom: _selected.top
                anchors.topMargin: AppTheme.spacing12
                anchors.bottomMargin: AppTheme.spacing8
                clip: true
                visible: Library.pickerTitles.count > 0
                model: Library.pickerTitles
                cellWidth: _pickStep.cellWidth
                cellHeight: Math.round((_pickStep.cellWidth - AppTheme.spacing10)
                                       * AppTheme.posterAspectRatio) + 38
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: cellHeight
                reuseItems: true

                ScrollBar.vertical: AppScrollBar {}

                delegate: Item {
                    id: _cell

                    required property var mediaId
                    required property string title
                    required property string kind
                    required property string posterPath
                    required property int year
                    required property int runtimeMinutes
                    required property int artworkStamp

                    readonly property int pickedAt: root.pickIndex(_cell.mediaId)

                    width: _picker.cellWidth
                    height: _picker.cellHeight

                    Column {
                        width: parent.width - AppTheme.spacing10
                        spacing: AppTheme.spacing4

                        Item {
                            width: parent.width
                            height: Math.round(width * AppTheme.posterAspectRatio)

                            Poster {
                                anchors.fill: parent
                                label: _cell.posterPath.length > 0 ? "" : _cell.title
                                labelSize: AppTheme.fs11
                                posterPath: _cell.posterPath
                                artworkStamp: _cell.artworkStamp
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: AppTheme.radiusSmall / 2
                                color: "transparent"
                                border.width: _cell.pickedAt >= 0 ? 2 : 0
                                border.color: AppTheme.textPrimary
                            }

                            Rectangle {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: AppTheme.spacing4
                                width: 22
                                height: 22
                                radius: 11
                                color: _cell.pickedAt >= 0 ? AppTheme.textPrimary
                                                           : Qt.rgba(0, 0, 0, 0.45)
                                border.width: _cell.pickedAt >= 0 ? 0 : 2
                                border.color: Qt.rgba(1, 1, 1, 0.8)

                                Text {
                                    anchors.centerIn: parent
                                    visible: _cell.pickedAt >= 0
                                    text: String(_cell.pickedAt + 1)
                                    color: AppTheme.background
                                    font.pixelSize: AppTheme.fs11
                                    font.weight: Font.Bold
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.margins: AppTheme.spacing4
                                visible: _cell.kind === "tv"
                                width: _tvTag.implicitWidth + 10
                                height: _tvTag.implicitHeight + 3
                                radius: 4
                                color: Qt.rgba(0, 0, 0, 0.72)

                                Text {
                                    id: _tvTag

                                    anchors.centerIn: parent
                                    text: qsTr("TV")
                                    color: "white"
                                    font.pixelSize: AppTheme.fs9
                                    font.weight: Font.Bold
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.togglePick({
                                    "mediaId": _cell.mediaId,
                                    "title": _cell.title,
                                    "kind": _cell.kind,
                                    "posterPath": _cell.posterPath,
                                    "year": _cell.year,
                                    "runtimeMinutes": _cell.runtimeMinutes
                                })
                            }
                        }

                        Text {
                            width: parent.width
                            text: _cell.title
                            color: AppTheme.textPrimary
                            font.pixelSize: AppTheme.fs11
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: {
                                const bits = []
                                if (_cell.year > 0)
                                    bits.push(String(_cell.year))
                                if (_cell.runtimeMinutes > 0)
                                    bits.push(Format.runtime(_cell.runtimeMinutes))
                                return bits.join(" · ")
                            }
                            color: AppTheme.textSecondary
                            font.pixelSize: AppTheme.fs10
                            elide: Text.ElideRight
                        }
                    }
                }
            }

            EmptyState {
                anchors.centerIn: _picker
                width: Math.min(_pickStep.width - 2 * AppTheme.spacing24, 360)
                visible: Library.pickerTitles.count === 0
                iconSource: Icons.search
                title: qsTr("Nothing found")
                message: qsTr("No title in your library matches that.")
            }

            Rectangle {
                id: _selected

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 48
                radius: AppTheme.radiusSmall
                color: AppTheme.surfaceVariant

                Text {
                    id: _selectedCount

                    anchors.left: parent.left
                    anchors.leftMargin: AppTheme.spacing12
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 selected").arg(root.picks.length)
                    color: AppTheme.textPrimary
                    font.pixelSize: AppTheme.fs12
                }

                ListView {
                    anchors.left: _selectedCount.right
                    anchors.right: _clearPicks.left
                    anchors.leftMargin: AppTheme.spacing10
                    anchors.rightMargin: AppTheme.spacing10
                    anchors.verticalCenter: parent.verticalCenter
                    height: 36
                    orientation: ListView.Horizontal
                    spacing: AppTheme.spacing4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.picks

                    delegate: Poster {
                        required property var modelData

                        width: Math.round(36 / AppTheme.posterAspectRatio)
                        height: 36
                        radius: 2
                        posterPath: modelData.posterPath || ""
                        sourceWidth: 92
                    }
                }

                AppButton {
                    id: _clearPicks

                    anchors.right: parent.right
                    anchors.rightMargin: AppTheme.spacing8
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.picks.length > 0
                    width: visible ? implicitWidth : 0
                    size: AppButton.Medium
                    variant: AppButton.Plain
                    text: qsTr("Clear")

                    onClicked: root.clearPicks()
                }
            }
        }

        Divider {
            id: _footerLine

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: _footer.top
            anchors.bottomMargin: AppTheme.spacing12
        }

        Item {
            id: _footer

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: AppTheme.spacing20
            anchors.rightMargin: AppTheme.spacing20
            anchors.bottomMargin: AppTheme.spacing14
            height: _buttons.implicitHeight

            Text {
                anchors.left: parent.left
                anchors.right: _buttons.left
                anchors.rightMargin: AppTheme.spacing12
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    if (root.step === 2) {
                        if (!root.canLeavePickStep)
                            return qsTr("Pick at least 2")
                        const bits = [qsTr("%1 films").arg(root.picks.length)]
                        const runtime = Format.runtime(root.pickedRuntime)
                        if (runtime.length > 0)
                            bits.push(runtime)
                        if (root.pickedShows > 0) {
                            bits.push(root.pickedShows === 1
                                      ? qsTr("1 show")
                                      : qsTr("%1 shows").arg(root.pickedShows))
                        }
                        return bits.join("  ·  ")
                    }
                    if (root.step === 3)
                        return qsTr("Order can be changed later")
                    if (root.step === 4)
                        return ""
                    return root.canLeaveNameStep ? "" : qsTr("Name required")
                }
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs12
                elide: Text.ElideRight
            }

            Row {
                id: _buttons

                anchors.right: parent.right
                spacing: AppTheme.spacing8

                AppButton {
                    text: {
                        if (root.step === 1)
                            return qsTr("Cancel")
                        if (root.step === 4)
                            return qsTr("Create another")
                        return qsTr("Back")
                    }
                    variant: AppButton.Outlined
                    size: AppButton.Medium

                    onClicked: {
                        if (root.step === 4)
                            root.open()
                        else
                            root.back()
                    }
                }

                AppButton {
                    text: {
                        if (root.step === 1)
                            return qsTr("Next: pick films")
                        if (root.step === 2)
                            return qsTr("Next: order")
                        if (root.step === 3)
                            return qsTr("Create collection")
                        return qsTr("Open collection")
                    }
                    variant: AppButton.Filled
                    size: AppButton.Medium
                    enabled: {
                        if (root.step === 1)
                            return root.canLeaveNameStep
                        if (root.step === 2)
                            return root.canLeavePickStep
                        return true
                    }

                    onClicked: {
                        if (root.step === 3) {
                            root.create()
                        } else if (root.step === 4) {
                            const id = root.createdId
                            root.close()
                            root.openRequested(id)
                        } else {
                            root.next()
                        }
                    }
                }
            }
        }
    }
}
