pragma ComponentBehavior: Bound

import QtQuick
import com.topicdev.makimedia 1.0

Item {
    id: root

    required property var player

    property int dragThreshold: 12
    property int doubleTapMs: 280
    property int longPressMs: 500
    property real scrubSpanSeconds: 120

    property real pressX: 0
    property real pressY: 0
    property real lastY: 0
    property bool moved: false
    property bool holding: false
    property string axis: ""
    property real scrubTarget: 0
    property double lastTapMs: 0
    property real pendingTapX: 0

    function zoneOf(x) {
        if (x < width / 3)
            return "left"
        if (x > 2 * width / 3)
            return "right"
        return "centre"
    }

    function beginAxis(dx, dy) {
        if (Math.abs(dx) > Math.abs(dy)) {
            axis = "scrub"
            scrubTarget = MpvPlayer.position
            return
        }
        axis = root.pressX > width / 2 ? "volume" : "brightness"
    }

    function applyScrub(dx) {
        if (!MpvPlayer.fileLoaded || MpvPlayer.duration <= 0)
            return
        const offset = (dx / Math.max(1, width)) * root.scrubSpanSeconds
        scrubTarget = Math.max(0, Math.min(MpvPlayer.duration,
                                           MpvPlayer.position + offset))
        root.player.showScrubPreview(scrubTarget)
    }

    function applyVolume(dy) {
        const step = (-dy / Math.max(1, height)) * 200
        root.player.setVolume(MpvPlayer.volume + step)
    }

    function applyBrightness(dy) {
        const step = -dy / Math.max(1, height)
        root.player.adjustBrightness(step)
    }

    function finishGesture() {
        if (axis === "scrub" && MpvPlayer.fileLoaded) {
            root.player.seekTo(scrubTarget, false)
            root.player.hideCentreOsd()
        }
        axis = ""
    }

    function handleTap(x) {
        const now = Date.now()
        if (now - lastTapMs < root.doubleTapMs) {
            lastTapMs = 0
            _tapTimer.stop()
            const zone = root.zoneOf(x)
            if (zone === "left")
                root.player.skip(-root.player.skipSeconds, false)
            else if (zone === "right")
                root.player.skip(root.player.skipSeconds, false)
            return
        }
        lastTapMs = now
        pendingTapX = x
        _tapTimer.restart()
    }

    Timer {
        id: _tapTimer

        interval: root.doubleTapMs
        onTriggered: root.player.toggleControls()
    }

    Timer {
        id: _holdTimer

        interval: root.longPressMs
        onTriggered: {
            if (root.moved)
                return
            root.holding = true
            root.player.beginHoldSpeed()
        }
    }

    MouseArea {
        id: _surface

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        preventStealing: false

        onPressed: (mouse) => {
            root.pressX = mouse.x
            root.pressY = mouse.y
            root.lastY = mouse.y
            root.moved = false
            root.holding = false
            root.axis = ""
            if (!root.player.locked)
                _holdTimer.restart()
        }

        onPositionChanged: (mouse) => {
            if (root.player.locked || root.holding)
                return

            const dx = mouse.x - root.pressX
            const dy = mouse.y - root.pressY

            if (!root.moved) {
                if (Math.abs(dx) < root.dragThreshold
                        && Math.abs(dy) < root.dragThreshold)
                    return
                root.moved = true
                _holdTimer.stop()
                root.beginAxis(dx, dy)
            }

            if (root.axis === "scrub")
                root.applyScrub(dx)
            else if (root.axis === "volume")
                root.applyVolume(mouse.y - root.lastY)
            else if (root.axis === "brightness")
                root.applyBrightness(mouse.y - root.lastY)

            root.lastY = mouse.y
        }

        onReleased: (mouse) => {
            _holdTimer.stop()

            if (root.holding) {
                root.holding = false
                root.player.endHoldSpeed()
                return
            }

            if (root.moved) {
                root.finishGesture()
                return
            }

            if (root.player.locked) {
                root.handleLockedTap()
                return
            }

            root.handleTap(mouse.x)
        }

        onCanceled: {
            _holdTimer.stop()
            if (root.holding) {
                root.holding = false
                root.player.endHoldSpeed()
            }
            root.axis = ""
            root.player.hideCentreOsd()
        }
    }

    property real pinchStartFactor: 1
    property real lastPanX: 0
    property real lastPanY: 0

    function abandonOneFingerGesture() {
        _holdTimer.stop()
        if (root.holding) {
            root.holding = false
            root.player.endHoldSpeed()
        }
        root.axis = ""
        root.moved = true
        root.player.hideCentreOsd()
    }

    PinchHandler {
        id: _pinch

        target: null
        enabled: !root.player.locked
        minimumPointCount: 2
        maximumPointCount: 2
        grabPermissions: PointerHandler.CanTakeOverFromAnything

        onActiveChanged: {
            if (!active) {
                root.player.endZoomGesture()
                return
            }
            root.abandonOneFingerGesture()
            root.pinchStartFactor = root.player.zoomFactor
            root.lastPanX = 0
            root.lastPanY = 0
        }

        onActiveScaleChanged: {
            if (!active)
                return
            root.player.setZoomFactor(root.pinchStartFactor * activeScale, true)
        }

        onActiveTranslationChanged: {
            if (!active)
                return
            const dx = (activeTranslation.x - root.lastPanX) / Math.max(1, root.width)
            const dy = (activeTranslation.y - root.lastPanY) / Math.max(1, root.height)
            root.lastPanX = activeTranslation.x
            root.lastPanY = activeTranslation.y
            root.player.panBy(dx, dy)
        }
    }

    property double lockedTapMs: 0

    function handleLockedTap() {
        const now = Date.now()
        if (now - lockedTapMs < root.doubleTapMs) {
            lockedTapMs = 0
            root.player.setLocked(false)
            return
        }
        lockedTapMs = now
    }
}
