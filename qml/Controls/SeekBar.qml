pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons"

Item {
    id: root

    property real position: 0
    property real duration: 0
    property color trackColor: Qt.rgba(1, 1, 1, 0.24)
    property color bufferColor: Qt.rgba(1, 1, 1, 0.45)
    property color fillColor: AppTheme.primary
    property real trackHeight: AppTheme.seekBarHeight
    property real handleSize: AppTheme.seekBarHandleSize

    readonly property bool seeking: _area.pressed || keyScrubbing
    readonly property real displayPosition: seeking ? _scrubPosition : position

    property real _scrubPosition: 0

    property real keyStep: 10
    property bool keyScrubbing: false
    property int _keyRepeats: 0

    function nudge(seconds) {
        if (duration <= 0)
            return
        if (!keyScrubbing) {
            _scrubPosition = position
            keyScrubbing = true
        }
        _scrubPosition = Math.max(0, Math.min(duration, _scrubPosition + seconds))
        root.scrubbing(_scrubPosition)
        _keyCommit.restart()
    }

    function commitKeyScrub() {
        if (!keyScrubbing)
            return
        _keyCommit.stop()
        keyScrubbing = false
        root.seekRequested(_scrubPosition)
    }

    Timer {
        id: _keyCommit

        interval: 700
        onTriggered: root.commitKeyScrub()
    }

    onActiveFocusChanged: {
        if (!activeFocus)
            root.commitKeyScrub()
    }

    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Left:
        case Qt.Key_Right: {
            root._keyRepeats = event.isAutoRepeat ? root._keyRepeats + 1 : 0
            const step = root.keyStep * Math.min(8, 1 + Math.floor(root._keyRepeats / 5))
            root.nudge(event.key === Qt.Key_Left ? -step : step)
            event.accepted = true
            break
        }
        case Qt.Key_Select:
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (root.keyScrubbing) {
                root.commitKeyScrub()
                event.accepted = true
            }
            break
        }
    }

    signal seekRequested(real seconds)
    signal scrubbing(real seconds)

    property bool previewEnabled: false
    property url previewSource
    signal previewRequested(real seconds)

    implicitHeight: Math.max(handleSize, AppTheme.touchTargetMinimum / 2)
    implicitWidth: 200

    function secondsAt(mouseX) {
        if (duration <= 0 || _track.width <= 0)
            return 0
        const clamped = Math.max(0, Math.min(_track.width, mouseX - _track.x))
        return (clamped / _track.width) * duration
    }

    readonly property real _fraction:
        duration > 0 ? Math.max(0, Math.min(1, displayPosition / duration)) : 0

    Rectangle {
        id: _track

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: root.handleSize / 2
        anchors.rightMargin: root.handleSize / 2
        height: root.trackHeight
        radius: AppTheme.radiusPill
        color: root.trackColor

        Rectangle {
            width: parent.width * root._fraction
            height: parent.height
            radius: AppTheme.radiusPill
            color: root.enabled ? root.fillColor : root.bufferColor
        }
    }

    Rectangle {
        id: _handle

        x: _track.x + _track.width * root._fraction - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: root.handleSize
        height: root.handleSize
        radius: AppTheme.radiusPill
        color: root.fillColor
        visible: root.enabled
        border.width: root.activeFocus ? AppTheme.focusThickness : 0
        border.color: AppTheme.focus
        scale: root.activeFocus ? 1.5
                                : (_area.pressed ? 1.25 : (_area.containsMouse ? 1.1 : 1.0))

        Behavior on scale {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: _preview

        readonly property real seconds:
            (_area.pressed || root.keyScrubbing) ? root._scrubPosition
                                                 : root.secondsAt(_area.mouseX)

        readonly property bool showFrame:
            root.previewEnabled && root.previewSource.toString().length > 0

        visible: root.enabled
                 && (_area.containsMouse || _area.pressed || root.keyScrubbing)
        anchors.bottom: parent.top
        anchors.bottomMargin: AppTheme.spacing4
        x: Math.max(0, Math.min(root.width - width,
                                _track.x + _track.width
                                * (root.duration > 0
                                   ? Math.max(0, Math.min(1, seconds / root.duration))
                                   : 0) - width / 2))
        width: Math.max(_previewLabel.implicitWidth + 2 * AppTheme.spacing8,
                        showFrame ? 160 + 2 * AppTheme.spacing4 : 0)
        height: _previewColumn.implicitHeight + AppTheme.spacing6
        radius: AppTheme.radiusSmall
        color: AppTheme.surfaceRaised
        border.width: 1
        border.color: AppTheme.outline

        onSecondsChanged: {
            if (root.previewEnabled && visible)
                root.previewRequested(seconds)
        }

        onVisibleChanged: {
            if (root.previewEnabled && visible)
                root.previewRequested(seconds)
        }

        Column {
            id: _previewColumn

            anchors.centerIn: parent
            spacing: AppTheme.spacing4

            Rectangle {
                visible: _preview.showFrame
                width: 160
                height: 90
                radius: AppTheme.radiusSmall
                color: "#000000"
                clip: true

                Image {
                    anchors.fill: parent
                    source: root.previewSource
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: false
                }
            }

            Text {
                id: _previewLabel

                anchors.horizontalCenter: parent.horizontalCenter
                text: Format.clock(_preview.seconds)
                color: AppTheme.textPrimary
                font.pixelSize: AppTheme.fs12
                font.weight: Font.DemiBold
            }
        }
    }

    MouseArea {
        id: _area

        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        preventStealing: true
        acceptedButtons: Qt.LeftButton
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: (mouse) => {
            _keyCommit.stop()
            root.keyScrubbing = false
            root._scrubPosition = root.secondsAt(mouse.x)
            root.scrubbing(root._scrubPosition)
        }

        onPositionChanged: (mouse) => {
            if (!pressed)
                return
            root._scrubPosition = root.secondsAt(mouse.x)
            root.scrubbing(root._scrubPosition)
        }

        onReleased: (mouse) => {
            root._scrubPosition = root.secondsAt(mouse.x)
            root.seekRequested(root._scrubPosition)
        }

        onCanceled: {
            root._scrubPosition = root.position
        }
    }
}
