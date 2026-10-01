pragma ComponentBehavior: Bound

import QtQuick
import "../../Singletons" as S
import "../../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Column {
    spacing: 0

    Ctrl.SectionLabel {
        text: qsTr("Playback")
        topPadding: 0
    }

    Ctrl.KeyHints {
        width: parent.width
        hints: [
            { keys: "Space", description: qsTr("Play or pause") },
            { keys: "← →", description: qsTr("Seek %1 seconds")
                           .arg(AppSettings.skipIntervalSeconds) },
            { keys: "J L", description: qsTr("Seek %1 seconds")
                           .arg(AppSettings.skipIntervalSeconds) },
            { keys: "↑ ↓", description: qsTr("Volume up or down") },
            { keys: "M", description: qsTr("Mute") },
            { keys: "[ ]", description: qsTr("Playback speed") },
            { keys: qsTr("Hold click"),
              description: qsTr("Play at %1x until you let go")
                           .arg(S.Format.speed(AppSettings.holdToSpeedMultiplier)) }
        ]
    }

    Ctrl.SectionLabel { text: qsTr("View") }

    Ctrl.KeyHints {
        width: parent.width

        hints: {
            const rows = [
                { keys: "F", description: qsTr("Full screen") },
                { keys: "Esc", description: qsTr("Leave full screen, then leave the player") },
                { keys: "A", description: qsTr("Cycle aspect ratio") },
                { keys: "+  −", description: qsTr("Zoom in and out") },
                { keys: "0", description: qsTr("Reset zoom") },
                { keys: "Ctrl + wheel", description: qsTr("Zoom") },
                { keys: qsTr("Drag"), description: qsTr("Pan, while zoomed in") }
            ]
            if (System.supportsMiniPlayer) {
                rows.push({ keys: "P",
                            description: qsTr("Mini player - a borderless window that stays on top. Drag it to move, double-click to come back.") })
            }
            return rows
        }
    }

    Ctrl.SectionLabel { text: qsTr("Tracks") }

    Ctrl.KeyHints {
        width: parent.width
        hints: [
            { keys: "V", description: qsTr("Cycle subtitle track") },
            { keys: "#", description: qsTr("Cycle audio track") }
        ]
    }
}
