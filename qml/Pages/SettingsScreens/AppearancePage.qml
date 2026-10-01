pragma ComponentBehavior: Bound

import QtQuick
import "../../Singletons" as S
import "../../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Column {
    spacing: S.AppTheme.spacing8

    Ctrl.SectionLabel {
        text: qsTr("Accent")
        visible: !System.isTelevision
        height: visible ? implicitHeight : 0
    }

    Ctrl.AccentPicker {
        width: parent.width
        visible: !System.isTelevision
        height: visible ? implicitHeight : 0
    }

    Ctrl.SectionLabel {
        text: qsTr("Text size")
        visible: !System.isTelevision
        height: visible ? implicitHeight : 0
    }

    Ctrl.FontSizePicker {
        width: parent.width
        visible: !System.isTelevision
        height: visible ? implicitHeight : 0
    }
}
