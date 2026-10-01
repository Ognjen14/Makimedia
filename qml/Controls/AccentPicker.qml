import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import "../Singletons"
import com.topicdev.makimedia 1.0

Item {
    id: root
    implicitHeight: _col.implicitHeight + 32

    readonly property int columns: 6
    Rectangle {
        anchors.left:        parent.left
        anchors.leftMargin:  16
        anchors.right:       parent.right
        anchors.rightMargin: 16
        height:              root.implicitHeight
        radius:              16
        color:               AppTheme.surface
        Behavior on color        { ColorAnimation { duration: 160 } }

    Column {
        id: _col
        anchors.verticalCenter: parent.verticalCenter
        anchors.left:           parent.left
        anchors.right:          parent.right
        anchors.leftMargin:     16
        anchors.rightMargin:    16
        spacing:                14

        Row {
            width: parent.width

            Text {
                id: _label
                text:           qsTr("Accent color")
                font.pixelSize: AppTheme.fs13
                font.weight:    Font.Medium
                color:          AppTheme.textPrimary
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 160 } }
            }

            Item { width: parent.width - _label.implicitWidth - _pill.width; height: 1 }

            Rectangle {
                id: _pill

                readonly property var accent: AppTheme.accentPresets[AppTheme.safeAccentIndex]
                readonly property color swatchColor:
                    AppTheme.darkMode ? accent.dark : accent.light
                readonly property color onColor:
                    AppTheme.darkMode ? accent.darkOn : accent.lightOn

                height: 24
                width:  _pillText.implicitWidth + 20
                radius: 12
                color:  swatchColor
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 200 } }

                Text {
                    id:             _pillText
                    anchors.centerIn: parent
                    text:           AppTheme.accentNames[AppTheme.safeAccentIndex]
                    font.pixelSize: AppTheme.fs11
                    font.weight:    Font.DemiBold
                    color:          _pill.onColor
                }
            }
        }

        Grid {
            readonly property real _maxWidth: root.columns * 44 + (root.columns - 1) * 10

            width:   Math.min(parent.width, _maxWidth)
            anchors.horizontalCenter: parent.horizontalCenter
            columns: root.columns
            spacing: 10

            Repeater {
                model: AppTheme.accentPresets.length

                Item {
                    id: _cell

                    required property int index

                    readonly property var accent: AppTheme.accentPresets[index]
                    readonly property color swatchColor:
                        AppTheme.darkMode ? accent.dark : accent.light
                    readonly property color onColor:
                        AppTheme.darkMode ? accent.darkOn : accent.lightOn
                    readonly property bool _selected: AppTheme.safeAccentIndex === index

                    width:  (parent.width - (root.columns - 1) * 10) / root.columns
                    height: width + 4

                    Rectangle {
                        id:     _circle
                        width:  _cell._selected ? parent.width : parent.width - 8
                        height: width
                        radius: width / 2
                        color:  _cell.swatchColor
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top:              parent.top

                        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutBack; easing.overshoot: 0.5 } }

                        layer.enabled: _cell._selected
                        layer.effect: DropShadow {
                            verticalOffset: 3; radius: 10; samples: 21
                            color: Qt.rgba(_cell.swatchColor.r, _cell.swatchColor.g, _cell.swatchColor.b, 0.55)
                        }

                        Image {
                            id:       _checkImg
                            anchors.centerIn: parent
                            width:    14; height: 14
                            source:   "qrc:/assets/check.png"
                            fillMode: Image.PreserveAspectFit
                            smooth:   true
                            visible:  false
                        }
                        ColorOverlay {
                            anchors.fill: _checkImg
                            source:       _checkImg
                            color:        _cell.onColor
                            opacity:      _cell._selected ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 160 } }
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom:           parent.bottom
                        width:  _cell._selected ? 5 : 0
                        height: 5
                        radius: 3
                        color:  _cell.swatchColor
                        Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        Behavior on color { ColorAnimation  { duration: 200 } }
                    }

                    TapHandler { onTapped: AppSettings.accentIndex = _cell.index }
                }
            }
        }
    }
}
}