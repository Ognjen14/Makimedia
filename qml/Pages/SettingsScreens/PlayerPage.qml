pragma ComponentBehavior: Bound

import QtQuick
import "../../Singletons" as S
import "../../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Column {
    spacing: 0

    Ctrl.SectionLabel { text: qsTr("Skip interval") }

    Ctrl.ChipGroup {
        width: parent.width
        options: [1, 3, 5, 10, 15, 30]
        textFor: (seconds) => qsTr("%1s").arg(seconds)
        value: AppSettings.skipIntervalSeconds
        onChosen: (value) => AppSettings.skipIntervalSeconds = value
    }

    Ctrl.SectionLabel { text: qsTr("Hold to speed") }

    Ctrl.ChipGroup {
        width: parent.width
        options: [1.25, 1.5, 2.0, 3.0]
        textFor: (multiplier) => S.Format.speed(multiplier) + "×"
        value: AppSettings.holdToSpeedMultiplier
        onChosen: (value) => AppSettings.holdToSpeedMultiplier = value
    }

    Ctrl.SectionLabel { text: qsTr("Behaviour") }

    Ctrl.SettingsRow {
        width: parent.width
        trailing: Ctrl.SettingsRow.Switch
        title: qsTr("Play the next episode automatically")
        subtitle: qsTr("Counts down for the last 30 seconds, then plays on. Off, the button appears with 10 seconds left and waits for you")
        switchChecked: AppSettings.autoPlayNextEpisode
        onSwitchToggled: (checked) => AppSettings.autoPlayNextEpisode = checked
    }

    Ctrl.Divider { width: parent.width }

    Ctrl.SettingsRow {
        width: parent.width
        trailing: Ctrl.SettingsRow.Switch
        title: qsTr("Keep screen on")
        subtitle: qsTr("While a video is playing")
        switchChecked: AppSettings.keepScreenOn
        onSwitchToggled: (checked) => AppSettings.keepScreenOn = checked
    }

    Ctrl.Divider { width: parent.width }

    Ctrl.SettingsRow {
        width: parent.width
        visible: System.canForceHardwareDecoding
        trailing: Ctrl.SettingsRow.Switch
        title: qsTr("Force hardware decoding")
        subtitle: qsTr("Tries decoders normally skipped as risky. Turn it off if you see artefacts or a file will not play")
        switchChecked: AppSettings.forceHardwareDecoding
        onSwitchToggled: (checked) => AppSettings.forceHardwareDecoding = checked
    }

    Ctrl.Divider {
        width: parent.width
        visible: System.canForceHardwareDecoding
    }

    Ctrl.SettingsRow {
        width: parent.width
        trailing: Ctrl.SettingsRow.Value
        title: qsTr("Hardware decoding")
        subtitle: qsTr("Decoder chosen automatically")
        valueText: MpvPlayer.hwdecActive.length > 0
                   ? MpvPlayer.hwdecActive
                   : qsTr("idle")
        enabled: false
    }
}
