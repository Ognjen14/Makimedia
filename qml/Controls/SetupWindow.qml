pragma ComponentBehavior: Bound

import QtQuick
import com.topicdev.makimedia 1.0
import "../Singletons"

FocusScope {
    id: root

    property bool shown: false

    function close() {}

    function timeText() {
        if (Setup.finished)
            return Setup.problem.length > 0 ? Setup.problem : qsTr("Your library is ready")
        const seconds = Setup.secondsLeft
        if (seconds >= 60)
            return qsTr("About %n min left", "", Math.round(seconds / 60))
        if (seconds >= 0)
            return qsTr("About %n s left", "", Math.max(1, seconds))
        const steps = Setup.steps
        if (steps.length > 0 && steps[0].state === "running")
            return qsTr("Looking for videos")
        return qsTr("Working")
    }

    readonly property bool tight: root.height < 640

    readonly property int cardPadding: tight ? AppTheme.spacing16 : AppTheme.spacing32
    readonly property int cardGap: tight ? AppTheme.spacing2 : AppTheme.spacing6
    readonly property int cardSpacer: tight ? AppTheme.spacing8 : AppTheme.spacing16
    readonly property int runningRowHeight: tight ? 58 : 72
    readonly property int restingRowHeight: tight ? 42 : 56

    visible: shown
    focus: shown

    onShownChanged: {
        if (shown) {
            PopupRegistry.register(root)
            root.forceActiveFocus()
        } else {
            PopupRegistry.unregister(root)
        }
    }

    Connections {
        target: Setup

        function onActiveChanged() {
            if (Setup.finished && root.shown)
                Qt.callLater(_ok.forceActiveFocus)
        }
    }

    Component.onDestruction: PopupRegistry.unregister(root)

    Keys.onPressed: (event) => { event.accepted = true }

    FontMetrics {
        id: _titleMetrics

        font.pixelSize: AppTheme.fs16
    }

    Rectangle {
        anchors.fill: parent
        color: AppTheme.overlay

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            onWheel: (wheel) => { wheel.accepted = true }
        }
    }

    Rectangle {
        id: _card

        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * AppTheme.spacing16, 460)
        height: _column.implicitHeight + 2 * root.cardPadding
        radius: AppTheme.radiusXLarge
        color: AppTheme.surface
        border.width: 1
        border.color: AppTheme.outline

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
        }

        Column {
            id: _column

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: root.cardPadding
            spacing: root.cardGap

            Text {
                width: parent.width
                text: qsTr("Setting up your library")
                color: AppTheme.textPrimary
                font.pixelSize: root.tight ? AppTheme.fs18 : AppTheme.fs22
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: {
                    if (Setup.round <= 1)
                        return qsTr("This only takes long the first time")
                    if (Setup.roundFiles <= 0)
                        return qsTr("More videos turned up while it was working. Looking for them (round %1)")
                               .arg(Setup.round)
                    return qsTr("More videos turned up while it was working. Adding %n more (round %1)",
                                "", Setup.roundFiles).arg(Setup.round)
                }
                color: Setup.round > 1 ? AppTheme.primary : AppTheme.textSecondary
                font.pixelSize: AppTheme.fs14
                wrapMode: Text.Wrap
            }

            Item {
                width: parent.width
                height: root.cardSpacer
            }

            Repeater {
                model: 6

                delegate: Rectangle {
                    id: _row

                    required property int index

                    readonly property var step: Setup.steps.length > index
                                                ? Setup.steps[index]
                                                : ({ title: "", state: "waiting", done: 0, total: -1 })
                    readonly property bool running: step.state === "running"
                    readonly property bool finished: step.state === "done"
                    readonly property bool skipped: step.state === "skipped"
                    readonly property bool failed: step.state === "failed"
                    readonly property bool waiting: step.state === "waiting"

                    width: _column.width
                    height: running ? root.runningRowHeight : root.restingRowHeight
                    radius: AppTheme.radiusMedium
                    color: running ? AppTheme.surfaceVariant : "transparent"

                    Item {
                        id: _mark

                        anchors.left: parent.left
                        anchors.leftMargin: AppTheme.spacing10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 26
                        height: 26

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            visible: _row.finished
                            color: AppTheme.textPrimary

                            ThemedIcon {
                                anchors.centerIn: parent
                                width: 16
                                height: 16
                                source: Icons.check
                                tintColor: AppTheme.surface
                            }
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            visible: _row.waiting || _row.skipped
                            color: "transparent"
                            border.width: 2
                            border.color: AppTheme.outlineStrong
                        }

                        ThemedIcon {
                            anchors.fill: parent
                            visible: _row.failed
                            source: Icons.alertCircle
                            tintColor: AppTheme.error
                        }

                        ThemedIcon {
                            id: _spinner

                            anchors.fill: parent
                            visible: _row.running
                            source: Icons.spinner
                            tintColor: AppTheme.textPrimary

                            RotationAnimator on rotation {
                                running: _spinner.visible && root.visible
                                from: 0
                                to: 360
                                duration: 900
                                loops: Animation.Infinite
                            }
                        }
                    }

                    Text {
                        id: _count

                        anchors.right: parent.right
                        anchors.rightMargin: AppTheme.spacing10
                        anchors.verticalCenter: _title.verticalCenter
                        text: {
                            if (_row.skipped)
                                return qsTr("Skipped")
                            if (_row.failed)
                                return qsTr("Failed")
                            if (_row.waiting)
                                return ""
                            if (_row.step.total < 0)
                                return qsTr("%n found", "", _row.step.done)
                            if (_row.step.total === 0)
                                return ""
                            return _row.step.done + " / " + _row.step.total
                        }
                        color: _row.failed ? AppTheme.error : AppTheme.textSecondary
                        font.pixelSize: AppTheme.fs14
                    }

                    Text {
                        id: _title

                        anchors.left: _mark.right
                        anchors.leftMargin: AppTheme.spacing18
                        anchors.right: _count.left
                        anchors.rightMargin: AppTheme.spacing10
                        anchors.top: parent.top
                        anchors.topMargin: _row.running ? AppTheme.spacing14 : 0
                        height: _row.running ? Math.ceil(_titleMetrics.height) : parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: _row.step.title
                        color: _row.waiting || _row.skipped ? AppTheme.textDisabled
                                                            : AppTheme.textPrimary
                        font.pixelSize: AppTheme.fs16
                        elide: Text.ElideRight
                    }

                    LinearProgress {
                        anchors.left: _title.left
                        anchors.right: parent.right
                        anchors.rightMargin: AppTheme.spacing10
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: AppTheme.spacing14
                        visible: _row.running
                        indeterminate: _row.step.total <= 0
                        value: _row.step.total > 0 ? _row.step.done / _row.step.total : 0
                        fillColor: AppTheme.textPrimary
                    }
                }
            }

            Item {
                width: parent.width
                height: root.cardSpacer
            }

            Item {
                width: parent.width
                height: Math.max(_status.implicitHeight, _ok.visible ? _ok.implicitHeight : 0)

                Text {
                    id: _status

                    anchors.left: parent.left
                    anchors.right: _ok.visible ? _ok.left : parent.right
                    anchors.rightMargin: AppTheme.spacing16
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.timeText()
                    color: Setup.finished && Setup.problem.length > 0 ? AppTheme.error
                                                                       : AppTheme.textSecondary
                    font.pixelSize: AppTheme.fs14
                    wrapMode: Text.Wrap
                }

                AppButton {
                    id: _ok

                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: Setup.finished
                    text: qsTr("OK")
                    onClicked: Setup.acknowledge()
                }
            }
        }
    }
}
