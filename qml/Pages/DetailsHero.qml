import QtQuick
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    property real pageHeight: 0
    property real scrollY: 0

    property string backdropPath: ""
    property string posterPath: ""
    property string title: ""
    property string kicker: ""
    property string metaText: ""

    readonly property bool wide: width >= 720

    readonly property int fullHeight: {
        const floor = Math.min(280, Math.max(216, width * 0.40))
        return Math.round(Math.max(floor,
                                   Math.min(width * 0.34, pageHeight * 0.50, 640)))
    }

    readonly property int posterWidth:
        Math.round((fullHeight - 2 * S.AppTheme.spacing16)
                   / S.AppTheme.posterAspectRatio)

    readonly property real fade:
        Math.max(0, Math.min(1, (fullHeight - scrollY) / 80))

    y: -scrollY
    height: fullHeight
    clip: true

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: S.AppTheme.surfaceVariant }
            GradientStop { position: 1.0; color: S.AppTheme.surface }
        }
    }

    HeroBackdrop {
        anchors.fill: parent
        source: {
            void Metadata.artworkRevision
            return Metadata.backdropUrl(root.backdropPath, root.wide ? 1280 : 780)
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.35; color: Qt.rgba(
                S.AppTheme.background.r, S.AppTheme.background.g,
                S.AppTheme.background.b, 0.55) }
            GradientStop { position: 1.0; color: S.AppTheme.background }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: S.AppTheme.controlHeightLarge + S.AppTheme.spacing16
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.45) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: S.AppTheme.spacing16
        anchors.rightMargin: S.AppTheme.spacing16
        anchors.bottomMargin: S.AppTheme.spacing16
        height: _poster.height

        Rectangle {
            id: _poster

            anchors.left: parent.left
            anchors.bottom: parent.bottom
            width: root.posterWidth
            height: Math.min(root.height - S.AppTheme.spacing32,
                             width * S.AppTheme.posterAspectRatio)
            radius: S.AppTheme.radiusSmall
            color: S.AppTheme.surfaceVariant

            Ctrl.RoundedClip {
                anchors.fill: parent
                radius: _poster.radius

                HeldImage {
                    anchors.fill: parent
                    source: {
                        void Metadata.artworkRevision
                        return Metadata.posterUrl(root.posterPath,
                                                  root.posterWidth >= 190 ? 500 : 342)
                    }
                }
            }
        }

        Column {
            anchors.left: _poster.right
            anchors.leftMargin: S.AppTheme.spacing16
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            spacing: S.AppTheme.spacing6

            Text {
                width: parent.width
                text: root.title
                color: S.AppTheme.textPrimary
                font.pixelSize: root.wide ? S.AppTheme.fs38 : S.AppTheme.fs28
                font.weight: Font.Bold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: root.kicker.length > 0
                text: root.kicker
                color: S.AppTheme.primary
                font.pixelSize: root.wide ? S.AppTheme.fs15 : S.AppTheme.fs13
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.metaText
                color: S.AppTheme.textPrimary
                font.pixelSize: root.wide ? S.AppTheme.fs15 : S.AppTheme.fs13
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }
}
