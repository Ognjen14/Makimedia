pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Window
import "../../Singletons" as S
import "../../Controls" as Ctrl
import ".." as Pages
import com.topicdev.makimedia 1.0

Item {
    id: root

    property var collectionId: 0

    signal backRequested()
    signal playRequested(string handle)
    signal detailsRequested(string handle)

    readonly property bool acceptsFocus: true

    component Stat: Column {
        id: _stat

        property string value
        property string label

        visible: value.length > 0
        spacing: 0

        Text {
            text: _stat.value
            color: S.AppTheme.textPrimary
            font.pixelSize: S.AppTheme.fs18
            font.weight: Font.Bold
            textFormat: Text.StyledText
        }

        Text {
            text: _stat.label
            color: S.AppTheme.textSecondary
            font.pixelSize: S.AppTheme.fs11
        }
    }

    function takeFocus() {
        if (_playNext.visible)
            _playNext.forceActiveFocus(Qt.TabFocusReason)
        else if (_markAll.visible)
            _markAll.forceActiveFocus(Qt.TabFocusReason)
        else
            root.focusBelowTheHero()
    }

    function focusBelowTheHero() {
        if (_orderRow.visible) {
            _orderRow.forceActiveFocus(Qt.TabFocusReason)
            root.showFocused(_orderRow)
            return true
        }
        return root.stepRow(-1, 1, 0)
    }

    function focusAboveTheRows() {
        if (_orderRow.visible) {
            _orderRow.forceActiveFocus(Qt.TabFocusReason)
            root.showFocused(_orderRow)
        } else if (_playNext.visible) {
            _playNext.forceActiveFocus(Qt.TabFocusReason)
            _scroll.contentY = 0
        } else if (_markAll.visible) {
            _markAll.forceActiveFocus(Qt.TabFocusReason)
            _scroll.contentY = 0
        }
    }

    function stepRow(from, delta, column) {
        let at = from + delta
        while (at >= 0 && at < _rows.count) {
            const item = _rows.itemAt(at)
            if (item && item.selectable) {
                item.focusAt(column)
                root.showFocused(item)
                return true
            }
            at += delta
        }
        return false
    }

    function showFocused(item) {
        _focusScroll.reveal(item)
    }

    Ctrl.FocusScroller {
        id: _focusScroll

        owner: root
        flickable: _scroll
        content: _content
    }

    Keys.onUpPressed: _focusScroll.step(false)
    Keys.onDownPressed: _focusScroll.step(true)

    readonly property var page: Library.collectionPage
    readonly property var info: page.info
    readonly property bool wide: width >= 720
    readonly property real gutter: wide ? S.AppTheme.spacing24 : S.AppTheme.spacing16

    readonly property int heroHeight: {
        if (wide) {
            return Math.round(Math.max(Math.min(420, height * 0.55),
                                       Math.min(560, height * 0.62)))
        }
        return Math.round(Math.max(Math.min(440, height * 0.55), height * 0.7))
    }

    function tagText(tag) {
        switch (tag) {
        case "universe":
            return qsTr("UNIVERSE")
        case "custom":
            return qsTr("CUSTOM")
        case "duology":
            return qsTr("DUOLOGY")
        case "trilogy":
            return qsTr("TRILOGY")
        default:
            return qsTr("COLLECTION")
        }
    }

    function yearsText() {
        const first = info.firstYear || 0
        const last = info.lastYear || 0
        if (first <= 0)
            return ""
        if (last <= first)
            return String(first)
        const tail = Math.floor(first / 100) === Math.floor(last / 100)
                     ? String(last % 100).padStart(2, "0") : String(last)
        return first + "–" + tail
    }

    function filmsText(count) {
        return count === 1 ? qsTr("1 FILM") : qsTr("%1 FILMS").arg(count)
    }

    onCollectionIdChanged: page.collectionId = root.collectionId
    Component.onCompleted: {
        page.collectionId = root.collectionId
        Metadata.ensureCollectionFilms()
    }

    Rectangle {
        anchors.fill: parent
        color: S.AppTheme.background
    }

    Flickable {
        id: _scroll

        anchors.fill: parent
        contentHeight: _content.implicitHeight + S.AppTheme.spacing32
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Column {
            id: _content

            width: _scroll.width

            Item {
                id: _hero

                width: parent.width
                height: root.heroHeight
                clip: true

                Rectangle {
                    anchors.fill: parent
                    color: S.AppTheme.surfaceVariant
                }

                Pages.HeroBackdrop {
                    anchors.fill: parent
                    source: {
                        void Metadata.artworkRevision
                        return Metadata.backdropUrl(root.info.backdropPath || "",
                                                    root.wide ? 1280 : 780)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0.0
                            color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                                           S.AppTheme.background.b, 0.92)
                        }
                        GradientStop {
                            position: 0.45
                            color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                                           S.AppTheme.background.b, 0.65)
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.rgba(S.AppTheme.background.r, S.AppTheme.background.g,
                                           S.AppTheme.background.b, 0.0)
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.35) }
                        GradientStop { position: 0.45; color: Qt.rgba(
                            S.AppTheme.background.r, S.AppTheme.background.g,
                            S.AppTheme.background.b, 0.55) }
                        GradientStop { position: 1.0; color: S.AppTheme.background }
                    }
                }

                Item {
                    id: _headerArea

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: root.gutter
                    anchors.rightMargin: root.gutter
                    anchors.bottomMargin: S.AppTheme.spacing16
                    height: _header.implicitHeight

                    Column {
                        id: _header

                        anchors.left: parent.left
                        anchors.right: _ringArea.visible ? _ringArea.left : parent.right
                        anchors.rightMargin: _ringArea.visible ? S.AppTheme.spacing24 : 0
                        anchors.bottom: parent.bottom
                        spacing: S.AppTheme.spacing10

                        Row {
                            spacing: S.AppTheme.spacing8

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: _tag.implicitWidth + S.AppTheme.spacing12
                                height: _tag.implicitHeight + S.AppTheme.spacing6
                                radius: 4
                                color: Qt.rgba(S.AppTheme.textPrimary.r, S.AppTheme.textPrimary.g,
                                               S.AppTheme.textPrimary.b, 0.14)

                                Text {
                                    id: _tag

                                    anchors.centerIn: parent
                                    text: root.tagText(root.info.tag || "")
                                    color: S.AppTheme.textPrimary
                                    font.pixelSize: S.AppTheme.fs11
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: {
                                    const bits = []
                                    const years = root.yearsText()
                                    if (years.length > 0)
                                        bits.push(years)
                                    bits.push(root.filmsText(root.info.total || 0))
                                    return bits.join("  ·  ")
                                }
                                color: S.AppTheme.textSecondary
                                font.pixelSize: S.AppTheme.fs12
                                font.weight: Font.Medium
                                font.letterSpacing: 1
                            }
                        }

                        Text {
                            width: parent.width
                            text: root.info.name || ""
                            color: S.AppTheme.textPrimary
                            font.pixelSize: root.wide ? S.AppTheme.fs28 : S.AppTheme.fs22
                            font.weight: Font.Bold
                            font.capitalization: Font.AllUppercase
                            lineHeight: 1.0
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Text {
                            width: Math.min(parent.width, 520)
                            visible: text.length > 0
                            text: root.info.description || ""
                            color: S.AppTheme.textPrimary
                            font.pixelSize: S.AppTheme.fs13
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Flow {
                            width: parent.width
                            spacing: S.AppTheme.spacing24

                            Stat {
                                value: (root.info.owned || 0) + "<font color=\""
                                       + S.AppTheme.textSecondary + "\"> / "
                                       + (root.info.total || 0) + "</font>"
                                label: qsTr("in library")
                            }

                            Stat {
                                value: S.Format.runtime(root.info.totalMinutes || 0)
                                label: qsTr("total runtime")
                            }

                            Stat {
                                value: S.Format.runtime(root.info.leftMinutes || 0)
                                label: qsTr("left to watch")
                            }

                            Stat {
                                value: (root.info.averageRating || 0) > 0
                                       ? S.Format.rating(root.info.averageRating) : ""
                                label: qsTr("avg rating")
                            }
                        }

                        Flow {
                            width: parent.width
                            spacing: S.AppTheme.spacing10

                            Ctrl.AppButton {
                                id: _playNext

                                visible: (root.info.nextHandle || "").length > 0
                                variant: Ctrl.AppButton.Filled
                                iconSource: S.Icons.play

                                Keys.onRightPressed: {
                                    if (_markAll.visible)
                                        _markAll.forceActiveFocus(Qt.TabFocusReason)
                                }
                                Keys.onDownPressed: root.focusBelowTheHero()
                                text: {
                                    const title = root.info.nextTitle || ""
                                    if (root.info.nextResumes === true)
                                        return qsTr("Resume %1 · %2 left").arg(title)
                                                .arg(S.Format.runtime(root.info.nextLeftMinutes || 0))
                                    return qsTr("Play %1").arg(title)
                                }

                                onClicked: root.playRequested(root.info.nextHandle)
                            }

                            Ctrl.AppButton {
                                id: _markAll

                                visible: (root.info.owned || 0) > 0
                                variant: Ctrl.AppButton.Outlined
                                iconSource: root.info.allOwnedWatched === true ? "" : S.Icons.check

                                Keys.onLeftPressed: {
                                    if (_playNext.visible)
                                        _playNext.forceActiveFocus(Qt.TabFocusReason)
                                }
                                Keys.onDownPressed: root.focusBelowTheHero()
                                text: root.info.allOwnedWatched === true
                                      ? qsTr("Mark all unwatched")
                                      : qsTr("Mark all watched")

                                onClicked: Library.setAllWatched(root.info.ownedHandles || [],
                                                                 root.info.allOwnedWatched !== true)
                            }
                        }
                    }

                    Column {
                        id: _ringArea

                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        visible: root.wide
                        spacing: S.AppTheme.spacing12

                        Item {
                            id: _ring

                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 112
                            height: 112

                            readonly property real fraction: (root.info.total || 0) > 0
                                ? (root.info.watched || 0) / root.info.total : 0

                            onFractionChanged: _ringCanvas.requestPaint()

                            Canvas {
                                id: _ringCanvas

                                anchors.fill: parent
                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.reset()
                                    const r = width / 2 - 6
                                    ctx.lineWidth = 8
                                    ctx.lineCap = "round"
                                    ctx.strokeStyle = S.AppTheme.outline
                                    ctx.beginPath()
                                    ctx.arc(width / 2, height / 2, r, 0, 2 * Math.PI)
                                    ctx.stroke()
                                    if (_ring.fraction > 0) {
                                        ctx.strokeStyle = S.AppTheme.textPrimary
                                        ctx.beginPath()
                                        ctx.arc(width / 2, height / 2, r, -Math.PI / 2,
                                                -Math.PI / 2 + 2 * Math.PI * _ring.fraction)
                                        ctx.stroke()
                                    }
                                }
                            }

                            Column {
                                anchors.centerIn: parent

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: Math.round(_ring.fraction * 100) + "%"
                                    color: S.AppTheme.textPrimary
                                    font.pixelSize: S.AppTheme.fs22
                                    font.weight: Font.Bold
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: qsTr("%1 of %2 watched").arg(root.info.watched || 0)
                                          .arg(root.info.total || 0)
                                    color: S.AppTheme.textSecondary
                                    font.pixelSize: S.AppTheme.fs10
                                }
                            }
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: -10

                            Repeater {
                                model: root.info.posters || []

                                delegate: Rectangle {
                                    required property string modelData

                                    width: 26
                                    height: 39
                                    radius: 3
                                    color: S.AppTheme.surfaceRaised
                                    border.width: 1
                                    border.color: S.AppTheme.background
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        source: Metadata.posterUrl(parent.modelData, 185)
                                        sourceSize.width: 64
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: _orderRow.visible ? _orderRow.height + S.AppTheme.spacing16 : S.AppTheme.spacing8

                Row {
                    id: _orderRow

                    x: root.gutter
                    y: S.AppTheme.spacing8
                    visible: root.info.hasStoryOrder === true
                    spacing: S.AppTheme.spacing8

                    property int chipIndex: root.page.order === 1 ? 1 : 0

                    activeFocusOnTab: true

                    Keys.onLeftPressed: _orderRow.chipIndex = 0
                    Keys.onRightPressed: _orderRow.chipIndex = 1
                    Keys.onUpPressed: {
                        if (_playNext.visible)
                            _playNext.forceActiveFocus(Qt.TabFocusReason)
                        else if (_markAll.visible)
                            _markAll.forceActiveFocus(Qt.TabFocusReason)
                        _scroll.contentY = 0
                    }
                    Keys.onDownPressed: root.stepRow(-1, 1, 0)
                    Keys.onPressed: (event) => {
                        if (S.AppTheme.isActivateKey(event.key)) {
                            root.page.order = _orderRow.chipIndex
                            event.accepted = true
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        rightPadding: S.AppTheme.spacing4
                        text: qsTr("Watch order")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                    }

                    Ctrl.PillChip {
                        text: qsTr("Release")
                        selected: root.page.order === 0
                        highlighted: _orderRow.activeFocus && _orderRow.chipIndex === 0
                        onClicked: root.page.order = 0
                    }

                    Ctrl.PillChip {
                        text: qsTr("Story")
                        selected: root.page.order === 1
                        highlighted: _orderRow.activeFocus && _orderRow.chipIndex === 1
                        onClicked: root.page.order = 1
                    }
                }
            }

            Repeater {
                id: _rows

                model: root.page

                delegate: Item {
                    id: _row

                    required property int index

                    readonly property bool selectable: !_row.heading

                    function focusAt(column) {
                        if (column === 1 && _play.visible)
                            _play.forceActiveFocus(Qt.TabFocusReason)
                        else
                            _card.forceActiveFocus(Qt.TabFocusReason)
                    }

                    required property bool heading
                    required property bool isShow
                    required property string phase
                    required property int number
                    required property string title
                    required property int year
                    required property int runtimeMinutes
                    required property real rating
                    required property string posterPath
                    required property bool owned
                    required property string handle
                    required property bool watched
                    required property real progress
                    required property int leftMinutes
                    required property int artworkStamp

                    readonly property bool started: !watched && progress > 0
                    readonly property int posterWidth: root.wide ? 64 : 52

                    width: _content.width
                    height: heading ? _phase.implicitHeight + S.AppTheme.spacing24
                                    : _card.height + S.AppTheme.spacing8

                    Text {
                        id: _phase

                        x: root.gutter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: S.AppTheme.spacing8
                        visible: _row.heading
                        text: _row.phase.toUpperCase()
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs12
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                    }

                    Rectangle {
                        id: _card

                        x: root.gutter
                        width: parent.width - 2 * root.gutter
                        height: Math.max(_posterBox.height, _texts.implicitHeight)
                                + 2 * S.AppTheme.spacing12
                        visible: !_row.heading
                        radius: S.AppTheme.radiusMedium
                        color: _row.owned
                               ? (_rowArea.containsMouse ? S.AppTheme.surfaceRaised : S.AppTheme.surface)
                               : "transparent"
                        border.width: activeFocus ? 2 : (_row.owned ? 1 : 0)
                        border.color: activeFocus ? S.AppTheme.focus : S.AppTheme.outline

                        activeFocusOnTab: !_row.heading

                        Keys.onPressed: (event) => {
                            if (S.AppTheme.isActivateKey(event.key)) {
                                if (_row.owned)
                                    root.detailsRequested(_row.handle)
                                event.accepted = true
                            }
                        }

                        Keys.onDownPressed: root.stepRow(_row.index, 1, 0)
                        Keys.onUpPressed: {
                            if (!root.stepRow(_row.index, -1, 0))
                                root.focusAboveTheRows()
                        }
                        Keys.onRightPressed: {
                            if (_play.visible)
                                _play.forceActiveFocus(Qt.TabFocusReason)
                        }

                        MouseArea {
                            id: _rowArea

                            anchors.fill: parent
                            enabled: _row.owned
                            hoverEnabled: true
                            cursorShape: _row.owned ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: root.detailsRequested(_row.handle)
                        }

                        Shape {
                            anchors.fill: parent
                            visible: !_row.owned
                            preferredRendererType: Shape.CurveRenderer

                            ShapePath {
                                strokeColor: S.AppTheme.outlineStrong
                                strokeWidth: 1
                                strokeStyle: ShapePath.DashLine
                                dashPattern: [4, 4]
                                fillColor: "transparent"

                                PathRectangle {
                                    x: 0.5
                                    y: 0.5
                                    width: _card.width - 1
                                    height: _card.height - 1
                                    radius: _card.radius
                                }
                            }
                        }

                        Text {
                            id: _number

                            anchors.left: parent.left
                            anchors.leftMargin: S.AppTheme.spacing12
                            anchors.verticalCenter: parent.verticalCenter
                            width: root.wide ? 32 : 24
                            text: String(_row.number)
                            color: S.AppTheme.textDisabled
                            font.pixelSize: S.AppTheme.fs16
                            font.weight: Font.Bold
                        }

                        Column {
                            id: _posterBox

                            anchors.left: _number.right
                            anchors.leftMargin: S.AppTheme.spacing8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: S.AppTheme.spacing4

                            Rectangle {
                                id: _rowPoster

                                width: _row.posterWidth
                                height: Math.round(width * 1.5)
                                radius: S.AppTheme.radiusSmall / 2
                                color: S.AppTheme.surfaceVariant
                                opacity: _row.owned ? 1 : 0.45

                                Ctrl.RoundedClip {
                                    anchors.fill: parent
                                    radius: _rowPoster.radius

                                Canvas {
                                    id: _hatch

                                    anchors.fill: parent
                                    visible: !_row.owned
                                    onPaint: {
                                        const ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.strokeStyle = S.AppTheme.outline
                                        ctx.lineWidth = 4
                                        for (let i = -height; i < width + height; i += 10) {
                                            ctx.beginPath()
                                            ctx.moveTo(i, 0)
                                            ctx.lineTo(i + height, height)
                                            ctx.stroke()
                                        }
                                    }
                                }

                                    Image {
                                        anchors.fill: parent
                                        source: {
                                            void _row.artworkStamp
                                            return Metadata.posterUrl(_row.posterPath, 185)
                                        }
                                        sourceSize.width: Math.ceil(width * Screen.devicePixelRatio / 32) * 32
                                        fillMode: Image.PreserveAspectCrop
                                        asynchronous: true
                                        visible: status === Image.Ready
                                    }
                                }

                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 4
                                    visible: _row.watched
                                    width: 20
                                    height: 20
                                    radius: 10
                                    color: "white"

                                    Ctrl.ThemedIcon {
                                        anchors.centerIn: parent
                                        width: 13
                                        height: 13
                                        source: S.Icons.check
                                        tintColor: "black"
                                        showPlaceholder: false
                                    }
                                }
                            }

                            Rectangle {
                                width: _row.posterWidth
                                height: 3
                                radius: 2
                                visible: _row.started
                                color: S.AppTheme.outline

                                Rectangle {
                                    width: parent.width * Math.min(1, _row.progress)
                                    height: parent.height
                                    radius: 2
                                    color: S.AppTheme.textPrimary
                                }
                            }
                        }

                        Column {
                            id: _texts

                            anchors.left: _posterBox.right
                            anchors.leftMargin: S.AppTheme.spacing16
                            anchors.right: _action.left
                            anchors.rightMargin: S.AppTheme.spacing12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: S.AppTheme.spacing4

                            Row {
                                width: parent.width
                                spacing: S.AppTheme.spacing8

                                Text {
                                    id: _title

                                    width: Math.min(implicitWidth,
                                                    parent.width - (_resume.visible ? _resume.width + parent.spacing : 0))
                                    text: _row.title
                                    color: _row.owned ? S.AppTheme.textPrimary : S.AppTheme.textSecondary
                                    font.pixelSize: root.wide ? S.AppTheme.fs16 : S.AppTheme.fs14
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    id: _resume

                                    anchors.verticalCenter: _title.verticalCenter
                                    visible: _row.started
                                    width: _resumeLabel.implicitWidth + 12
                                    height: _resumeLabel.implicitHeight + 4
                                    radius: 4
                                    color: S.AppTheme.warning

                                    Text {
                                        id: _resumeLabel

                                        anchors.centerIn: parent
                                        text: qsTr("RESUME")
                                        color: "#1A1300"
                                        font.pixelSize: S.AppTheme.fs10
                                        font.weight: Font.Bold
                                    }
                                }
                            }

                            Text {
                                width: parent.width
                                text: {
                                    const bits = []
                                    if (_row.year > 0)
                                        bits.push(String(_row.year))
                                    if (_row.isShow)
                                        bits.push(qsTr("TV show"))
                                    else if (_row.runtimeMinutes > 0)
                                        bits.push(S.Format.runtime(_row.runtimeMinutes))
                                    if (_row.rating > 0)
                                        bits.push(S.Format.rating(_row.rating))
                                    if (_row.started && _row.leftMinutes > 0)
                                        bits.push(qsTr("%1 left").arg(S.Format.runtime(_row.leftMinutes)))
                                    return bits.join("  ·  ")
                                }
                                color: S.AppTheme.textSecondary
                                font.pixelSize: S.AppTheme.fs12
                                elide: Text.ElideRight
                            }
                        }

                        Item {
                            id: _action

                            anchors.right: parent.right
                            anchors.rightMargin: S.AppTheme.spacing16
                            anchors.verticalCenter: parent.verticalCenter
                            width: _row.owned ? _play.implicitWidth : _notHere.implicitWidth
                            height: _row.owned ? _play.implicitHeight : _notHere.implicitHeight

                            Text {
                                id: _notHere

                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                visible: !_row.owned
                                text: qsTr("Not in library")
                                color: S.AppTheme.textDisabled
                                font.pixelSize: S.AppTheme.fs12
                            }

                            Ctrl.AppButton {
                                id: _play

                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                visible: _row.owned
                                size: Ctrl.AppButton.Medium
                                variant: _row.started ? Ctrl.AppButton.Filled
                                                      : Ctrl.AppButton.Outlined
                                text: _row.started ? qsTr("Resume")
                                                   : (_row.watched ? qsTr("Again") : qsTr("Play"))

                                Keys.onLeftPressed: _card.forceActiveFocus(Qt.TabFocusReason)
                                Keys.onDownPressed: root.stepRow(_row.index, 1, 1)
                                Keys.onUpPressed: {
                                    if (!root.stepRow(_row.index, -1, 1))
                                        root.focusAboveTheRows()
                                }

                                onClicked: root.playRequested(_row.handle)
                            }
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: S.AppTheme.controlHeightLarge + S.AppTheme.spacing16
        opacity: Math.min(1, _scroll.contentY / 120)
        color: S.AppTheme.background
    }

}
