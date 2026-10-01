pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import "../Singletons"

RemoteButton {
    id: root

    enum Variant {
        Standard,
        Filled,
        Tonal,
        Outlined,
        Plain
    }

    enum Size {
        Large,
        Medium
    }

    property int variant: AppButton.Standard
    property int size: AppButton.Large
    property url iconSource
    property bool iconFilled: variant === AppButton.Filled
    property bool iconOnly: false
    property string accessibleName: text
    property string tooltipText: iconOnly ? text : ""

    readonly property bool hasIcon: iconSource.toString().length > 0
    readonly property bool hasText: text.length > 0 && !iconOnly
    readonly property int controlHeight: size === AppButton.Medium
                                         ? AppTheme.controlHeightMedium
                                         : AppTheme.controlHeightLarge
    readonly property int iconSize: size === AppButton.Medium ? 18 : 19
    readonly property int labelSize: size === AppButton.Medium ? AppTheme.fs14 : AppTheme.fs15

    readonly property color resolvedBackground: {
        if (!enabled)
            return (variant === AppButton.Plain || variant === AppButton.Outlined)
                   ? "transparent" : AppTheme.surfaceVariant
        switch (variant) {
        case AppButton.Filled:
            return AppTheme.primary
        case AppButton.Tonal:
            return AppTheme.primaryContainer
        case AppButton.Standard:
            return AppTheme.surfaceVariant
        case AppButton.Outlined:
        case AppButton.Plain:
        default:
            return "transparent"
        }
    }

    readonly property color resolvedForeground: {
        if (!enabled)
            return AppTheme.textDisabled
        switch (variant) {
        case AppButton.Filled:
            return AppTheme.onPrimaryStrong
        case AppButton.Tonal:
            return AppTheme.onPrimaryContainerStrong
        case AppButton.Standard:
        case AppButton.Outlined:
        case AppButton.Plain:
        default:
            return AppTheme.textPrimary
        }
    }

    readonly property color resolvedIconTint: resolvedForeground

    leftPadding: iconOnly ? 0 : (size === AppButton.Medium ? AppTheme.spacing20 : AppTheme.spacing24)
    rightPadding: leftPadding

    implicitHeight: controlHeight
    implicitWidth: iconOnly
                   ? controlHeight
                   : leftPadding
                     + (hasIcon ? iconSize : 0)
                     + (hasIcon && hasText ? AppTheme.spacing8 : 0)
                     + (hasText ? Math.ceil(_metrics.width) : 0)
                     + rightPadding

    TextMetrics {
        id: _metrics

        font.pixelSize: root.labelSize
        font.weight: Font.Medium
        text: root.text
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName.length > 0 ? accessibleName : qsTr("Button")

    contentItem: Item {
        Row {
            anchors.centerIn: parent
            spacing: root.hasIcon && root.hasText ? AppTheme.spacing8 : 0

            ThemedIcon {
                id: _icon

                anchors.verticalCenter: parent.verticalCenter
                width: root.iconSize
                height: root.iconSize
                visible: root.hasIcon
                source: root.iconSource
                tintColor: root.resolvedIconTint
                showPlaceholder: false
            }

            Text {
                id: _label

                readonly property real available:
                    Math.max(0, root.width - root.leftPadding - root.rightPadding
                             - (root.hasIcon ? root.iconSize + AppTheme.spacing8 : 0))

                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasText
                width: Math.min(Math.ceil(_metrics.width), available)
                text: root.text
                color: root.resolvedForeground
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: root.labelSize
                font.weight: Font.Medium
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: Math.max(9, Math.round(root.labelSize * 0.72))
                elide: Text.ElideRight
                maximumLineCount: 1
            }
        }
    }

    HoldToolTip {
        id: _tooltip

        text: root.tooltipText
    }

    onPressAndHold: _tooltip.hold()

    background: Rectangle {
        radius: AppTheme.radiusPill
        color: root.resolvedBackground
        border.width: root.variant === AppButton.Outlined ? 1 : 0
        border.color: AppTheme.outlineStrong

        FocusRing {
            active: AppTheme.remoteNavigation ? root.activeFocus
                                              : root.visualFocus
            ringRadius: parent.radius
        }

        StateLayer { control: root }
    }
}
