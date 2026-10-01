import QtQuick
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property url source

    property bool tintEnabled: true
    property color tintColor: "black"
    property alias effectCached: _colorOverlay.cached

    property alias fillMode: _sourceImage.fillMode
    property alias sourceSize: _sourceImage.sourceSize
    property alias asynchronous: _sourceImage.asynchronous
    property alias cache: _sourceImage.cache
    property alias smooth: _sourceImage.smooth
    property alias mipmap: _sourceImage.mipmap

    property bool showPlaceholder: true
    property string placeholderText: "SVG"
    property color placeholderBackground: "transparent"
    property color placeholderBorderColor: "black"
    property color placeholderTextColor: "black"
    property real placeholderRadius: 5
    property real placeholderBorderWidth: 1
    property int placeholderFontSize: 7

    property alias imageItem: _sourceImage
    property alias colorOverlayItem: _colorOverlay
    property alias placeholderItem: _placeholder
    property alias placeholderLabel: _placeholderLabel

    readonly property bool hasSource: source.toString().length > 0
    readonly property bool sourceFailed: _sourceImage.status === Image.Error
    readonly property int imageStatus: _sourceImage.status

    implicitWidth: 24
    implicitHeight: 24

    Image {
        id: _sourceImage

        anchors.fill: parent
        source: root.source
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        visible: root.hasSource && !root.sourceFailed && !root.tintEnabled
    }

    ColorOverlay {
        id: _colorOverlay

        anchors.fill: _sourceImage
        source: _sourceImage
        color: root.tintColor
        visible: root.hasSource && !root.sourceFailed && root.tintEnabled
        cached: true
    }

    Rectangle {
        id: _placeholder

        anchors.fill: parent
        visible: root.showPlaceholder && (!root.hasSource || root.sourceFailed)
        radius: root.placeholderRadius
        color: root.placeholderBackground
        border.width: root.placeholderBorderWidth
        border.color: root.placeholderBorderColor

        Text {
            id: _placeholderLabel

            anchors.centerIn: parent
            text: root.placeholderText
            color: root.placeholderTextColor
            font.pixelSize: root.placeholderFontSize
            font.weight: Font.DemiBold
        }
    }
}
