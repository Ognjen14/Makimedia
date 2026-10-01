pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property bool showNotice: false
    property string caption: qsTr("Metadata from TMDB")

    readonly property url website: "https://www.themoviedb.org"

    implicitWidth: parent ? parent.width : _column.implicitWidth
    implicitHeight: _column.implicitHeight

    Accessible.role: Accessible.Link
    Accessible.name: qsTr("The Movie Database")

    Column {
        id: _column

        width: root.width
        spacing: AppTheme.spacing8

        Row {
            spacing: AppTheme.spacing8

            Image {
                id: _logo

                anchors.verticalCenter: parent.verticalCenter
                width: 92
                height: Math.round(width * 35.52 / 273.42)
                source: Icons.tmdbLogo
                sourceSize.width: width * 3
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.caption.length > 0
                text: root.caption
                color: AppTheme.textDisabled
                font.pixelSize: AppTheme.fs11
            }
        }

        Text {
            width: parent.width
            visible: root.showNotice
            text: qsTr("This product uses the TMDB API but is not endorsed or certified by TMDB.")
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fs12
            wrapMode: Text.Wrap
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: Qt.openUrlExternally(root.website)
    }
}
