pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons" as S
import com.topicdev.makimedia 1.0

BottomSheet {
    id: root

    property string handle
    property string videoPath
    property real tmdbId: 0
    property int season: 0
    property int episode: 0

    property string savedName: ""
    property string savedPath: ""
    property string problem: ""

    signal landed(string path, string displayName)

    title: qsTr("Find subtitles")

    function start() {
        root.savedName = ""
        root.savedPath = ""
        root.problem = ""
        SubtitleSearch.find(root.videoPath, root.tmdbId,
                            root.season, root.episode)
        root.open()
    }

    Connections {
        target: SubtitleSearch

        function onSaved(path, displayName) {
            root.problem = ""
            root.savedName = displayName
            root.savedPath = path
            if (root.handle.length > 0)
                Library.attachSubtitle(root.handle, "file:///" + path,
                                       "downloaded")
            root.landed(path, displayName)
        }

        function onFailed(reason) {
            root.problem = reason
        }
    }

    pinned: [
        Rectangle {
            width: parent ? parent.width : 0
            visible: root.savedName.length > 0
            radius: S.AppTheme.radiusLarge
            color: S.AppTheme.surfaceVariant
            border.width: 1
            border.color: S.AppTheme.primary
            implicitHeight: _done.implicitHeight + 2 * S.AppTheme.spacing14

            Row {
                id: _done

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: S.AppTheme.spacing14
                anchors.rightMargin: S.AppTheme.spacing14
                spacing: S.AppTheme.spacing12

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 36
                    height: 36
                    radius: width / 2
                    color: S.AppTheme.primary

                    ThemedIcon {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        source: S.Icons.check
                        tintColor: S.AppTheme.onPrimaryStrong
                        showPlaceholder: false
                    }
                }

                Column {
                    width: parent.width - 36 - parent.spacing
                    spacing: 2

                    Text {
                        width: parent.width
                        text: qsTr("Subtitle added")
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs15
                        font.weight: Font.Bold
                    }

                    Text {
                        width: parent.width
                        text: root.savedName
                        color: S.AppTheme.textPrimary
                        font.pixelSize: S.AppTheme.fs12
                        elide: Text.ElideMiddle
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Beside the film and in its subtitle list. It loads the next time this plays.")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs12
                        wrapMode: Text.Wrap
                    }
                }
            }
        },

        Text {
            width: parent ? parent.width : 0
            visible: root.problem.length > 0
            text: root.problem
            color: S.AppTheme.error
            font.pixelSize: S.AppTheme.fs13
            wrapMode: Text.Wrap
        },

        Row {
            visible: SubtitleSearch.busy
            spacing: S.AppTheme.spacing10

            BusyIndicator {
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22
                running: SubtitleSearch.busy
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.savedName.length > 0 || root.problem.length > 0
                      ? qsTr("Downloading…")
                      : qsTr("Looking…")
                color: S.AppTheme.textSecondary
                font.pixelSize: S.AppTheme.fs14
            }
        }
    ]

    Column {
        width: parent.width
        spacing: S.AppTheme.spacing10

        Text {
            width: parent.width
            visible: !SubtitleSearch.busy
                     && SubtitleSearch.results.length === 0
                     && root.problem.length === 0
                     && root.savedName.length === 0
            text: qsTr("Nothing found. A different release of the same film may still have one.")
            color: S.AppTheme.textSecondary
            font.pixelSize: S.AppTheme.fs13
            wrapMode: Text.Wrap
        }

        SectionLabel {
            width: parent.width
            visible: SubtitleSearch.results.length > 0
            topPadding: root.savedName.length > 0 ? S.AppTheme.spacing8 : 0
            text: root.savedName.length > 0 ? qsTr("Add another")
                                            : qsTr("%n found", "",
                                                   SubtitleSearch.results.length)
        }

        Repeater {
            model: SubtitleSearch.results

            delegate: SheetRow {
                required property var modelData
                required property int index

                width: parent.width
                showRadio: false
                enabled: !SubtitleSearch.busy
                title: modelData.language.toUpperCase() + "  ·  "
                       + (modelData.release.length > 0 ? modelData.release
                                                       : modelData.name)
                subtitle: {
                    const bits = []
                    if (modelData.hashMatched)
                        bits.push(qsTr("matches this exact file"))
                    if (modelData.downloads > 0)
                        bits.push(qsTr("%1 downloads").arg(modelData.downloads))
                    return bits.join("  ·  ")
                }
                onClicked: SubtitleSearch.fetch(index)
            }
        }

        Text {
            width: parent.width
            visible: SubtitleSearch.downloadsLeft >= 0
            topPadding: S.AppTheme.spacing8
            text: qsTr("%n download(s) left today", "", SubtitleSearch.downloadsLeft)
            color: S.AppTheme.textDisabled
            font.pixelSize: S.AppTheme.fs12
        }
    }
}
