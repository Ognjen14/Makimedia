import QtQuick
import QtQuick.Window
import com.topicdev.makimedia 1.0
import "../Singletons"

Column {
    id: root

    property var person: ({})
    property int faceSize: 72
    property int nameSize: AppTheme.fs12
    property int roleSize: AppTheme.fs11

    spacing: AppTheme.spacing6

    Rectangle {
        id: _face

        anchors.horizontalCenter: parent.horizontalCenter
        width: root.faceSize
        height: root.faceSize
        radius: width / 2
        color: AppTheme.surfaceVariant

        Text {
            anchors.centerIn: parent
            text: Format.initials(root.person.name)
            color: AppTheme.textDisabled
            font.pixelSize: AppTheme.fs16
            font.weight: Font.Medium
        }

        RoundedClip {
            anchors.fill: parent
            radius: _face.radius

            Image {
                anchors.fill: parent
                source: {
                    void Metadata.artworkRevision
                    return Metadata.profileUrl(root.person.profilePath || "", 185)
                }
                sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
            }
        }
    }

    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        text: root.person.name || ""
        color: AppTheme.textPrimary
        font.pixelSize: root.nameSize
        elide: Text.ElideRight
    }

    Text {
        width: parent.width
        horizontalAlignment: Text.AlignHCenter
        visible: text.length > 0
        text: root.person.role || ""
        color: AppTheme.textSecondary
        font.pixelSize: root.roleSize
        elide: Text.ElideRight
    }
}
