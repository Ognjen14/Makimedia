pragma ComponentBehavior: Bound

import QtQuick
import com.topicdev.makimedia 1.0
import "../Singletons"

FocusScope {
    id: root

    property var pick: null
    property bool searched: false

    readonly property bool hasPick: pick !== null && pick !== undefined
                                    && (pick.mediaId || 0) > 0
    readonly property int count: 1
    property int currentIndex: 0

    signal surpriseRequested()
    signal playRequested(var pick)
    signal detailsRequested(var pick)
    signal leftEdgeReached()
    signal upRequested()
    signal downRequested()

    function buttons() {
        if (!hasPick)
            return [_surprise]
        return _play.visible ? [_surprise, _play, _details] : [_surprise, _details]
    }

    function playText() {
        if (!hasPick || pick.kind !== "tv")
            return qsTr("Play")
        return Format.nextUpLabel(pick.playSeason, pick.playEpisode, pick.resuming)
    }

    function focusButton(index) {
        const list = buttons()
        const clamped = Math.max(0, Math.min(index, list.length - 1))
        list[clamped].forceActiveFocus(Qt.TabFocusReason)
    }

    function focusedIndex() {
        const list = buttons()
        for (let i = 0; i < list.length; ++i) {
            if (list[i].activeFocus)
                return i
        }
        return 0
    }

    function infoLine() {
        if (!hasPick)
            return ""
        const bits = []
        if (pick.year > 0)
            bits.push(String(pick.year))
        if (pick.kind === "tv") {
            if (pick.seasonCount > 0)
                bits.push(qsTr("%n season(s)", "", pick.seasonCount))
        } else if (pick.runtimeMinutes > 0) {
            bits.push(Format.runtime(pick.runtimeMinutes))
        }
        if ((pick.genres || "").length > 0)
            bits.push(pick.genres)
        return bits.join(" · ")
    }

    onActiveFocusChanged: {
        if (activeFocus && !_surprise.activeFocus && !_play.activeFocus
                && !_details.activeFocus)
            focusButton(0)
    }

    Keys.onLeftPressed: {
        const at = focusedIndex()
        if (at === 0)
            root.leftEdgeReached()
        else
            focusButton(at - 1)
    }
    Keys.onRightPressed: focusButton(focusedIndex() + 1)
    Keys.onUpPressed: root.upRequested()
    Keys.onDownPressed: root.downRequested()

    implicitHeight: _card.implicitHeight

    Rectangle {
        id: _card

        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: _content.implicitHeight + 2 * AppTheme.spacing16
        height: implicitHeight
        radius: AppTheme.radiusLarge
        color: AppTheme.surface
        border.width: 1
        border.color: AppTheme.outline

        Column {
            id: _content

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: AppTheme.spacing16
            spacing: AppTheme.spacing14

            Row {
                spacing: AppTheme.spacing10

                AppButton {
                    id: _surprise

                    variant: AppButton.Filled
                    size: AppButton.Medium
                    text: qsTr("Surprise me")
                    onClicked: root.surpriseRequested()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("unwatched only")
                    color: AppTheme.textSecondary
                    font.pixelSize: AppTheme.fs13
                }
            }

            Text {
                width: parent.width
                visible: root.searched && !root.hasPick
                text: qsTr("Nothing unwatched is left in the library.")
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Row {
                width: parent.width
                visible: root.hasPick
                spacing: AppTheme.spacing16

                Rectangle {
                    id: _poster

                    width: 120
                    height: width * AppTheme.posterAspectRatio
                    radius: AppTheme.radiusMedium
                    color: AppTheme.surfaceVariant

                    RoundedClip {
                        anchors.fill: parent
                        radius: _poster.radius

                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize.width: 256
                            source: {
                                void Metadata.artworkRevision
                                return root.hasPick
                                       ? Metadata.posterUrl(root.pick.posterPath || "", 342)
                                       : ""
                            }
                        }
                    }
                }

                Column {
                    width: parent.width - _poster.width - parent.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: AppTheme.spacing8

                    Text {
                        width: parent.width
                        text: root.hasPick ? root.pick.title : ""
                        color: AppTheme.textPrimary
                        font.pixelSize: AppTheme.fs18
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    Text {
                        width: parent.width
                        text: root.infoLine()
                        color: AppTheme.textSecondary
                        font.pixelSize: AppTheme.fs13
                        wrapMode: Text.Wrap
                    }

                    Row {
                        spacing: AppTheme.spacing10
                        topPadding: AppTheme.spacing6

                        AppButton {
                            id: _play

                            variant: AppButton.Filled
                            size: AppButton.Medium
                            visible: root.hasPick && (root.pick.playHandle || "").length > 0
                            text: root.playText()
                            onClicked: root.playRequested(root.pick)
                        }

                        AppButton {
                            id: _details

                            size: AppButton.Medium
                            text: qsTr("Details")
                            onClicked: root.detailsRequested(root.pick)
                        }
                    }
                }
            }
        }
    }
}
