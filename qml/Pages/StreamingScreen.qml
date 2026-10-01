pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Item {
    id: root

    signal drawerRequested()

    readonly property bool acceptsFocus: true

    readonly property int tileSpacing: S.AppTheme.spacing16

    readonly property int tileColumns: {
        const room = width - 2 * S.AppTheme.spacing20
        if (room >= 3 * 300 + 2 * tileSpacing)
            return 3
        return room >= 2 * 300 + tileSpacing ? 2 : 1
    }

    readonly property real tileWidth: {
        const room = width - 2 * S.AppTheme.spacing20 - S.AppTheme.scrollBarWidth
        return Math.max(240, (room - (tileColumns - 1) * tileSpacing) / tileColumns)
    }

    function takeFocus() {
        if (Streaming.serving)
            _stop.forceActiveFocus(Qt.TabFocusReason)
        else
            _start.forceActiveFocus(Qt.TabFocusReason)
    }

    function formName(form) {
        if (form === "television")
            return qsTr("Television")
        if (form === "tablet")
            return qsTr("Tablet")
        if (form === "phone")
            return qsTr("Phone")
        return qsTr("Device")
    }

    function stateName(device) {
        if (!device.watching)
            return qsTr("Connected")
        return device.paused ? qsTr("Paused") : qsTr("Playing")
    }

    function goneText(device) {
        return qsTr("%1 gone").arg(S.Format.clock(device.position))
    }

    function leftText(device) {
        if (device.duration <= 0)
            return ""
        return qsTr("%1 left").arg(S.Format.clock(device.duration - device.position))
    }

    function stopAsked() {
        if (Streaming.watchingCount > 0) {
            _stopDialog.open()
            return
        }
        Streaming.stopServing()
    }

    Flickable {
        id: _scroll

        anchors.fill: parent
        anchors.leftMargin: S.AppTheme.spacing20
        anchors.rightMargin: S.AppTheme.spacing20
        contentHeight: _column.implicitHeight + S.AppTheme.spacing24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: Ctrl.AppScrollBar {}

        Column {
            id: _column

            width: parent.width - S.AppTheme.scrollBarWidth
            spacing: S.AppTheme.spacing16

            Ctrl.Card {
                width: parent.width
                visible: !Streaming.serving

                Item {
                    width: parent.width
                    implicitHeight: Math.max(_invitationText.implicitHeight,
                                             _start.implicitHeight)

                    Column {
                        id: _invitationText

                        anchors.left: parent.left
                        anchors.right: _start.left
                        anchors.rightMargin: S.AppTheme.spacing24
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: S.AppTheme.spacing8

                        Text {
                            width: parent.width
                            text: qsTr("Your library, on the Android TV (Google TV), Tablet or Phone")
                            color: S.AppTheme.textPrimary
                            font.pixelSize: S.AppTheme.fs22
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                        }

                        Text {
                            width: parent.width
                            text: qsTr("Start streaming and every phone, tablet and television running Makimedia on this network can play these films. This page then shows what each one is watching, and nothing plays on this PC until you stop.")
                            color: S.AppTheme.textSecondary
                            font.pixelSize: S.AppTheme.fs14
                            wrapMode: Text.Wrap
                        }

                        Text {
                            width: parent.width
                            text: qsTr("Several can watch at once: as many televisions, tablets and phones as your network can keep up with.")
                            color: S.AppTheme.textSecondary
                            font.pixelSize: S.AppTheme.fs14
                            wrapMode: Text.Wrap
                        }
                    }

                    Ctrl.AppButton {
                        id: _start

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Start streaming")
                        variant: Ctrl.AppButton.Filled
                        iconSource: S.Icons.play
                        Keys.onLeftPressed: root.drawerRequested()
                        onClicked: Streaming.startServing()
                    }
                }
            }

            Row {
                width: parent.width
                visible: Streaming.serving
                spacing: S.AppTheme.spacing12

                Rectangle {
                    id: _liveDot

                    anchors.verticalCenter: parent.verticalCenter
                    width: 9
                    height: 9
                    radius: 4.5
                    color: S.AppTheme.primary

                    SequentialAnimation {
                        running: _liveDot.visible
                        loops: Animation.Infinite

                        NumberAnimation {
                            target: _liveDot
                            property: "opacity"
                            from: 1.0
                            to: 0.2
                            duration: 900
                            easing.type: Easing.InOutQuad
                        }

                        NumberAnimation {
                            target: _liveDot
                            property: "opacity"
                            from: 0.2
                            to: 1.0
                            duration: 900
                            easing.type: Easing.InOutQuad
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("%1 is on").arg(Streaming.serverName)
                    color: S.AppTheme.textPrimary
                    font.pixelSize: S.AppTheme.fs14
                    font.weight: Font.Medium
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Streaming.devices.count === 1
                          ? qsTr("%1 films  ·  1 device").arg(Streaming.servedFileCount)
                          : qsTr("%1 films  ·  %2 devices").arg(Streaming.servedFileCount)
                                                           .arg(Streaming.devices.count)
                    color: S.AppTheme.textDisabled
                    font.pixelSize: S.AppTheme.fs13
                }
            }

            Text {
                width: parent.width
                visible: Streaming.lastError.length > 0
                text: Streaming.lastError
                color: S.AppTheme.error
                font.pixelSize: S.AppTheme.fs13
                wrapMode: Text.Wrap
            }

            Ctrl.SectionLabel {
                text: Streaming.devices.count > 0
                      ? qsTr("Watching now")
                      : qsTr("Nothing is watching yet")
            }

            Flow {
                width: parent.width
                visible: Streaming.devices.count === 0
                spacing: root.tileSpacing

                Repeater {
                    model: [
                        { form: "television",
                          line: qsTr("A television that connects shows up here, with what it is playing.") },
                        { form: "tablet",
                          line: qsTr("So does a tablet, whether it is watching or just looking around.") },
                        { form: "phone",
                          line: qsTr("And a phone, which keeps its own place in a film.") }
                    ]

                    delegate: Rectangle {
                        id: _ghost

                        required property var modelData

                        width: root.tileWidth
                        height: 260
                        radius: S.AppTheme.radiusLarge
                        color: "transparent"
                        border.width: 1
                        border.color: S.AppTheme.outline

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: S.AppTheme.spacing16
                            spacing: S.AppTheme.spacing10

                            Rectangle {
                                width: 40
                                height: 40
                                radius: S.AppTheme.radiusPill
                                color: S.AppTheme.surface

                                Text {
                                    anchors.centerIn: parent
                                    text: root.formName(_ghost.modelData.form).substring(0, 1)
                                    color: S.AppTheme.textDisabled
                                    font.pixelSize: S.AppTheme.fs16
                                    font.weight: Font.Medium
                                }
                            }

                            Text {
                                width: parent.width
                                text: _ghost.modelData.line
                                color: S.AppTheme.textDisabled
                                font.pixelSize: S.AppTheme.fs13
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }

            Flow {
                width: parent.width
                visible: Streaming.devices.count > 0
                spacing: root.tileSpacing

                Repeater {
                    model: Streaming.devices

                    delegate: Rectangle {
                        id: _tile

                        required property string name
                        required property string form
                        required property string title
                        required property string posterPath
                        required property string backdropPath
                        required property double position
                        required property double duration
                        required property bool paused
                        required property bool watching

                        readonly property var device: ({
                            "name": _tile.name,
                            "form": _tile.form,
                            "title": _tile.title,
                            "position": _tile.position,
                            "duration": _tile.duration,
                            "paused": _tile.paused,
                            "watching": _tile.watching
                        })

                        width: root.tileWidth
                        height: 300
                        radius: S.AppTheme.radiusLarge
                        color: _tile.watching ? S.AppTheme.surface : S.AppTheme.background
                        border.width: _tile.watching ? 0 : 1
                        border.color: S.AppTheme.outline
                        clip: true

                        Rectangle {
                            id: _hero

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 148
                            gradient: Gradient {
                                GradientStop {
                                    position: 0.0
                                    color: _tile.watching ? S.AppTheme.surfaceRaised
                                                          : S.AppTheme.surface
                                }
                                GradientStop {
                                    position: 1.0
                                    color: _tile.watching ? S.AppTheme.surface
                                                          : S.AppTheme.background
                                }
                            }

                            Image {
                                id: _art

                                anchors.fill: parent
                                source: _tile.backdropPath.length > 0
                                        ? Metadata.backdropUrl(_tile.backdropPath, 780)
                                        : (_tile.posterPath.length > 0
                                           ? Metadata.posterUrl(_tile.posterPath, 342)
                                           : "")
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                                visible: status === Image.Ready
                            }

                            Rectangle {
                                anchors.fill: parent
                                visible: _art.visible
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, 0.55) }
                                    GradientStop { position: 0.55; color: Qt.rgba(0, 0, 0, 0.2) }
                                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.45) }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.rightMargin: _stateBadge.width + 2 * S.AppTheme.spacing12
                                anchors.margins: S.AppTheme.spacing12
                                height: 22
                                radius: S.AppTheme.radiusPill
                                color: S.AppTheme.background

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: S.AppTheme.spacing10
                                    anchors.rightMargin: S.AppTheme.spacing10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: _tile.name + "  ·  " + root.formName(_tile.form)
                                    color: S.AppTheme.textPrimary
                                    font.pixelSize: S.AppTheme.fs11
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                id: _stateBadge

                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: S.AppTheme.spacing12
                                width: _stateText.implicitWidth + 2 * S.AppTheme.spacing10
                                height: 22
                                radius: S.AppTheme.radiusPill
                                color: _tile.watching && !_tile.paused
                                       ? S.AppTheme.primary
                                       : S.AppTheme.surfaceVariant

                                Text {
                                    id: _stateText

                                    anchors.centerIn: parent
                                    text: root.stateName(_tile.device)
                                    color: _tile.watching && !_tile.paused
                                           ? S.AppTheme.onPrimaryStrong
                                           : S.AppTheme.textSecondary
                                    font.pixelSize: S.AppTheme.fs11
                                    font.weight: Font.Medium
                                }
                            }
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: _hero.bottom
                            anchors.margins: S.AppTheme.spacing16
                            spacing: S.AppTheme.spacing6

                            Text {
                                width: parent.width
                                text: _tile.watching ? _tile.title
                                                     : qsTr("Looking through the library")
                                color: _tile.watching ? S.AppTheme.textPrimary
                                                      : S.AppTheme.textSecondary
                                font.pixelSize: S.AppTheme.fs16
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                            }

                            Text {
                                width: parent.width
                                visible: !_tile.watching
                                text: qsTr("It can start a film at any time")
                                color: S.AppTheme.textDisabled
                                font.pixelSize: S.AppTheme.fs13
                                wrapMode: Text.Wrap
                            }
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: S.AppTheme.spacing16
                            visible: _tile.watching
                            spacing: S.AppTheme.spacing8

                            Rectangle {
                                width: parent.width
                                height: 4
                                radius: 2
                                color: S.AppTheme.surfaceVariant

                                Rectangle {
                                    width: _tile.duration > 0
                                           ? parent.width * Math.max(0, Math.min(1, _tile.position / _tile.duration))
                                           : 0
                                    height: parent.height
                                    radius: parent.radius
                                    color: _tile.paused ? S.AppTheme.textDisabled
                                                        : S.AppTheme.primary
                                }
                            }

                            Item {
                                width: parent.width
                                implicitHeight: _gone.implicitHeight

                                Text {
                                    id: _gone

                                    anchors.left: parent.left
                                    text: root.goneText(_tile.device)
                                    color: S.AppTheme.textSecondary
                                    font.pixelSize: S.AppTheme.fs12
                                }

                                Text {
                                    anchors.right: parent.right
                                    text: root.leftText(_tile.device)
                                    color: S.AppTheme.textSecondary
                                    font.pixelSize: S.AppTheme.fs12
                                }
                            }
                        }
                    }
                }
            }

            Ctrl.Card {
                width: parent.width
                visible: Streaming.serving

                Item {
                    width: parent.width
                    implicitHeight: Math.max(_stopText.implicitHeight, _stop.implicitHeight)

                    Text {
                        id: _stopText

                        anchors.left: parent.left
                        anchors.right: _stop.left
                        anchors.rightMargin: S.AppTheme.spacing20
                        anchors.verticalCenter: parent.verticalCenter
                        text: Streaming.watchingCount > 0
                              ? qsTr("Stopping ends their films now. Each device keeps the place it reached and can carry on the next time you stream.")
                              : qsTr("Nothing plays on this PC and the library holds still until you stop. A device finds this PC on its own and offers to connect.")
                        color: S.AppTheme.textSecondary
                        font.pixelSize: S.AppTheme.fs13
                        wrapMode: Text.Wrap
                    }

                    Ctrl.AppButton {
                        id: _stop

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Stop streaming")
                        variant: Ctrl.AppButton.Outlined
                        iconSource: S.Icons.close
                        Keys.onLeftPressed: root.drawerRequested()
                        onClicked: root.stopAsked()
                    }
                }
            }

            Text {
                width: parent.width
                text: Streaming.serving && Streaming.addresses.length === 0
                      ? qsTr("This PC is not on a network right now, so no device can find it.")
                      : qsTr("A device does not find this PC? In Windows, set this network to Private and let Makimedia through the firewall.")
                color: Streaming.serving && Streaming.addresses.length === 0
                       ? S.AppTheme.error
                       : S.AppTheme.textDisabled
                font.pixelSize: S.AppTheme.fs12
                wrapMode: Text.Wrap
            }

            Ctrl.SettingsRow {
                width: parent.width
                visible: Streaming.serving && S.DevTools.showDeveloperSurfaces
                title: qsTr("Developer tools")
                subtitle: qsTr("A network that drops or crawls")
                trailing: Ctrl.SettingsRow.Chevron
                onClicked: _developerSheet.open()
            }
        }
    }

    Ctrl.BottomSheet {
        id: _developerSheet

        title: qsTr("Developer tools")

        Column {
            width: parent.width
            spacing: 0

            Ctrl.SettingsRow {
                width: parent.width
                title: Streaming.developerSilent ? qsTr("Silent for a minute")
                                                 : qsTr("Go silent for a minute")
                subtitle: qsTr("Every request goes unanswered, as if the PC had left the network. A device gives up after about 15 seconds.")
                enabled: !Streaming.developerSilent
                onClicked: Streaming.developerGoSilent()
            }

            Ctrl.SettingsRow {
                width: parent.width
                title: qsTr("Slow network")
                subtitle: qsTr("Sends files at 256 KB/s, about 2 Mbit/s")
                trailing: Ctrl.SettingsRow.Switch
                switchChecked: Streaming.developerSlowNetwork
                onSwitchToggled: (checked) => Streaming.developerSlowNetwork = checked
            }
        }
    }

    Ctrl.AppDialog {
        id: _stopDialog

        title: Streaming.watchingCount === 1
               ? qsTr("1 device is watching")
               : qsTr("%1 devices are watching").arg(Streaming.watchingCount)
        message: qsTr("Stopping ends their film now. Each keeps the place it reached.")
        acceptText: qsTr("Stop streaming")
        dismissText: qsTr("Keep streaming")
        onAccepted: Streaming.stopServing()
        onClosed: root.takeFocus()
    }
}
