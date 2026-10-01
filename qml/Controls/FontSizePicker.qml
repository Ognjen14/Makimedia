import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Qt5Compat.GraphicalEffects
import "../Singletons"
import com.topicdev.makimedia 1.0

Item {
    id: root
    implicitHeight: _card.height

    readonly property var _steps: [
        { label: qsTr("Small"),   scale: 0.90 },
        { label: qsTr("Default"), scale: 1.00 },
        { label: qsTr("Large"),   scale: 1.15 },
        { label: qsTr("XL"),      scale: 1.30 }
    ]

    function _indexForScale(s) {
        for (var i = 0; i < _steps.length; i++)
            if (Math.abs(_steps[i].scale - s) < 0.001) return i
        return 1
    }

    Rectangle {
        id: _card
        anchors.left:        parent.left
        anchors.leftMargin:  16
        anchors.right:       parent.right
        anchors.rightMargin: 16
        height:       _col.implicitHeight + 28
        radius:       16
        color:        AppTheme.surface
        Behavior on color        { ColorAnimation { duration: 160 } }

        Column {
            id: _col
            anchors.left:        parent.left
            anchors.right:       parent.right
            anchors.top:         parent.top
            anchors.leftMargin:  16
            anchors.rightMargin: 16
            anchors.topMargin:   14
            spacing: 14

            Item {
                width:  parent.width
                height: Math.max(_title.implicitHeight, _preview.implicitHeight)

                Text {
                    id: _title
                    anchors.left:           parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text:           qsTr("Text size")
                    font.pixelSize: AppTheme.fs14
                    font.weight:    Font.Medium
                    color:          AppTheme.textPrimary
                    Behavior on color { ColorAnimation { duration: 160 } }
                }
                Text {
                    id: _preview
                    anchors.right:          parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text:           qsTr("Aa")
                    font.pixelSize: AppTheme.fs22
                    font.weight:    Font.DemiBold
                    color:          AppTheme.primary
                    Behavior on color { ColorAnimation { duration: 160 } }
                }
            }

            Slider {
                id: _slider
                width:       parent.width
                height:      34
                from:        0
                to:          3
                stepSize:    1
                snapMode:    Slider.SnapAlways
                padding:     0

                readonly property real _hw: 22

                Component.onCompleted: value = root._indexForScale(AppSettings.fontSizeScale)
                onMoved: {
                    AppSettings.fontSizeScale = root._steps[Math.round(value)].scale
                    AppTheme.fontScale = AppSettings.fontSizeScale
                }

                background: Item {
                    x:      _slider.leftPadding
                    y:      _slider.topPadding + (_slider.availableHeight - height) / 2
                    width:  _slider.availableWidth
                    height: 22

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        x:      _slider._hw / 2
                        width:  parent.width - _slider._hw
                        height: 4
                        radius: 2
                        color:  AppTheme.outline
                        Behavior on color { ColorAnimation { duration: 160 } }

                        Rectangle {
                            height: parent.height
                            radius: 2
                            width:  (_slider.value / 3) * parent.width
                            color:  AppTheme.primary
                        }
                    }

                    Repeater {
                        model: 4
                        Rectangle {
                            width:  10
                            height: 10
                            radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            x: _slider._hw / 2 + (index / 3) * (parent.width - _slider._hw) - width / 2
                            color:        index <= Math.round(_slider.value) ? AppTheme.primary : AppTheme.outlineStrong
                            border.color: AppTheme.surfaceRaised
                            border.width: 2
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                    }
                }

                handle: Rectangle {
                    x:      _slider.leftPadding + _slider.visualPosition * (_slider.availableWidth - width)
                    y:      _slider.topPadding + (_slider.availableHeight - height) / 2
                    width:  22
                    height: 22
                    radius: 11
                    color:        _slider.pressed ? Qt.darker(AppTheme.primary, 1.12) : AppTheme.primary
                    border.color: AppTheme.surface
                    border.width: 3
                    Behavior on color { ColorAnimation { duration: 80 } }
                    layer.enabled: true
                    layer.effect: DropShadow {
                        verticalOffset: 2; radius: 7; samples: 15
                        color: Qt.rgba(0, 0, 0, AppTheme.darkMode ? 0.5 : 0.25)
                    }
                }
            }

            Item {
                width:  parent.width
                height: _lblProbe.implicitHeight

                Text { id: _lblProbe; visible: false; text: "Xy"; font.pixelSize: AppTheme.fs11 }

                Repeater {
                    model: 4
                    Text {
                        readonly property real _cx: _slider._hw / 2 + (index / 3) * (parent.width - _slider._hw)
                        x: Math.max(0, Math.min(parent.width - width, _cx - width / 2))
                        text:           root._steps[index].label
                        font.pixelSize: AppTheme.fs11
                        font.weight:    index === Math.round(_slider.value) ? Font.DemiBold : Font.Normal
                        color:          index === Math.round(_slider.value) ? AppTheme.primary : AppTheme.textDisabled
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }
            }
        }
    }
}
