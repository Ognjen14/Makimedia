pragma Singleton

import QtQuick
import com.topicdev.makimedia 1.0

QtObject {

    readonly property bool darkMode: true


    readonly property int televisionAccentIndex: 11

    property int accentIndex: System.isTelevision ? televisionAccentIndex
                                                  : AppSettings.accentIndex

    readonly property int safeAccentIndex:
        Math.max(0, Math.min(accentIndex, accentPresets.length - 1))

    readonly property var currentAccent:
        accentPresets[safeAccentIndex]
    readonly property var accentNames: [
        qsTr("Mono"), qsTr("Violet"), qsTr("Blue"), qsTr("Teal"), qsTr("Green"),
        qsTr("Amber"), qsTr("Rose"), qsTr("Crimson"), qsTr("Slate"), qsTr("Purple"),
        qsTr("Gold"), qsTr("Red")
    ]
    readonly property var accentPresets: [
        {
            light:          "#1B1B1B",
            lightContainer: "#E7E7E7",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#1B1B1B",

            dark:           "#ECECEC",
            darkContainer:  "#2A2A2A",
            darkOn:         "#1B1B1B",
            darkOnContainer: "#ECECEC"
        },

        {
            light:          "#6552D9",
            lightContainer: "#EEEBFF",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#221A4D",

            dark:           "#6F5CEB",
            darkContainer:  "#1F174F",
            darkOn:         "#171236",
            darkOnContainer: "#D9D7EA"
        },

        {
            light:          "#1565C0",
            lightContainer: "#E3F0FB",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#0D47A1",

            dark:           "#5C9FEB",
            darkContainer:  "#17314F",
            darkOn:         "#122336",
            darkOnContainer: "#D6E0EB"
        },

        {
            light:          "#007867",
            lightContainer: "#E0F5F2",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#004D40",

            dark:           "#5CEBD7",
            darkContainer:  "#174F47",
            darkOn:         "#123630",
            darkOnContainer: "#D6EBE8"
        },

        {
            light:          "#2E7D32",
            lightContainer: "#E8F5E9",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#1B5E20",

            dark:           "#61E569",
            darkContainer:  "#1B4B1E",
            darkOn:         "#123614",
            darkOnContainer: "#D9E7DA"
        },

        {
            light:          "#BF5B00",
            lightContainer: "#FFF3E0",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#743600",

            dark:           "#EBA05C",
            darkContainer:  "#4F3217",
            darkOn:         "#362312",
            darkOnContainer: "#EBE0D6"
        },

        {
            light:          "#C2185B",
            lightContainer: "#FCE4EC",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#880E4F",

            dark:           "#EB5C94",
            darkContainer:  "#4F172D",
            darkOn:         "#361220",
            darkOnContainer: "#EBD6DE"
        },

        {
            light:          "#C62828",
            lightContainer: "#FFEBEE",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#7F0000",

            dark:           "#EB5C5C",
            darkContainer:  "#4F1717",
            darkOn:         "#361212",
            darkOnContainer: "#EBD6D6"
        },

        {
            light:          "#455A64",
            lightContainer: "#ECEFF1",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#263238",

            dark:           "#74BBDC",
            darkContainer:  "#213945",
            darkOn:         "#122A36",
            darkOnContainer: "#DCE2E5"
        },

        {
            light:          "#7A3DB8",
            lightContainer: "#F2E7FA",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#46196B",

            dark:           "#A25CEB",
            darkContainer:  "#33194D",
            darkOn:         "#241236",
            darkOnContainer: "#E0D9E8"
        },

        {
            light:          "#8A6512",
            lightContainer: "#FBF0D8",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#4A3608",

            dark:           "#E2AE3A",
            darkContainer:  "#4F3F17",
            darkOn:         "#2A1F06",
            darkOnContainer: "#EFE6D4"
        },

        {
            light:          "#D32F2F",
            lightContainer: "#FDEAEA",
            lightOn:        "#FFFFFF",
            lightOnContainer: "#7A0000",

            dark:           "#F23636",
            darkContainer:  "#4F1414",
            darkOn:         "#2E0C0C",
            darkOnContainer: "#EBD8D8"
        }
    ]

    readonly property color background:
        darkMode ? "#090A0C" : "#F8F7F4"

    readonly property color surface:
        darkMode ? "#111317" : "#FFFFFF"

    readonly property color surfaceVariant:
        darkMode ? "#191C22" : "#F1EFE8"

    readonly property color surfaceRaised:
        darkMode ? "#20242B" : "#FFFFFF"


    readonly property color textPrimary:
        darkMode ? "#F3F4F6" : "#1A1A1A"

    readonly property color textSecondary:
        darkMode ? "#A5AAB3" : "#6B6966"

    readonly property color textDisabled:
        darkMode ? "#5E646E" : "#B4B2A9"


    readonly property color outline:
        darkMode ? "#282D35" : "#E8E6E0"

    readonly property color outlineStrong:
        darkMode ? "#3A404A" : "#D3D1C7"


    readonly property color primary:
        darkMode ? currentAccent.dark
                 : currentAccent.light

    readonly property color primaryContainer:
        darkMode ? currentAccent.darkContainer
                 : currentAccent.lightContainer

    readonly property color onPrimary:
        darkMode ? currentAccent.darkOn
                 : currentAccent.lightOn

    readonly property color onPrimaryContainer:
        darkMode ? currentAccent.darkOnContainer
                 : currentAccent.lightOnContainer

    readonly property color onPrimaryIcon:
        (darkMode && safeAccentIndex === 0) ? onPrimary : "#FFFFFF"

    function luminance(colour) {
        return 0.2126 * colour.r + 0.7152 * colour.g + 0.0722 * colour.b
    }

    function contrastOn(surface) {
        return luminance(surface) > 0.5 ? "#111317" : "#FFFFFF"
    }

    readonly property color onPrimaryStrong: contrastOn(primary)
    readonly property color onPrimaryContainerStrong: contrastOn(primaryContainer)

    readonly property color primaryUnderWhite:
        Qt.darker(primary, Math.max(1.0, luminance(primary) / 0.30))

    readonly property color success:
        darkMode ? "#63C99F" : "#16815F"

    readonly property color warning:
        darkMode ? "#EFB14D" : "#A86508"

    readonly property color error:
        darkMode ? "#F28B8B" : "#B3261E"

    readonly property color info:
        darkMode ? "#5BB5EE" : "#0277BD"


    readonly property color successContainer:
        darkMode ? "#12372B" : "#E1F5EE"

    readonly property color warningContainer:
        darkMode ? "#3A2709" : "#FFF0D1"

    readonly property color errorContainer:
        darkMode ? "#431818" : "#FCE8E7"

    readonly property color infoContainer:
        darkMode ? "#102F42" : "#E1F2FC"


    readonly property color overlay:
        darkMode ? Qt.rgba(0, 0, 0, 0.64)
                 : Qt.rgba(0, 0, 0, 0.32)

    readonly property color hover:
        darkMode ? Qt.rgba(1, 1, 1, 0.06)
                 : Qt.rgba(0, 0, 0, 0.04)

    readonly property color pressed:
        darkMode ? Qt.rgba(1, 1, 1, 0.10)
                 : Qt.rgba(0, 0, 0, 0.08)

    readonly property color focusTelevision: darkMode ? "white" : "black"

    readonly property color focus:
        remoteNavigation ? focusTelevision : primary

    readonly property int focusThickness: 2

    readonly property int rowContentPadding: 8

    readonly property bool remoteNavigation: System.isTelevision

    function isActivateKey(key) {
        return key === Qt.Key_Select
            || key === Qt.Key_Return
            || key === Qt.Key_Enter
            || key === Qt.Key_Space
    }

    readonly property real focusScale: System.isTelevision ? 1.07 : 1.0

    readonly property int focusHeadroom:
        focusScale > 1.0
        ? Math.ceil(posterTileWidth * posterAspectRatio * (focusScale - 1.0)) + 2
        : 0


    readonly property int spacing2:  2
    readonly property int spacing4:  4
    readonly property int spacing6:  6
    readonly property int spacing8:  8
    readonly property int spacing10: 10
    readonly property int spacing12: 12
    readonly property int spacing14: 14
    readonly property int spacing16: 16
    readonly property int spacing18: 18
    readonly property int spacing20: 20
    readonly property int spacing24: 24
    readonly property int spacing32: 32

    readonly property int radiusSmall:  8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge:  16
    readonly property int radiusXLarge: 20
    readonly property int radiusSheet:  24
    readonly property int radiusPill:   999

    readonly property int controlHeightSmall:  36
    readonly property int controlHeightMedium: 46
    readonly property int controlHeightLarge:  52

    readonly property int touchTargetMinimum: 48

    readonly property int breakpointCompact:  600
    readonly property int breakpointExpanded: 840

    readonly property real posterAspectRatio: 1.5
    readonly property real backdropAspectRatio: 9.0 / 16.0

    readonly property int posterTileWidth: System.usesWindowGeometry ? 190 : 112
    readonly property int wideTileWidth: System.usesWindowGeometry ? 300 : 184
    readonly property int gridTileTarget: {
        if (System.isTelevision)
            return 105
        return System.usesWindowGeometry ? 180 : 150
    }

    function gridColumns(width, size) {
        const medium = Math.max(3, Math.floor(width / gridTileTarget))
        if (size === 2)
            return Math.max(2, Math.min(medium - 1, Math.round(medium * 0.6)))
        if (size === 0)
            return Math.max(medium + 1, Math.min(12, Math.round(medium * 1.6)))
        return medium
    }

    readonly property int tileLabelHeight: 52

    readonly property int seekBarHeight:       4

    readonly property int scrollBarWidth:      12
    readonly property int seekBarHandleSize:   14
    readonly property int playerControlsHideMs: 3000


    readonly property string fontFamily: "Roboto"
    readonly property string monoFontFamily: "Roboto Mono"

    property real fontScale: 1.0

    readonly property int fs9:  Math.round(9  * fontScale)
    readonly property int fs10: Math.round(10 * fontScale)
    readonly property int fs11: Math.round(11 * fontScale)
    readonly property int fs12: Math.round(12 * fontScale)
    readonly property int fs13: Math.round(13 * fontScale)
    readonly property int fs14: Math.round(14 * fontScale)
    readonly property int fs15: Math.round(15 * fontScale)
    readonly property int fs16: Math.round(16 * fontScale)
    readonly property int fs17: Math.round(17 * fontScale)
    readonly property int fs18: Math.round(18 * fontScale)
    readonly property int fs20: Math.round(20 * fontScale)
    readonly property int fs22: Math.round(22 * fontScale)
    readonly property int fs28: Math.round(28 * fontScale)
    readonly property int fs38: Math.round(38 * fontScale)
    readonly property int fs48: Math.round(48 * fontScale)

}
