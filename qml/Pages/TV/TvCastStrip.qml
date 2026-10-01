pragma ComponentBehavior: Bound

import QtQuick
import "../../Controls" as Ctrl
import "../../Singletons" as S

Column {
    id: root

    property var cast: []
    property string directors: ""
    property string writers: ""
    property int faceSize: 64
    property int crewNameWidth: 150

    readonly property int kept: Math.min(3, cast ? cast.length : 0)
    readonly property bool hasCrew: directors.length > 0 || writers.length > 0

    visible: kept > 0 || hasCrew
    spacing: S.AppTheme.spacing4

    component CrewFact: Column {
        id: _fact

        property string label
        property string value
        property int maxWidth: 150

        visible: value.length > 0
        spacing: S.AppTheme.spacing2

        Ctrl.SectionLabel {
            text: _fact.label
            color: S.AppTheme.textSecondary
            topPadding: 0
            bottomPadding: 0
        }

        Text {
            width: Math.min(implicitWidth, _fact.maxWidth)
            text: _fact.value
            color: S.AppTheme.textPrimary
            font.pixelSize: S.AppTheme.fs14
            elide: Text.ElideRight
        }
    }

    Ctrl.SectionLabel {
        text: qsTr("Cast & crew")
        color: S.AppTheme.textSecondary
        topPadding: 0
    }

    Row {
        spacing: S.AppTheme.spacing16
        visible: root.kept > 0

        Repeater {
            model: root.kept

            delegate: Ctrl.CastFace {
                required property int index

                width: Math.round(root.faceSize * 1.5)
                person: root.cast[index]
                faceSize: root.faceSize
                nameSize: S.AppTheme.fs13
                roleSize: S.AppTheme.fs12
            }
        }
    }

    Row {
        topPadding: S.AppTheme.spacing8
        spacing: S.AppTheme.spacing24
        visible: root.hasCrew

        CrewFact {
            label: qsTr("Director")
            value: root.directors
            maxWidth: root.crewNameWidth
        }

        CrewFact {
            label: qsTr("Writer")
            value: root.writers
            maxWidth: root.crewNameWidth
        }
    }
}
