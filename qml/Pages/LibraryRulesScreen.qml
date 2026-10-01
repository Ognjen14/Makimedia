pragma ComponentBehavior: Bound

import QtQuick
import "../Singletons" as S
import "../Controls" as Ctrl
import com.topicdev.makimedia 1.0

Column {
    id: root

    spacing: 0

    component Para: Text {
        width: parent ? parent.width : 0
        rightPadding: S.AppTheme.spacing16
        bottomPadding: S.AppTheme.spacing12
        color: S.AppTheme.textSecondary
        font.pixelSize: S.AppTheme.fs13
        lineHeight: 1.25
        wrapMode: Text.Wrap
    }

    component Example: Text {
        width: parent ? parent.width : 0
        leftPadding: S.AppTheme.spacing12
        rightPadding: S.AppTheme.spacing16
        bottomPadding: S.AppTheme.spacing14
        color: S.AppTheme.textPrimary
        font.pixelSize: S.AppTheme.fs12
        font.family: S.AppTheme.monoFontFamily
        lineHeight: 1.35
        wrapMode: Text.Wrap
    }

    component Formats: Flow {
        id: _row

        property alias items: _formats.model
        property string lead: "."

        width: parent ? parent.width : 0
        bottomPadding: S.AppTheme.spacing14
        spacing: S.AppTheme.spacing8

        Repeater {
            id: _formats

            delegate: Ctrl.PillChip {
                required property string modelData

                hoverEnabled: false
                text: _row.lead + modelData
            }
        }
    }

    Ctrl.SectionLabel {
        text: qsTr("Folders")
        topPadding: 0
    }

    Para {
        text: qsTr("Makimedia reads the name of a file and the names of the folders above it. Nothing else is opened to find out what something is, so the shape of your folders is most of the answer.")
    }

    Para {
        text: qsTr("A film is happiest in a folder of its own, named the way the file is. A folder holding one film also lets the subtitle rules below be far more generous.")
    }

    Example {
        text: "Films/\n" +
              "  Heat (1995)/\n" +
              "    Heat (1995).mkv"
    }

    Para {
        text: qsTr("A show wants a folder per show and a folder per season. The season folder is read as well as the file, so an episode whose name has lost its season number is still placed correctly.")
    }

    Example {
        text: "Shows/\n" +
              "  The Wire (2002)/\n" +
              "    Season 01/\n" +
              "      The.Wire.S01E01.mkv"
    }

    Para {
        text: qsTr("A season folder may be called Season 1, Season 01, Series 2 or S03. Anything else is read as part of the show's name.")
    }

    Ctrl.SectionLabel { text: qsTr("Names") }

    Para {
        text: qsTr("The year belongs in the name, in brackets or not. It is the single most useful thing you can add: it separates the remake from the original, and a film with a common name from a show with the same one.")
    }

    Para {
        text: qsTr("An episode is recognised from S01E02, 1x02, Season 1 Episode 2, or S01-02. On its own, E02 or Episode 2 is read as an episode of whatever season the folder says. S00E01 is a special.")
    }

    Para {
        text: qsTr("Everything after the year or the episode number is thrown away - resolution, source, codec, release group, the language tags. You do not need to clean a name up. What matters is that the title, the year and the numbers are in it.")
    }

    Example {
        text: "The.Wire.S01E01.1080p.BluRay.x265-GROUP.mkv\n" +
              "  -> The Wire, season 1, episode 1"
    }

    Ctrl.SectionLabel { text: qsTr("When it gets one wrong") }

    Para {
        text: qsTr("Open the film or the show and use Fix match. Search for the right title, choose it, and the answer is remembered - a later scan will not undo it, and for a show it settles every episode underneath at once.")
    }

    Para {
        text: qsTr("A file that is not a film or a show at all can be taken out of the library from its own page. Nothing on disk is touched, and Settings puts it back.")
    }

    Ctrl.SectionLabel { text: qsTr("Subtitles") }

    Para {
        text: qsTr("A subtitle file is found in three places. Which names count depends on how sure the folder makes it which film the subtitle belongs to.")
    }

    Para {
        text: qsTr("Beside the film, the subtitle's name must start with the film's own. Anything may follow it - a language, a label - as long as a full stop or a dash comes first.")
    }

    Example {
        text: "Heat (1995).mkv\n" +
              "Heat (1995).srt\n" +
              "Heat (1995).en.srt\n" +
              "Heat (1995).en.forced.srt"
    }

    Para {
        text: qsTr("In a subtitle folder beside a film, where that folder holds only the one film, the name does not matter at all. This is the place for subtitles that arrived named after nothing in particular.")
    }

    Formats {
        items: Library.subtitleFolderNames
        lead: ""
    }

    Example {
        text: "Heat (1995)/\n" +
              "  Heat (1995).mkv\n" +
              "  Subs/\n" +
              "    2_English.srt\n" +
              "    3_Croatian.srt"
    }

    Para {
        text: qsTr("And inside a subtitle folder, a folder named after the film takes any name too. That is what lets one subtitle folder serve several films.")
    }

    Para {
        text: qsTr("The rule tightens as the folder gets vaguer, on purpose: with twenty films in one folder there is no way to know which subtitle belongs to which, so there the name is all there is to go on.")
    }

    Para {
        text: qsTr("These endings are read as subtitles.")
    }

    Formats { items: Library.subtitleFormats }

    Ctrl.SectionLabel { text: qsTr("Video files") }

    Para {
        text: qsTr("Everything with one of these endings is indexed. Whether a given file plays is a separate question - that is the decoder's, not the library's.")
    }

    Formats { items: Library.videoFormats }

    Ctrl.SectionLabel { text: qsTr("What is asked of TMDB") }

    Para {
        text: qsTr("Each title is looked up once. The poster, the backdrop, the overview, the cast and the crew are kept on this device afterwards and never asked for again, so a library settles into asking for nothing at all.")
    }

    Para {
        text: qsTr("A device watching a PC's library asks TMDB nothing. Everything it shows, pictures included, comes from the PC.")
    }
}
