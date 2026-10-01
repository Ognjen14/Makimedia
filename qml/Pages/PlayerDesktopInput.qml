pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import com.topicdev.makimedia 1.0

Item {
    id: root

    required property var player

    property double menuClosedMs: 0

    property bool panning: false
    property real pressX: 0
    property real pressY: 0

    property int longPressMs: 500
    property int dragThreshold: 12
    property bool holding: false

    property bool suppressClick: false

    Timer {
        id: _holdTimer

        interval: root.longPressMs
        onTriggered: {
            if (root.panning)
                return
            root.holding = true
            root.player.beginHoldSpeed()
        }
    }

    Timer {
        id: _clickTimer

        interval: Qt.styleHints.mouseDoubleClickInterval
        onTriggered: {
            if (root.player.locked)
                return
            if (root.player.controlsVisible)
                root.player.hideControls()
            else
                root.player.revealControls()
        }
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true

        onPressed: (mouse) => {
            if (mouse.button === Qt.RightButton && !root.player.locked) {
                _contextMenu.popup()
                return
            }
            root.panning = false
            root.holding = false
            root.suppressClick = false
            root.pressX = mouse.x
            root.pressY = mouse.y

            if (!root.player.locked && mouse.button === Qt.LeftButton)
                _holdTimer.restart()
        }

        onReleased: {
            _holdTimer.stop()
            if (root.holding) {
                root.holding = false
                root.suppressClick = true
                root.player.endHoldSpeed()
            }
        }

        onCanceled: {
            _holdTimer.stop()
            if (root.holding) {
                root.holding = false
                root.player.endHoldSpeed()
            }
        }

        onPositionChanged: (mouse) => {
            root.player.revealControls()

            if (!root.holding
                    && (Math.abs(mouse.x - root.pressX) > root.dragThreshold
                        || Math.abs(mouse.y - root.pressY) > root.dragThreshold))
                _holdTimer.stop()

            if (root.player.locked || root.holding || !root.player.zoomed
                    || !(mouse.buttons & Qt.LeftButton))
                return

            root.panning = true
            const dx = (mouse.x - root.pressX) / Math.max(1, width)
            const dy = (mouse.y - root.pressY) / Math.max(1, height)
            root.pressX = mouse.x
            root.pressY = mouse.y
            root.player.panBy(dx, dy)
        }

        onWheel: (wheel) => {
            if (root.player.locked)
                return
            if (wheel.modifiers & Qt.ControlModifier) {
                root.player.nudgeZoom(wheel.angleDelta.y)
                return
            }
            root.player.nudgeVolume(wheel.angleDelta.y > 0 ? 5 : -5)
        }

        onClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton || root.player.locked)
                return
            if (_contextMenu.visible)
                return
            if (root.panning) {
                root.panning = false
                return
            }
            if (root.suppressClick) {
                root.suppressClick = false
                return
            }
            if (Date.now() - root.menuClosedMs < 250)
                return
            _clickTimer.restart()
        }

        onDoubleClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton)
                return
            _clickTimer.stop()
            if (root.player.locked)
                root.player.setLocked(false)
            else
                root.player.fullScreenToggleRequested()
        }
    }

    Shortcut {
        sequence: "Space"
        enabled: !root.player.locked
        onActivated: root.player.togglePause()
    }

    Shortcut {
        sequences: ["Left"]
        enabled: !root.player.locked
        onActivated: root.player.skip(-root.player.skipSeconds, false)
    }

    Shortcut {
        sequences: ["Right"]
        enabled: !root.player.locked
        onActivated: root.player.skip(root.player.skipSeconds, false)
    }

    Shortcut {
        sequence: "J"
        enabled: !root.player.locked
        onActivated: root.player.skip(-root.player.skipSeconds, false)
    }

    Shortcut {
        sequence: "L"
        enabled: !root.player.locked
        onActivated: root.player.skip(root.player.skipSeconds, false)
    }

    Shortcut {
        sequences: ["Up"]
        enabled: !root.player.locked
        onActivated: root.player.nudgeVolume(5)
    }

    Shortcut {
        sequences: ["Down"]
        enabled: !root.player.locked
        onActivated: root.player.nudgeVolume(-5)
    }

    Shortcut {
        sequence: "M"
        enabled: !root.player.locked
        onActivated: root.player.toggleMute()
    }

    Shortcut {
        sequence: "F"
        enabled: !root.player.locked
        onActivated: root.player.fullScreenToggleRequested()
    }

    Shortcut {
        sequence: "P"
        enabled: !root.player.locked && System.supportsMiniPlayer
        onActivated: root.player.miniPlayerToggleRequested()
    }

    Shortcut {
        sequence: "["
        enabled: !root.player.locked
        onActivated: root.player.cycleSpeed(-1)
    }

    Shortcut {
        sequence: "]"
        enabled: !root.player.locked
        onActivated: root.player.cycleSpeed(1)
    }

    Shortcut {
        sequence: "V"
        enabled: !root.player.locked
        onActivated: MpvPlayer.cycleSubtitleTrack()
    }

    Shortcut {
        sequence: "#"
        enabled: !root.player.locked
        onActivated: MpvPlayer.cycleAudioTrack()
    }

    Shortcut {
        sequence: "A"
        enabled: !root.player.locked
        onActivated: root.player.cycleAspect()
    }

    Shortcut {
        sequences: ["+", "="]
        enabled: !root.player.locked
        onActivated: root.player.nudgeZoom(1)
    }

    Shortcut {
        sequence: "-"
        enabled: !root.player.locked
        onActivated: root.player.nudgeZoom(-1)
    }

    Shortcut {
        sequence: "0"
        enabled: !root.player.locked
        onActivated: root.player.resetZoom()
    }

    Menu {
        id: _contextMenu

        onClosed: root.menuClosedMs = Date.now()

        MenuItem {
            text: MpvPlayer.paused ? qsTr("Play") : qsTr("Pause")
            onTriggered: root.player.togglePause()
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Audio track...")
            enabled: MpvPlayer.audioTracks.count > 0
            onTriggered: root.player.openAudioTracks()
        }

        MenuItem {
            text: qsTr("Subtitles...")
            enabled: MpvPlayer.subtitleTracks.count > 0
            onTriggered: root.player.openSubtitleTracks()
        }

        MenuItem {
            text: qsTr("Audio and subtitle delay...")
            enabled: MpvPlayer.fileLoaded
            onTriggered: root.player.openDelays()
        }

        Menu {
            title: qsTr("Speed")

            Repeater {
                model: root.player.speedSteps

                delegate: MenuItem {
                    required property int index
                    required property real modelData

                    text: modelData.toFixed(2) + "×"
                    checkable: true
                    checked: root.player.speedIndex === index
                    onTriggered: root.player.applySpeed(index, false)
                }
            }
        }

        Menu {
            title: qsTr("Aspect ratio")

            Repeater {
                model: root.player.aspectModes

                delegate: MenuItem {
                    required property int index
                    required property var modelData

                    text: modelData.label
                    checkable: true
                    checked: root.player.aspectIndex === index
                    onTriggered: root.player.applyAspect(index)
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            text: root.player.fullScreen ? qsTr("Exit full screen") : qsTr("Full screen")
            onTriggered: root.player.fullScreenToggleRequested()
        }

        MenuItem {
            text: qsTr("Lock controls")
            onTriggered: root.player.setLocked(true)
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Back to library")
            onTriggered: root.player.backRequested()
        }
    }
}
