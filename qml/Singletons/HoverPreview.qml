pragma Singleton
import QtQuick
import com.topicdev.makimedia 1.0

QtObject {
    id: root

    readonly property bool available: Qt.platform.os === "windows"
    readonly property bool supported: available && AppSettings.hoverPreviews
    property bool blocked: false

    property int hoverDelay: 1000
    property int closeGrace: 200

    property Item anchor: null
    property var mediaId: 0
    readonly property bool showing: anchor !== null

    property Item _pending: null
    property var _pendingId: 0
    property bool _anchorHovered: false
    property bool cardHovered: false

    function tileHovered(tile, id, hovered) {
        if (!supported || !tile || !id)
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

    function tileGone(tile) {
        if (tile === _pending) {
            _delay.stop()
            _pending = null
        }
        if (tile === anchor)
            close()
    }

    function setCardHovered(hovered) {
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
        anchor = null
        mediaId = 0
    }

    onBlockedChanged: if (blocked) close()
    onSupportedChanged: if (!supported) close()

    property Timer _delay: Timer {
        interval: root.hoverDelay
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
