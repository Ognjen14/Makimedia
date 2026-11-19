pragma Singleton
import QtQuick
import com.topicdev.makimedia 1.0

QtObject {
    id: root

    readonly property bool touchTriggers: Qt.platform.os === "android" && !System.isTelevision
    readonly property bool focusTriggers: System.isTelevision
    readonly property bool available: Qt.platform.os === "windows" || touchTriggers
                                      || focusTriggers
    readonly property bool supported: available && AppSettings.hoverPreviews
                                      && !PreviewPlayer.playerStuck
    property bool blocked: false

    property int hoverDelay: 1000
    property int focusDelay: 4000
    property int closeGrace: 200

    property Item anchor: null
    property var mediaId: 0
    readonly property bool showing: anchor !== null

    property Item _pending: null
    property var _pendingId: 0
    property bool _anchorHovered: false
    property bool cardHovered: false
    property bool touchOpened: false

    function tileHovered(tile, id, hovered) {
        if (!supported || touchTriggers || focusTriggers || touchOpened || !tile || !id)
            return
        if (hovered) {
            if (tile === anchor) {
                _anchorHovered = true
                return
            }
            _pending = tile
            _pendingId = id
            _delay.restart()
            return
        }
        if (tile === _pending) {
            _delay.stop()
            _pending = null
        }
        if (tile === anchor) {
            _anchorHovered = false
            _grace.restart()
        }
    }

    function tileFocused(tile, id, focused) {
        if (!supported || !focusTriggers || !tile || !id)
            return
        if (focused) {
            if (tile === anchor)
                return
            if (anchor)
                close()
            _pending = tile
            _pendingId = id
            _delay.restart()
            return
        }
        if (tile === _pending) {
            _delay.stop()
            _pending = null
        }
        if (tile === anchor)
            close()
    }

    function tileGone(tile) {
        if (tile === _pending) {
            _delay.stop()
            _pending = null
        }
        if (tile === anchor)
            close()
    }

    function openNow(tile, id) {
        if (!supported || blocked || !tile || !id)
            return
        close()
        touchOpened = true
        _anchorHovered = true
        anchor = tile
        mediaId = id
    }

    function setCardHovered(hovered) {
        if (touchOpened)
            return
        cardHovered = hovered
        if (!hovered)
            _grace.restart()
    }

    function close() {
        _delay.stop()
        _grace.stop()
        _pending = null
        _anchorHovered = false
        cardHovered = false
        touchOpened = false
        anchor = null
        mediaId = 0
    }

    onBlockedChanged: if (blocked) close()
    onSupportedChanged: if (!supported) close()

    readonly property int tipLimit: 3
    onMediaIdChanged: {
        if (mediaId && AppSettings.previewTipCount < tipLimit)
            AppSettings.previewTipCount = tipLimit
    }

    property Timer _delay: Timer {
        interval: root.focusTriggers ? root.focusDelay : root.hoverDelay
        onTriggered: {
            if (!root._pending || root.blocked)
                return
            root.anchor = root._pending
            root.mediaId = root._pendingId
            root._anchorHovered = true
            root._pending = null
        }
    }

    property Timer _grace: Timer {
        interval: root.closeGrace
        onTriggered: {
            if (root.anchor && !root._anchorHovered && !root.cardHovered)
                root.close()
        }
    }
}
