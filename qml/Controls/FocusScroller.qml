import QtQuick
import QtQuick.Window
import "../Singletons"

QtObject {
    id: root

    property Item owner
    property Flickable flickable
    property Item content
    property real margin: AppTheme.spacing16

    function owns(item) {
        for (let candidate = item; candidate; candidate = candidate.parent) {
            if (candidate === root.owner)
                return true
        }
        return false
    }

    function reveal(item) {
        if (!item || !root.flickable || !root.content)
            return

        const view = root.flickable
        const top = item.mapToItem(root.content, 0, 0).y
        const bottom = top + item.height
        const maxY = Math.max(0, view.contentHeight - view.height)

        if (top - root.margin < view.contentY)
            view.contentY = Math.max(0, top - root.margin)
        else if (bottom + root.margin > view.contentY + view.height)
            view.contentY = Math.min(maxY, bottom + root.margin - view.height)
    }

    function scrollBy(direction) {
        const view = root.flickable
        if (!view)
            return
        const maxY = Math.max(0, view.contentHeight - view.height)
        const step = view.height * 0.6
        view.contentY = Math.max(0, Math.min(maxY, view.contentY + direction * step))
    }

    function step(forward) {
        const current = root.owner ? root.owner.Window.activeFocusItem : null
        if (!current)
            return

        const next = current.nextItemInFocusChain(forward)
        if (next && root.owns(next)) {
            next.forceActiveFocus(Qt.TabFocusReason)
            root.reveal(next)
            return
        }

        root.scrollBy(forward ? 1 : -1)
    }
}
