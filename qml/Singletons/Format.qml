pragma Singleton

import QtQuick

QtObject {
    id: root

    function clock(seconds) {
        if (!seconds || seconds < 0 || !isFinite(seconds))
            return "0:00"
        const total = Math.floor(seconds)
        const h = Math.floor(total / 3600)
        const m = Math.floor((total % 3600) / 60)
        const s = total % 60
        const mm = m < 10 && h > 0 ? "0" + m : String(m)
        const ss = s < 10 ? "0" + s : String(s)
        return h > 0 ? h + ":" + mm + ":" + ss : mm + ":" + ss
    }

    function runtime(minutes) {
        const total = Math.max(0, Math.round(minutes))
        if (total <= 0)
            return ""
        const h = Math.floor(total / 60)
        const m = total % 60
        if (h <= 0)
            return qsTr("%1m").arg(m)
        if (m <= 0)
            return qsTr("%1h").arg(h)
        return qsTr("%1h %2m").arg(h).arg(m)
    }

    function scanLine(width, height) {
        const w = Math.max(0, Math.round(width))
        const h = Math.max(0, Math.round(height))

        if (w >= 7680) return qsTr("4320p")
        if (w >= 3840) return qsTr("2160p")
        if (w >= 2560) return qsTr("1440p")
        if (w >= 1900) return qsTr("1080p")
        if (w >= 1260) return qsTr("720p")
        if (w >= 850)  return qsTr("576p")
        if (w >= 620)  return qsTr("480p")

        return h > 0 ? qsTr("%1p").arg(h) : ""
    }

    function quality(width, height, hdr, codec) {
        const bits = []
        const line = root.scanLine(width || 0, height || 0)
        if (line.length > 0)
            bits.push(line)
        const name = String(codec || "").split("/")[0].trim()
        if (name.length > 0)
            bits.push(name)
        if (hdr === true)
            bits.push(qsTr("HDR"))
        return bits.join(" ")
    }

    function nextUpLabel(season, episode, resuming) {
        return resuming ? qsTr("Resume S%1 E%2").arg(season).arg(episode)
                        : qsTr("Continue S%1 E%2").arg(season).arg(episode)
    }

    function episodeCode(season, episode) {
        return qsTr("S%1E%2").arg(String(season).padStart(2, "0"))
                             .arg(String(episode).padStart(2, "0"))
    }

    function rating(value) {
        return "★ " + Number(value || 0).toFixed(1)
    }

    function speed(multiplier) {
        return String(Math.round(multiplier * 100) / 100)
    }

    function initials(name) {
        const parts = String(name || "").trim().split(/\s+/)
        if (parts.length === 0 || parts[0].length === 0)
            return ""
        if (parts.length === 1)
            return parts[0].charAt(0).toUpperCase()
        return (parts[0].charAt(0) + parts[parts.length - 1].charAt(0)).toUpperCase()
    }

}
