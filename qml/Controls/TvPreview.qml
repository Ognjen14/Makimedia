pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import Qt5Compat.GraphicalEffects
import "../Singletons"
import "../Pages/TV" as Tv
import com.topicdev.makimedia 1.0

Item {
    id: root

    property int playLimitMs: 30000
    property int firstFrameLimitMs: 14000
    property int soundVolume: 84

    property var info: ({})
    property bool open: false
    property bool videoShown: false
    property bool ended: false
    readonly property bool waiting: open && !videoShown && !ended
    property int _ticks: 0

    readonly property real frameWidth: Math.round(width * 0.56)
    readonly property real frameHeight: Math.round(frameWidth * 9 / 16)
    readonly property real frameX: width - frameWidth - Math.round(width * 0.05)
    readonly property real frameY: Math.round((height - frameHeight) / 2 - height * 0.02)
    readonly property real frameRadius: Math.round(height * 0.018)
    readonly property real sidePadding: Math.round(width * 0.06)

    readonly property real progress: {
        if (!videoShown || !info.start)
            return 0
        return Math.max(0, Math.min(1, (PreviewPlayer.position - info.start)
                                        / (playLimitMs / 1000)))
    }

    Component.onCompleted: PreviewPlayer.volume = root.soundVolume

    Binding {
        target: PreviewPlayer
        property: "muted"
        value: !AppSettings.tvPreviewSound
    }

    function placeSurface() {
        const ratio = Screen.devicePixelRatio
        const p = _frame.mapToItem(null, 0, 0)
        PreviewPlayer.setSurfaceFrame(Math.round(p.x * ratio), Math.round(p.y * ratio),
                                      Math.round(_frame.width * ratio),
                                      Math.round(_frame.height * ratio))
    }

    function start() {
        _limit.stop()
        root.videoShown = false
        root.ended = false
        root._ticks = 0
        if (!HoverPreview.anchor || !HoverPreview.mediaId) {
            root.finish()
            return
        }
        const found = Library.previewFor(HoverPreview.mediaId)
        if (!found.url) {
            HoverPreview.close()
            return
        }
        root.info = found
        root.open = true
        root.placeSurface()
        PreviewPlayer.setStartForNextOpen(found.start)
        PreviewPlayer.openPath(found.url)
        _firstFrame.restart()
    }

    function finish() {
        _limit.stop()
        _firstFrame.stop()
        if (root.open) {
            PreviewPlayer.stop()
            PreviewPlayer.setSurfaceFrame(0, 0, 0, 0)
        }
        root.open = false
        root.videoShown = false
    }

    function runtimeText(minutes) {
        if (!minutes || minutes <= 0)
            return ""
        const hours = Math.floor(minutes / 60)
        const rest = minutes % 60
        if (hours === 0)
            return qsTr("%1 min").arg(rest)
        return rest > 0 ? qsTr("%1 h %2 min").arg(hours).arg(rest) : qsTr("%1 h").arg(hours)
    }

    Connections {
        target: HoverPreview

        function onMediaIdChanged() {
            if (HoverPreview.mediaId)
                root.start()
            else
                root.finish()
        }

        function onAnchorChanged() {
            if (!HoverPreview.anchor)
                root.finish()
        }
    }

    Connections {
        target: PreviewPlayer
        enabled: root.open && !root.videoShown

        function onPositionChanged() {
            if (!PreviewPlayer.fileLoaded
                    || Math.abs(PreviewPlayer.position - (root.info.start || 0)) > 20)
                return
            root._ticks += 1
            if (root._ticks >= 2) {
                root.videoShown = true
                _firstFrame.stop()
                _limit.restart()
            }
        }
    }

    onWidthChanged: if (open) placeSurface()
    onHeightChanged: if (open) placeSurface()

    Timer {
        id: _firstFrame

        interval: root.firstFrameLimitMs
        onTriggered: {
            if (root.open && !root.videoShown) {
                Library.noteUi("tv preview showed no picture in "
                               + root.firstFrameLimitMs + " ms, closing it")
                HoverPreview.close()
            }
        }
    }

    Timer {
        id: _limit

        interval: root.playLimitMs
        onTriggered: {
            root.ended = true
            root.videoShown = false
            PreviewPlayer.stop()
        }
    }

    Item {
        id: _scene

        anchors.fill: parent
        visible: opacity > 0
        opacity: root.open ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        Item {
            id: _backdropLayer

            anchors.fill: parent
            layer.enabled: true
            layer.effect: OpacityMask {
                invert: true
                maskSource: _hole
            }

            Rectangle {
                anchors.fill: parent
                color: AppTheme.background
            }

            Tv.TvAmbientBackdrop {
                source: {
                    void Metadata.artworkRevision
                    if (!root.open)
                        return ""
                    return root.info.backdropPath
                           ? Metadata.backdropUrl(root.info.backdropPath, 780) : ""
                }
            }

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0.0
                        color: Qt.rgba(AppTheme.background.r, AppTheme.background.g,
                                       AppTheme.background.b, 0.92)
                    }
                    GradientStop {
                        position: 0.45
                        color: Qt.rgba(AppTheme.background.r, AppTheme.background.g,
                                       AppTheme.background.b, 0.7)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.rgba(AppTheme.background.r, AppTheme.background.g,
                                       AppTheme.background.b, 0.35)
                    }
                }
            }
        }

        Item {
            id: _hole

            anchors.fill: parent
            visible: false

            Rectangle {
                x: root.frameX
                y: root.frameY
                width: root.frameWidth
                height: root.frameHeight
                radius: root.frameRadius
            }
        }

        Item {
            id: _frame

            x: root.frameX
            y: root.frameY
            width: root.frameWidth
            height: root.frameHeight

            RoundedClip {
                anchors.fill: parent
                radius: root.frameRadius
                opacity: root.videoShown ? 0 : 1

                Behavior on opacity { NumberAnimation { duration: 300 } }

                Rectangle {
                    anchors.fill: parent
                    color: "black"
                }

                Image {
                    id: _still

                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    cache: false
                    sourceSize.width: 1280
                    opacity: status === Image.Ready ? 1 : 0
                    source: {
                        void Metadata.artworkRevision
                        if (!root.open)
                            return ""
                        if (root.info.backdropPath)
                            return Metadata.backdropUrl(root.info.backdropPath, 1280)
                        return root.info.posterPath
                               ? Metadata.posterUrl(root.info.posterPath, 342) : ""
                    }

                    Behavior on opacity { NumberAnimation { duration: 250 } }
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: AppTheme.spacing24
                width: _loadingRow.implicitWidth + 2 * AppTheme.spacing16
                height: _loadingRow.implicitHeight + AppTheme.spacing16
                radius: height / 2
                color: Qt.rgba(0, 0, 0, 0.62)
                opacity: root.waiting ? 1 : 0
                visible: opacity > 0

                Behavior on opacity { NumberAnimation { duration: 200 } }

                Row {
                    id: _loadingRow

                    anchors.centerIn: parent
                    spacing: AppTheme.spacing10

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5

                        Repeater {
                            model: 3

                            delegate: Rectangle {
                                id: _dot

                                required property int index

                                width: 7
                                height: 7
                                radius: 3.5
                                color: AppTheme.primary
                                opacity: 0.3

                                SequentialAnimation on opacity {
                                    running: root.waiting
                                    loops: Animation.Infinite

                                    PauseAnimation { duration: _dot.index * 160 }
                                    NumberAnimation { to: 1; duration: 280 }
                                    NumberAnimation { to: 0.3; duration: 280 }
                                    PauseAnimation { duration: (2 - _dot.index) * 160 }
                                }
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: PreviewPlayer.startNote.length > 0
                              ? PreviewPlayer.startNote : qsTr("Loading preview")
                        color: "white"
                        font.pixelSize: AppTheme.fs16
                        font.weight: Font.Medium
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                radius: root.frameRadius
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.14)
            }

            Rectangle {
                anchors.top: parent.bottom
                anchors.topMargin: AppTheme.spacing16
                anchors.left: parent.left
                anchors.right: parent.right
                height: 4
                radius: 2
                color: Qt.rgba(1, 1, 1, 0.14)
                opacity: root.videoShown ? 1 : 0

                Behavior on opacity { NumberAnimation { duration: 200 } }

                Rectangle {
                    width: parent.width * root.progress
                    height: parent.height
                    radius: parent.radius
                    color: AppTheme.primary
                }
            }
        }

        Column {
            id: _info

            x: root.sidePadding
            width: root.frameX - root.sidePadding - Math.round(root.width * 0.04)
            anchors.verticalCenter: _frame.verticalCenter
            spacing: AppTheme.spacing16

            Text {
                text: qsTr("PREVIEW")
                color: AppTheme.primary
                font.pixelSize: AppTheme.fs14
                font.weight: Font.DemiBold
                font.letterSpacing: 2.5
            }

            Text {
                width: parent.width
                text: root.info.title || ""
                color: AppTheme.textPrimary
                font.pixelSize: Math.round(root.height * 0.048)
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                lineHeight: 1.05
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: {
                    if (!root.info.episode)
                        return ""
                    return root.info.episodeTitle
                           ? root.info.episode + "  ·  " + root.info.episodeTitle
                           : root.info.episode
                }
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fs20
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: {
                    const parts = []
                    if (root.info.year > 0)
                        parts.push(String(root.info.year))
                    const runtime = root.runtimeText(root.info.runtimeMinutes)
                    if (runtime.length > 0 && !root.info.episode)
                        parts.push(runtime)
                    if (root.info.rating > 0)
                        parts.push("★ " + Number(root.info.rating).toFixed(1))
                    const genres = (root.info.genres || "").split(",")
                        .map(g => g.trim()).filter(g => g.length > 0).slice(0, 2)
                    if (genres.length > 0)
                        parts.push(genres.join(", "))
                    return parts.join("   ·   ")
                }
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs18
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: root.info.overview || ""
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs18
                wrapMode: Text.WordWrap
                maximumLineCount: 4
                elide: Text.ElideRight
                lineHeight: 1.25
            }

            Item {
                width: 1
                height: AppTheme.spacing8
            }

            Row {
                spacing: AppTheme.spacing24

                Repeater {
                    model: [
                        { key: qsTr("OK"), label: qsTr("Open") },
                        { key: qsTr("Back"), label: qsTr("Close") }
                    ]

                    delegate: Row {
                        id: _hint

                        required property var modelData

                        spacing: AppTheme.spacing8

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: _key.implicitWidth + 2 * AppTheme.spacing10
                            height: _key.implicitHeight + AppTheme.spacing8
                            radius: AppTheme.radiusSmall
                            color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1
                            border.color: Qt.rgba(1, 1, 1, 0.22)

                            Text {
                                id: _key

                                anchors.centerIn: parent
                                text: _hint.modelData.key
                                color: AppTheme.textPrimary
                                font.pixelSize: AppTheme.fs13
                                font.weight: Font.DemiBold
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: _hint.modelData.label
                            color: AppTheme.textSecondary
                            font.pixelSize: AppTheme.fs16
                        }
                    }
                }
            }
        }
    }
}
