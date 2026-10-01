pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../Singletons"

RemoteButton {
    id: root

    property url artSource
    property var behindSources: []
    property string title
    property string meta
    property int watchedCount: 0
    property int totalCount: 0
    property bool completed: false
    property bool custom: false
    property real progress: 0
    property bool highlighted: false

    readonly property real unit: width / 180
    readonly property real stackTop: Math.round(34 * unit)
    readonly property real stackRight: Math.round(20 * unit)
    readonly property real stackWidth: width - stackRight
    readonly property real stackHeight: stackWidth * 1.5
    readonly property bool fanned: hovered || visualFocus
                                   || (highlighted && AppTheme.remoteNavigation)

    property bool menuEnabled: false

    signal menuRequested()

    padding: 0
    z: fanned ? 1 : 0

    implicitWidth: AppTheme.posterTileWidth
    implicitHeight: stackTop + stackHeight + AppTheme.spacing10
                    + _title.implicitHeight + _meta.implicitHeight + 7 + 3

    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: meta

    background: Item {}

    component Layer: Rectangle {
        id: _layer

        property url source
        property real shiftX: 0
        property real shiftY: 0
        property real dim: 0

        radius: 6
        color: AppTheme.surfaceRaised
        antialiasing: true

        transform: Translate { x: _layer.shiftX; y: _layer.shiftY }

        Behavior on shiftX { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on shiftY { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on rotation { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

        RoundedClip {
            anchors.fill: parent
            radius: _layer.radius
            clipping: _layer.source.toString().length > 0

            Image {
                anchors.fill: parent
                source: _layer.source
                sourceSize.width: Math.ceil(width * Screen.devicePixelRatio / 64) * 64
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: _layer.radius
            color: "black"
            opacity: _layer.dim

            Behavior on opacity { NumberAnimation { duration: 300 } }
        }
    }

    contentItem: Item {
        Item {
            id: _stack

            x: 0
            y: root.stackTop
            width: root.stackWidth
            height: root.stackHeight

            Layer {
                width: root.stackWidth
                height: root.stackHeight
                source: root.behindSources.length > 1 ? root.behindSources[1] : ""
                shiftX: (root.fanned ? 34 : 18) * root.unit
                shiftY: (root.fanned ? -18 : -16) * root.unit
                rotation: root.fanned ? 15 : 7
                dim: root.fanned ? 0.55 : 0.68
            }

            Layer {
                width: root.stackWidth
                height: root.stackHeight
                source: root.behindSources.length > 0 ? root.behindSources[0] : ""
                shiftX: (root.fanned ? 18 : 9) * root.unit
                shiftY: (root.fanned ? -12 : -9) * root.unit
                rotation: root.fanned ? 8 : 3.5
                dim: root.fanned ? 0.3 : 0.45
            }

            Item {
                id: _front

                width: root.stackWidth
                height: root.stackHeight
                rotation: root.fanned ? -2 : 0
                transform: Translate {
                    x: root.fanned ? -4 * root.unit : 0
                    y: root.fanned ? -2 * root.unit : 0

                    Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                    Behavior on y { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
                }

                Behavior on rotation { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

                Repeater {
                    model: 3

                    delegate: Rectangle {
                        required property int index

                        x: -index
                        y: 4 + 3 * index
                        width: _front.width + 2 * index
                        height: _front.height
                        radius: 6 + index
                        color: "black"
                        opacity: 0.22 - 0.06 * index
                    }
                }

                Rectangle {
                    id: _frontCard

                    anchors.fill: parent
                    radius: 6
                    color: AppTheme.surfaceRaised
                    antialiasing: true

                    RoundedClip {
                        anchors.fill: parent
                        radius: _frontCard.radius

                        Image {
                            anchors.fill: parent
                            source: root.artSource
                            sourceSize.width: Math.ceil(width * Screen.devicePixelRatio / 64) * 64
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            visible: status === Image.Ready
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: _name.implicitHeight + 48 * root.unit

                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "transparent" }
                                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.78) }
                            }

                            Text {
                                id: _name

                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: Math.round(10 * root.unit)
                                anchors.rightMargin: Math.round(10 * root.unit)
                                anchors.bottomMargin: Math.round(12 * root.unit)
                                text: root.title
                                color: "white"
                                font.pixelSize: Math.max(9, Math.round(17 * root.unit))
                                font.weight: Font.Bold
                                font.capitalization: Font.AllUppercase
                                lineHeight: 1.02
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Row {
                        x: 8
                        y: 8
                        spacing: 4

                        Rectangle {
                            visible: root.totalCount > 0
                            width: _badge.implicitWidth + 14
                            height: _badge.implicitHeight + 4
                            radius: 5
                            color: root.completed ? AppTheme.textPrimary : AppTheme.warning

                            Text {
                                id: _badge

                                anchors.centerIn: parent
                                text: root.watchedCount + "/" + root.totalCount
                                color: root.completed ? AppTheme.background : "#1A1300"
                                font.pixelSize: AppTheme.fs11
                                font.weight: Font.Bold
                                font.features: { "tnum": 1 }
                            }
                        }

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.custom
                            width: _customTag.implicitWidth + 12
                            height: _customTag.implicitHeight + 4
                            radius: 5
                            color: Qt.rgba(0, 0, 0, 0.72)

                            Text {
                                id: _customTag

                                anchors.centerIn: parent
                                text: qsTr("custom")
                                color: "white"
                                font.pixelSize: AppTheme.fs10
                                font.weight: Font.Bold
                            }
                        }
                    }

                    AbstractButton {
                        id: _menuButton

                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 6
                        visible: root.menuEnabled
                        width: 28
                        height: 28
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        opacity: root.fanned || _menuButton.hovered ? 1 : 0.55
                        Accessible.name: qsTr("More for %1").arg(root.title)

                        Behavior on opacity {
                            NumberAnimation { duration: 120 }
                        }

                        background: Rectangle {
                            radius: width / 2
                            color: _menuButton.hovered ? Qt.rgba(0, 0, 0, 0.8)
                                                       : Qt.rgba(0, 0, 0, 0.55)
                        }

                        contentItem: Item {
                            ThemedIcon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: Icons.moreVertical
                                tintColor: "white"
                                showPlaceholder: false
                            }
                        }

                        onClicked: root.menuRequested()
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: root.down ? AppTheme.pressed : "transparent"
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -5
                    radius: 9
                    color: "transparent"
                    border.width: 2
                    border.color: AppTheme.focusTelevision
                    visible: root.visualFocus || (root.highlighted && AppTheme.remoteNavigation)
                }
            }
        }

        Text {
            id: _title

            anchors.left: parent.left
            anchors.right: parent.right
            y: root.stackTop + root.stackHeight + AppTheme.spacing10
            text: root.title
            color: AppTheme.textPrimary
            font.pixelSize: AppTheme.fs14
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Text {
            id: _meta

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: _title.bottom
            text: root.meta
            color: AppTheme.textSecondary
            font.pixelSize: AppTheme.fs12
            elide: Text.ElideRight
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: root.stackRight
            anchors.top: _meta.bottom
            anchors.topMargin: 7
            height: 3
            radius: 2
            color: AppTheme.outline
            clip: true

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                radius: 2
                color: AppTheme.textPrimary

                Behavior on width { NumberAnimation { duration: 350 } }
            }
        }
    }
}
