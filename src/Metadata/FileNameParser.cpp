#include "Metadata/FileNameParser.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {

const QSet<QString> &junkTokens()
{
    static const QSet<QString> tokens = {
        QStringLiteral("480p"), QStringLiteral("576p"), QStringLiteral("720p"),
        QStringLiteral("1080p"), QStringLiteral("1080i"), QStringLiteral("2160p"),
        QStringLiteral("4320p"), QStringLiteral("4k"), QStringLiteral("8k"),
        QStringLiteral("uhd"), QStringLiteral("fhd"), QStringLiteral("hd"),

        QStringLiteral("bluray"), QStringLiteral("blu-ray"), QStringLiteral("brrip"),
        QStringLiteral("bdrip"), QStringLiteral("bdremux"), QStringLiteral("remux"),
        QStringLiteral("webrip"), QStringLiteral("web-dl"), QStringLiteral("webdl"),
        QStringLiteral("web"), QStringLiteral("hdtv"), QStringLiteral("dvdrip"),
        QStringLiteral("dvd"), QStringLiteral("hdrip"), QStringLiteral("cam"),
        QStringLiteral("ts"), QStringLiteral("telesync"),

        QStringLiteral("x264"), QStringLiteral("x265"), QStringLiteral("h264"),
        QStringLiteral("h265"), QStringLiteral("h"), QStringLiteral("avc"),
        QStringLiteral("hevc"), QStringLiteral("xvid"), QStringLiteral("divx"),
        QStringLiteral("av1"), QStringLiteral("vp9"), QStringLiteral("10bit"),
        QStringLiteral("8bit"), QStringLiteral("hi10p"),

        QStringLiteral("aac"), QStringLiteral("ac3"), QStringLiteral("eac3"),
        QStringLiteral("dts"), QStringLiteral("dts-hd"), QStringLiteral("ma"),
        QStringLiteral("truehd"), QStringLiteral("atmos"), QStringLiteral("flac"),
        QStringLiteral("opus"), QStringLiteral("mp3"), QStringLiteral("dd"),
        QStringLiteral("ddp"), QStringLiteral("dd5"), QStringLiteral("ddp5"),
        QStringLiteral("dd2"), QStringLiteral("aac2"),

        QStringLiteral("hdr"), QStringLiteral("hdr10"), QStringLiteral("hdr10+"),
        QStringLiteral("dv"), QStringLiteral("dovi"), QStringLiteral("sdr"),
        QStringLiteral("imax"), QStringLiteral("hybrid"),

        QStringLiteral("proper"), QStringLiteral("repack"), QStringLiteral("extended"),
        QStringLiteral("unrated"), QStringLiteral("uncut"), QStringLiteral("remastered"),
        QStringLiteral("limited"), QStringLiteral("internal"), QStringLiteral("retail"),
        QStringLiteral("complete"), QStringLiteral("multi"), QStringLiteral("dual"),
        QStringLiteral("subbed"), QStringLiteral("dubbed"), QStringLiteral("repost"),

        QStringLiteral("amzn"), QStringLiteral("nf"), QStringLiteral("atvp"),
        QStringLiteral("dsnp"), QStringLiteral("hmax"), QStringLiteral("max"),
        QStringLiteral("hulu"), QStringLiteral("pcok"), QStringLiteral("stan")
    };
    return tokens;
}

const QSet<QString> &softJunkTokens()
{
    static const QSet<QString> tokens = {
        QStringLiteral("max"), QStringLiteral("web"), QStringLiteral("dual"),
        QStringLiteral("multi"), QStringLiteral("retail"),
        QStringLiteral("complete"), QStringLiteral("extended"),
        QStringLiteral("limited"), QStringLiteral("internal"),
        QStringLiteral("proper"), QStringLiteral("hybrid"),
        QStringLiteral("hd"), QStringLiteral("dv"), QStringLiteral("ts"),
        QStringLiteral("cam"), QStringLiteral("dvd"), QStringLiteral("ma"),
        QStringLiteral("dd"), QStringLiteral("h"), QStringLiteral("stan"),
        QStringLiteral("hulu"), QStringLiteral("nf")
    };
    return tokens;
}

bool isSoftJunkToken(const QString &token)
{
    QString value = token.toLower();
    while (!value.isEmpty() && !value.at(value.size() - 1).isLetterOrNumber()) {
        value.chop(1);
    }
    return softJunkTokens().contains(value);
}

QString normalise(const QString &raw)
{
    QString value = raw;

    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    value.replace(QLatin1Char('.'), QLatin1Char(' '));
    value.replace(QLatin1Char('['), QLatin1Char(' '));
    value.replace(QLatin1Char(']'), QLatin1Char(' '));
    value.replace(QLatin1Char('('), QLatin1Char(' '));
    value.replace(QLatin1Char(')'), QLatin1Char(' '));
    value.replace(QLatin1Char('{'), QLatin1Char(' '));
    value.replace(QLatin1Char('}'), QLatin1Char(' '));

    return value.simplified();
}

const int kAncestorsRead = 3;

bool leavesNothingUsable(const QString &candidate)
{
    const QStringList words =
        candidate.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return true;
    }
    if (words.size() > 1) {
        return false;
    }

    static const QSet<QString> articles = {
        QStringLiteral("a"), QStringLiteral("an"), QStringLiteral("the"),
        QStringLiteral("of"), QStringLiteral("and")
    };
    return articles.contains(words.first().toLower());
}

QString stripEditions(const QString &value)
{
    static const QRegularExpression editionPhrase(
        QStringLiteral("\\s*\\b(?:\\d{1,2}(?:st|nd|rd|th)\\s+)?"
                       "(?:anniversary|director'?s?|collector'?s?|special"
                       "|ultimate|criterion|theatrical|deluxe|definitive)\\s+"
                       "(?:edition|cut|version)\\b"),
        QRegularExpression::CaseInsensitiveOption);

    QString out = value;
    out.remove(editionPhrase);
    return out.simplified();
}

bool isTrimmablePunctuation(QChar c)
{
    return c == QLatin1Char('-') || c == QLatin1Char(',')
        || c == QLatin1Char(':') || c == QLatin1Char('|')
        || c == QLatin1Char('/') || c == QLatin1Char('~')
        || c == QLatin1Char('_');
}

QString cleanTitle(const QString &raw)
{
    QString value = raw.simplified();

    while (!value.isEmpty() && isTrimmablePunctuation(value.at(value.size() - 1))) {
        value.chop(1);
        value = value.trimmed();
    }

    while (!value.isEmpty() && isTrimmablePunctuation(value.at(0))) {
        value.remove(0, 1);
        value = value.trimmed();
    }

    return value;
}

}

namespace FileNameParser {

int seasonFromFolder(const QString &folderName)
{
    const QString value = normalise(folderName);

    static const QRegularExpression wordy(
        QStringLiteral("\\bSeason\\s?(\\d{1,2})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch wordyMatch = wordy.match(value);
    if (wordyMatch.hasMatch()) {
        return wordyMatch.captured(1).toInt();
    }

    static const QRegularExpression terse(
        QStringLiteral("\\bS(\\d{1,2})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch terseMatch = terse.match(value);
    if (terseMatch.hasMatch()) {
        return terseMatch.captured(1).toInt();
    }

    return 0;
}

bool isSeasonOnlyFolder(const QString &folderName)
{
    const QString value = normalise(folderName).trimmed();

    static const QRegularExpression seasonOnly(
        QStringLiteral("^(?:Season|Series|S)\\s?\\d{1,2}$"),
        QRegularExpression::CaseInsensitiveOption);
    return seasonOnly.match(value).hasMatch();
}

bool isJunkToken(const QString &token)
{
    QString value = token.toLower();
    while (!value.isEmpty() && !value.at(value.size() - 1).isLetterOrNumber()
           && value.at(value.size() - 1) != QLatin1Char('+')) {
        value.chop(1);
    }

    if (value.isEmpty()) {
        return false;
    }

    if (junkTokens().contains(value)) {
        return true;
    }

    static const QRegularExpression channels(
        QStringLiteral("^(?:[257]|5\\.1|7\\.1|2\\.0)$"));
    if (channels.match(value).hasMatch()) {
        return false;
    }

    static const QRegularExpression sized(
        QStringLiteral("^(?:\\d{3,4}p|\\d+bit|[xh]\\.?26[45]"
                       "|dd[p]?\\d(?:\\.\\d)?|\\d{3,4}x\\d{3,4})$"));
    return sized.match(value).hasMatch();
}

ParsedFileName parse(const QString &fileNameOrPath)
{
    ParsedFileName result;

    const QString base = QFileInfo(fileNameOrPath).completeBaseName();
    if (base.isEmpty()) {
        return result;
    }

    const QString working = stripEditions(normalise(base));
    if (working.isEmpty()) {
        return result;
    }

    int cut = working.size();

    static const QRegularExpression seasonEpisode(
        QStringLiteral("\\bS(\\d{1,2})\\s?E(\\d{1,3})(?:\\s?-\\s?|\\s?)?"
                       "(?:E\\d{1,3})?\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression crossPair(
        QStringLiteral("\\b(\\d{1,2})x(\\d{1,3})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression wordyPair(
        QStringLiteral("\\bSeason\\s?(\\d{1,2})\\s+Episode\\s?(\\d{1,3})\\b"),
        QRegularExpression::CaseInsensitiveOption);

    int episodeMarkerEnd = -1;
    int episodeMarkerStart = -1;

    for (const QRegularExpression &pattern : {seasonEpisode, crossPair, wordyPair}) {
        const QRegularExpressionMatch match = pattern.match(working);
        if (!match.hasMatch()) {
            continue;
        }
        result.season = match.captured(1).toInt();
        result.episode = match.captured(2).toInt();
        result.specials = result.season == 0 && result.episode > 0;
        cut = qMin(cut, match.capturedStart());
        episodeMarkerStart = match.capturedStart();
        episodeMarkerEnd = match.capturedEnd();
        break;
    }

    if (result.episode <= 0) {
        static const QRegularExpression joinedPair(
            QStringLiteral("\\bS(\\d{1,2})[_\\-.](\\d{1,3})\\b"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = joinedPair.match(base);
        if (match.hasMatch()) {
            result.season = match.captured(1).toInt();
            result.episode = match.captured(2).toInt();
            result.specials = result.season == 0 && result.episode > 0;
            episodeMarkerStart =
                stripEditions(normalise(base.left(match.capturedStart()))).size();
            cut = qMin(cut, episodeMarkerStart);
            episodeMarkerEnd =
                stripEditions(normalise(base.left(match.capturedEnd()))).size();
        }
    }

    int episodeOnlyStart = -1;
    if (result.episode <= 0) {
        static const QRegularExpression episodeOnly(
            QStringLiteral("\\bE(?:P|PISODE)?\\s?(\\d{1,3})\\b"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = episodeOnly.match(working);
        if (match.hasMatch()) {
            result.episode = match.captured(1).toInt();
            result.episodeWithoutSeason = true;
            episodeOnlyStart = match.capturedStart();
            episodeMarkerEnd = match.capturedEnd();
        }
    }

    static const QRegularExpression seasonOnly(
        QStringLiteral("\\b(?:Season|Series)\\s?(\\d{1,2})\\b|\\bS(\\d{1,2})\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch seasonMatch = seasonOnly.match(working);
    if (seasonMatch.hasMatch()) {
        if (result.season <= 0 && !result.specials) {
            const QString wordy = seasonMatch.captured(1);
            result.season = wordy.isEmpty() ? seasonMatch.captured(2).toInt()
                                            : wordy.toInt();
        }
        if (seasonMatch.capturedStart() > 0) {
            cut = qMin(cut, seasonMatch.capturedStart());
        }
    }

    const bool episodeAwaitsASeason = episodeOnlyStart >= 0
        && result.season <= 0
        && !cleanTitle(working.left(episodeOnlyStart)).isEmpty();
    if (episodeOnlyStart >= 0 && !episodeAwaitsASeason) {
        cut = qMin(cut, episodeOnlyStart);
    }

    static const QRegularExpression yearPattern(
        QStringLiteral("\\b(19\\d{2}|20\\d{2})\\b"));

    int firstJunkStart = working.size();
    {
        const QStringList all = working.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        int walk = 0;
        for (int i = 0; i < all.size(); ++i) {
            const QString &token = all.at(i);
            const int start = working.indexOf(token, walk);
            walk = start + token.size();

            if (start == 0 || !isJunkToken(token)) {
                continue;
            }

            if (leavesNothingUsable(working.left(start))) {
                continue;
            }
            if (isSoftJunkToken(token)
                && !(i + 1 < all.size() && isJunkToken(all.at(i + 1)))) {
                continue;
            }

            firstJunkStart = start;
            break;
        }
    }

    int yearStart = -1;
    QRegularExpressionMatchIterator years = yearPattern.globalMatch(working);
    while (years.hasNext()) {
        const QRegularExpressionMatch match = years.next();

        if (match.capturedStart() == 0 || match.capturedStart() >= firstJunkStart) {
            continue;
        }

        result.year = match.captured(1).toInt();
        yearStart = match.capturedStart();
    }

    if (yearStart >= 0) {
        cut = qMin(cut, yearStart);
    }

    QString nameWithTheYear;
    if (yearStart >= 0 && episodeMarkerStart > yearStart) {
        nameWithTheYear = cleanTitle(working.left(episodeMarkerStart));
    }

    const QStringList tokens = working.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    int offset = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const QString &token = tokens.at(i);
        const int start = working.indexOf(token, offset);
        offset = start + token.size();

        if (start == 0 || !isJunkToken(token)) {
            continue;
        }

        if (leavesNothingUsable(working.left(start))) {
            continue;
        }

        if (isSoftJunkToken(token)) {
            const bool nextIsJunk = i + 1 < tokens.size()
                && isJunkToken(tokens.at(i + 1));
            const bool afterTheYear = yearStart >= 0 && start > yearStart;
            if (!nextIsJunk && !afterTheYear) {
                continue;
            }
        }

        cut = qMin(cut, start);
        break;
    }

    result.title = cleanTitle(working.left(cut));
    if (episodeAwaitsASeason) {
        result.titleIfEpisode =
            cleanTitle(working.left(qMin(cut, episodeOnlyStart)));
    }

    if (result.title.isEmpty()) {
        result.title = cleanTitle(working);
        result.titleFromFallback = true;
    }

    static const QRegularExpression alsoKnownAs(
        QStringLiteral("\\s+(?:AKA|A\\.K\\.A\\.?)\\s+"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch aka = alsoKnownAs.match(result.title);
    if (aka.hasMatch()) {
        const QString first = cleanTitle(result.title.left(aka.capturedStart()));
        const QString second = cleanTitle(result.title.mid(aka.capturedEnd()));
        if (!first.isEmpty() && !second.isEmpty()) {
            result.title = first;
            result.alternativeTitle = second;
        }
    }

    if (!nameWithTheYear.isEmpty() && nameWithTheYear != result.title
        && result.alternativeTitle.isEmpty()) {
        result.alternativeTitle = nameWithTheYear;
    }

    if (episodeMarkerEnd >= 0 && episodeMarkerEnd < working.size()) {
        const QString tail = working.mid(episodeMarkerEnd).simplified();
        const QStringList tailTokens = tail.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        QStringList kept;
        for (const QString &token : tailTokens) {
            if (isJunkToken(token) || yearPattern.match(token).hasMatch()) {
                break;
            }
            kept.append(token);
        }
        result.episodeTitle = cleanTitle(kept.join(QLatin1Char(' ')));
    }

    static const QRegularExpression groupPattern(
        QStringLiteral("-([A-Za-z0-9]{2,})$"));
    const QRegularExpressionMatch group = groupPattern.match(base.trimmed());
    if (group.hasMatch()) {
        result.releaseGroup = group.captured(1);
    }

    return result;
}

QList<Ancestor> ancestorsOf(const QString &filePath)
{
    struct Segment
    {
        qsizetype start = 0;
        qsizetype end = 0;
    };

    QList<Segment> folders;
    qsizetype start = 0;
    for (qsizetype i = 0; i < filePath.size(); ++i) {
        const QChar c = filePath.at(i);
        if (c == QLatin1Char('/') || c == QLatin1Char('\\')) {
            folders.append({start, i});
            start = i + 1;
        }
    }

    QList<Ancestor> ancestors;
    for (qsizetype index = folders.size() - 1; index >= 0; --index) {
        if (ancestors.size() >= kAncestorsRead) {
            break;
        }

        const Segment &segment = folders.at(index);
        QString name = filePath.mid(segment.start, segment.end - segment.start);
        if (index == 0) {
            name = name.mid(name.lastIndexOf(QLatin1Char(':')) + 1);
        }
        if (name.trimmed().isEmpty()) {
            continue;
        }

        ancestors.append({name, filePath.left(segment.end)});
    }

    return ancestors;
}

ParsedFileName parsePath(const QString &filePath)
{
    ParsedFileName result = parse(filePath);

    const bool wantSeason = result.season <= 0 && !result.specials;
    const bool wantTitle = result.title.isEmpty() || result.titleFromFallback;
    if (!wantSeason && !wantTitle) {
        return result;
    }

    const QList<Ancestor> ancestors = ancestorsOf(filePath);

    if (wantSeason) {
        for (const Ancestor &ancestor : ancestors) {
            const int season = seasonFromFolder(ancestor.name);
            if (season > 0) {
                result.season = season;
                break;
            }
        }
    }

    if ((result.season > 0 || result.specials) && !result.titleIfEpisode.isEmpty()) {
        result.title = result.titleIfEpisode;
        result.titleIfEpisode.clear();
    }

    if (!wantTitle) {
        return result;
    }

    for (const Ancestor &ancestor : ancestors) {
        if (isSeasonOnlyFolder(ancestor.name)) {
            continue;
        }

        const ParsedFileName folder = parse(ancestor.name);
        if (folder.title.isEmpty() || folder.titleFromFallback) {
            continue;
        }

        result.title = folder.title;
        result.alternativeTitle = folder.alternativeTitle;
        result.titleFromFallback = false;
        if (result.year == 0) {
            result.year = folder.year;
        }
        if (result.season <= 0 && !result.specials && folder.season > 0) {
            result.season = folder.season;
        }
        break;
    }

    return result;
}

}
