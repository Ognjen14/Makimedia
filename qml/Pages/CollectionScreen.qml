pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Window
import "../Singletons" as S
import "../Controls" as Ctrl
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
        _back.forceActiveFocus(Qt.TabFocusReason)
    }

    readonly property var page: Library.collectionPage
    readonly property var info: page.info
    readonly property bool wide: width >= 720
    readonly property real gutter: wide ? S.AppTheme.spacing24 : S.AppTheme.spacing16
    readonly property int heroHeight: wide ? Math.max(420, Math.min(560, Math.round(height * 0.62)))
                                           : Math.max(440, Math.round(height * 0.7))

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

                HeroBackdrop {
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
                            font.pixelSize: root.wide ? S.AppTheme.fs48 : S.AppTheme.fs28
                            font.weight: Font.Bold
                            font.capitalization: Font.AllUppercase
                            lineHeight: 0.95
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }

                        Text {
                            width: Math.min(parent.width, 620)
                            visible: text.length > 0
                            text: root.info.description || ""
                            color: S.AppTheme.textPrimary
                            font.pixelSize: S.AppTheme.fs14
                            wrapMode: Text.Wrap
                            maximumLineCount: root.wide ? 2 : 3
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
                                visible: (root.info.nextHandle || "").length > 0
                                variant: Ctrl.AppButton.Filled
                                iconSource: S.Icons.play
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
                                visible: (root.info.owned || 0) > 0
                                variant: Ctrl.AppButton.Outlined
                                iconSource: root.info.allOwnedWatched === true ? "" : S.Icons.check
                                text: root.info.allOwnedWatched === true
                                      ? qsTr("Mark all unwatched")
                                      : qsTr("Mark all watched")

                                onClicked: Library.setAllWatched(root.info.ownedHandles || [],
                                                                 root.info.allOwnedWatched !== true)
                            }

                            Ctrl.AppButton {
                                id: _addFilm

                                visible: !System.isTelevision
                                         && (root.info.tag || "") !== "custom"
                                         && !Streaming.connected
                                variant: Ctrl.AppButton.Outlined
                                iconSource: S.Icons.plus
                                text: qsTr("Add a film")

                                onClicked: {
                                    Library.pickerKind = 1
                                    Library.pickerQuery = ""
                                    _pickerSearch.text = ""
                                    _addFilmSheet.open()
                                }
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
                        onClicked: root.page.order = 0
                    }

                    Ctrl.PillChip {
                        text: qsTr("Story")
                        selected: root.page.order === 1
                        onClicked: root.page.order = 1
                    }
                }
            }

            Repeater {
                model: root.page

                delegate: Item {
                    id: _row

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
                    required property bool added
                    required property string handle
                    required property bool watched
                    required property real progress
                    required property int leftMinutes
                    required property int artworkStamp
                    required property var tmdbId

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
                        border.width: _row.owned ? 1 : 0
                        border.color: S.AppTheme.outline

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
                            width: _actions.implicitWidth
                            height: _actions.implicitHeight

                            Row {
                                id: _actions

                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: S.AppTheme.spacing12

                                Text {
                                    id: _notHere

                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: !_row.owned
                                    text: qsTr("Not in library")
                                    color: S.AppTheme.textDisabled
                                    font.pixelSize: S.AppTheme.fs12
                                }

                                Ctrl.AppButton {
                                    id: _playRow

                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: _row.owned
                                    size: Ctrl.AppButton.Medium
                                    variant: _row.started ? Ctrl.AppButton.Filled
                                                          : Ctrl.AppButton.Outlined
                                    text: _row.started
                                          ? qsTr("Resume")
                                          : (_row.watched ? qsTr("Again") : qsTr("Play"))

                                    onClicked: root.playRequested(_row.handle)
                                }

                                Ctrl.AppButton {
                                    id: _forgetFilm

                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: !System.isTelevision && !Streaming.connected
                                             && (!_row.owned || _row.added)
                                    iconOnly: true
                                    size: Ctrl.AppButton.Medium
                                    variant: Ctrl.AppButton.Outlined
                                    iconSource: S.Icons.close
                                    text: _row.added
                                          ? qsTr("Take out of this collection")
                                          : qsTr("Remove from this collection")
                                    onClicked: Library.hideCollectionFilm(
                                                   root.collectionId, _row.tmdbId, _row.title)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Ctrl.IconButton {
        id: _back

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: S.AppTheme.spacing4
        opacity: Math.max(0, Math.min(1, (root.heroHeight - _scroll.contentY) / 80))
        visible: opacity > 0.01
        iconSource: S.Icons.chevronLeft
        tintColor: "#FFFFFF"
        scrim: true
        accessibleName: qsTr("Back")
        onClicked: root.backRequested()
    }

    Ctrl.BottomSheet {
        id: _addFilmSheet

        z: 60
        title: qsTr("Add a film to %1").arg(root.info.name || qsTr("this collection"))

        onClosed: Library.pickerQuery = ""

        pinned: [
            Ctrl.SearchBar {
                id: _pickerSearch

                width: parent ? parent.width : 0
                placeholder: qsTr("Film in your library")
                onTextChanged: Library.pickerQuery = text
            }
        ]

        Column {
            width: parent.width
            spacing: 0

            ListView {
                id: _pickList

                width: parent.width
                height: Math.min(420, count * S.AppTheme.touchTargetMinimum)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true

                model: _addFilmSheet.opened ? Library.pickerTitles : null

                ScrollBar.vertical: Ctrl.AppScrollBar {}

                delegate: Item {
                    id: _pick

                    required property var tmdbId
                    required property string title
                    required property int year
                    required property string posterPath
                    required property int artworkStamp

                    width: _pickList.width
                    height: _pickRow.height

                    Ctrl.ListRow {
                        id: _pickRow

                        width: parent.width
                        leading: Ctrl.ListRow.Thumbnail
                        thumbnailSource: {
                            void _pick.artworkStamp
                            return Metadata.posterUrl(_pick.posterPath, 154)
                        }
                        iconSource: S.Icons.movies
                        title: _pick.title
                        subtitle: _pick.year > 0 ? String(_pick.year) : ""
                        trailingText: qsTr("Add")

                        onClicked: {
                            Library.addFilmToCollection(root.collectionId, _pick.tmdbId,
                                                        _pick.title)
                            _addFilmSheet.close()
                        }
                    }
                }
            }
        }
    }
}
