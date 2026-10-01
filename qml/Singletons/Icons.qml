pragma Singleton

import QtQuick

QtObject {
    readonly property url menu: "qrc:/assets/icons/menu.svg"
    readonly property url search: "qrc:/assets/icons/search.svg"
    readonly property url home: "qrc:/assets/icons/home.svg"
    readonly property url continueWatching: "qrc:/assets/icons/continue-watching.svg"
    readonly property url all: "qrc:/assets/icons/all.svg"
    readonly property url suggested: "qrc:/assets/icons/suggested.svg"
    readonly property url unmatched: "qrc:/assets/icons/unmatched.svg"
    readonly property url identifyShows: "qrc:/assets/icons/identify-shows.svg"
    readonly property url folder: "qrc:/assets/icons/folder.svg"
    readonly property url movies: "qrc:/assets/icons/movies.svg"
    readonly property url tvShows: "qrc:/assets/icons/tv-shows.svg"
    readonly property url collections: "qrc:/assets/icons/collections.svg"
    readonly property url settings: "qrc:/assets/icons/settings.svg"
    readonly property url chevronLeft: "qrc:/assets/icons/chevron-left.svg"
    readonly property url chevronDown: "qrc:/assets/icons/chevron-down.svg"
    readonly property url gridSmall: "qrc:/assets/icons/grid-small.svg"
    readonly property url gridMedium: "qrc:/assets/icons/grid-medium.svg"
    readonly property url gridLarge: "qrc:/assets/icons/grid-large.svg"
    readonly property url moreVertical: "qrc:/assets/icons/more-vertical.svg"
    readonly property url chevronRight: "qrc:/assets/icons/chevron-right.svg"
    readonly property url play: "qrc:/assets/icons/play.svg"
    readonly property url check: "qrc:/assets/icons/check.svg"
    readonly property url spinner: "qrc:/assets/icons/spinner.svg"
    readonly property url lock: "qrc:/assets/icons/lock.svg"
    readonly property url miniPlayer: "qrc:/assets/icons/mini-player.svg"
    readonly property url skipBack: "qrc:/assets/icons/skip-back.svg"
    readonly property url pause: "qrc:/assets/icons/pause.svg"
    readonly property url skipForward: "qrc:/assets/icons/skip-forward.svg"
    readonly property url nextEpisode: "qrc:/assets/icons/next-episode.svg"

    readonly property url tmdbLogo: "qrc:/assets/tmdb_logo_blue.svg"
    readonly property url appIcon: "qrc:/assets/app-icon.png"
    readonly property url makimediaMark: "qrc:/assets/icons/makimedia-mark.svg"
    readonly property url noVideo: "qrc:/assets/icons/no-video.svg"
    readonly property url recentlyAdded: "qrc:/assets/icons/recently-added.svg"
    readonly property url rescan: "qrc:/assets/icons/rescan.svg"
    readonly property url alertCircle: "qrc:/assets/icons/alert-circle.svg"
    readonly property url close: "qrc:/assets/icons/close.svg"
    readonly property url subtitles: "qrc:/assets/icons/subtitles.svg"
    readonly property url tray: "qrc:/assets/icons/tray.svg"
    readonly property url appearance: "qrc:/assets/icons/appearance.svg"
    readonly property url plus: "qrc:/assets/icons/plus.svg"
    readonly property url volume: "qrc:/assets/icons/volume.svg"

    function byName(name) {
        const key = name.replace(/-([a-z])/g, function (m, c) { return c.toUpperCase() })
        return Icons[key] !== undefined ? Icons[key] : ""
    }
}
