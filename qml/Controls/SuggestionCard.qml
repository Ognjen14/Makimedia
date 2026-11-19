pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import com.topicdev.makimedia 1.0
import "../Singletons"

Item {
    id: root

    property var item: ({})
    property int cardWidth: 72
    property bool highlighted: false
    property bool compact: false
    property int compactHeight: 56

    readonly property string kind: root.item.kind || ""
    readonly property bool round: root.kind === "person"
    readonly property bool recent: root.kind === "recent"
    readonly property real artHeight: root.round || root.recent
                                      ? root.cardWidth
                                      : Math.round(root.cardWidth * AppTheme.posterAspectRatio)

    signal clicked()

    implicitWidth: root.cardWidth
    implicitHeight: root.compact ? root.compactHeight : _column.implicitHeight

    component Art: Rectangle {
        id: _art

        property var item: ({})
        property bool round: false
        property bool recent: false
        property bool focusRing: false

        radius: _art.round ? width / 2 : AppTheme.radiusSmall
        color: AppTheme.surfaceVariant

        Text {
            anchors.centerIn: parent
            visible: _art.round
            text: Format.initials(_art.item.name || "")
            color: AppTheme.textDisabled
            font.pixelSize: _art.width < 56 ? AppTheme.fs12 : AppTheme.fs16
            font.weight: Font.Medium
        }

        ThemedIcon {
            anchors.centerIn: parent
            visible: _art.recent
            width: Math.round(_art.width * 0.4)
            height: width
            source: Icons.search
            tintColor: AppTheme.textSecondary
            showPlaceholder: false
        }

        RoundedClip {
            anchors.fill: parent
            radius: _art.radius
            visible: !_art.recent

            Image {
                anchors.fill: parent
                source: {
                    void Metadata.artworkRevision
                    if (_art.round)
                        return Metadata.profileUrl(_art.item.profilePath || "", 185)
                    return Metadata.posterUrl(_art.item.posterPath || "", 185)
                }
                sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: _art.round ? width / 2 : _art.radius + 3
            color: "transparent"
            border.width: 2
            border.color: AppTheme.focus
            visible: _art.focusRing
        }
    }

    Column {
        id: _column

        visible: !root.compact
        width: root.cardWidth
        spacing: AppTheme.spacing4

        Art {
            item: root.item
            round: root.round
            recent: root.recent
            width: root.cardWidth
            height: root.artHeight
            focusRing: root.highlighted
        }

        Text {
            width: parent.width
            horizontalAlignment: root.round || root.recent ? Text.AlignHCenter
                                                           : Text.AlignLeft
            text: root.item.title || ""
            color: root.highlighted ? AppTheme.textPrimary : AppTheme.textSecondary
            font.pixelSize: AppTheme.fs12
            font.weight: root.highlighted ? Font.Medium : Font.Normal
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            horizontalAlignment: root.round || root.recent ? Text.AlignHCenter
                                                           : Text.AlignLeft
            visible: text.length > 0
            text: root.item.subtitle || ""
            color: AppTheme.textDisabled
            font.pixelSize: AppTheme.fs11
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: _pill

        visible: root.compact
        width: root.cardWidth
        height: root.compactHeight
        radius: AppTheme.radiusSmall
        color: root.highlighted ? AppTheme.primary : AppTheme.surfaceVariant
        border.width: root.highlighted ? 2 : 0
        border.color: AppTheme.focus

        Art {
            id: _pillArt

            item: root.item
            round: root.round
            recent: root.recent

            readonly property int side: root.compactHeight - 2 * AppTheme.spacing6

            anchors.left: parent.left
            anchors.leftMargin: AppTheme.spacing6
            anchors.verticalCenter: parent.verticalCenter
            height: _pillArt.side
            width: root.round || root.recent
                   ? _pillArt.side
                   : Math.round(_pillArt.side / AppTheme.posterAspectRatio)
            color: root.highlighted ? Qt.darker(AppTheme.primary, 1.2) : AppTheme.surface
        }

        Column {
            anchors.left: _pillArt.right
            anchors.right: parent.right
            anchors.leftMargin: AppTheme.spacing8
            anchors.rightMargin: AppTheme.spacing8
            anchors.verticalCenter: parent.verticalCenter
            spacing: AppTheme.spacing2

            Text {
                width: parent.width
                text: root.item.title || ""
                color: root.highlighted ? AppTheme.onPrimaryStrong : AppTheme.textPrimary
                font.pixelSize: AppTheme.fs13
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: root.item.subtitle || ""
                color: root.highlighted ? AppTheme.onPrimaryStrong : AppTheme.textSecondary
                opacity: root.highlighted ? 0.85 : 1
                font.pixelSize: AppTheme.fs11
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
