#include "Library/ListText.h"

#include "Data/FileRepository.h"

#include <QRegularExpression>
#include <QStringList>

namespace ListText {

QString clock(double seconds)
{
    if (seconds <= 0.0) {
        return QString();
    }
    const int total = static_cast<int>(seconds);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QString duration(double seconds)
{
    if (seconds <= 0.0) {
        return QString();
    }
    if (seconds < 60.0) {
        return QStringLiteral("<1m");
    }
    const int total = static_cast<int>(seconds);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    if (hours > 0) {
        return QStringLiteral("%1h %2m").arg(hours).arg(minutes);
    }
    return QStringLiteral("%1m").arg(minutes);
}

QString remaining(const LibraryFile &file)
{
    if (file.playback.watched) {
        return QString();
    }
    const double total = file.durationSeconds > 0.0
        ? file.durationSeconds
        : file.playback.durationSeconds;
    const double left = total - file.playback.positionSeconds;
    return left > 0.0 ? clock(left) : QString();
}

QString location(const QString &parentHandle)
{
    QString path = parentHandle;
    const qsizetype colon = path.indexOf(QLatin1Char(':'));
    if (colon > 1) {
        const QString volume = path.left(colon);
        if (!volume.contains(QLatin1Char('/')) && !volume.contains(QLatin1Char('\\'))) {
            path = path.mid(colon + 1);
        }
    }

    QStringList parts;
    const QStringList pieces = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &piece : pieces) {
        parts += piece.split(QLatin1Char('\\'), Qt::SkipEmptyParts);
    }
    if (parts.isEmpty()) {
        return QString();
    }

    const qsizetype take = qMin<qsizetype>(2, parts.size());
    return parts.mid(parts.size() - take).join(QStringLiteral(" / "));
}

QString prettyTitle(const QString &fileName)
{
    static const QRegularExpression extension(
        QStringLiteral("\\.[a-z0-9]{2,4}$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression separators(QStringLiteral("[._]+"));
    static const QRegularExpression spaces(
        QStringLiteral("\\s+"), QRegularExpression::UseUnicodePropertiesOption);
    static const QRegularExpression dangling(
        QStringLiteral("^[-–—·|]+$"));
    static const QRegularExpression year(
        QStringLiteral("^\\(?((?:19|20)\\d{2})\\)?$"));
    static const QRegularExpression junk(
        QStringLiteral(
            "^[\\(\\[\\{]*(?:\\d{3,4}p|4k|uhd|web|webrip|web-dl|webdl|bluray|"
            "blu-ray|bdrip|brrip|dvdrip|dvdscr|hdtv|hdrip|remux|x264|x265|h264|"
            "h265|hevc|avc|xvid|divx|aac\\d*|ac3|eac3|ddp\\d*|dd\\d*|dts|dtshd|"
            "truehd|atmos|flac|opus|mp3|10bit|8bit|hdr\\d*|dolby|sdr|proper|"
            "repack|extended|uncut|remastered|multi|dual|subbed|dubbed|yts|yify|"
            "rarbg|internal|limited)[\\)\\]\\}]*$"),
        QRegularExpression::CaseInsensitiveOption);

    if (fileName.isEmpty()) {
        return QString();
    }

    const qsizetype slash = qMax(fileName.lastIndexOf(QLatin1Char('/')),
                                 fileName.lastIndexOf(QLatin1Char('\\')));
    QString base = slash >= 0 ? fileName.mid(slash + 1) : fileName;
    base.remove(extension);
    if (base.isEmpty()) {
        return QString();
    }

    QString spaced = base;
    spaced.replace(separators, QStringLiteral(" "));
    spaced.replace(spaces, QStringLiteral(" "));
    spaced = spaced.trimmed();
    if (spaced.isEmpty()) {
        return base;
    }

    const QStringList tokens = spaced.split(QLatin1Char(' '));
    QStringList kept;
    for (const QString &token : tokens) {
        if (junk.match(token).hasMatch()) {
            break;
        }
        kept.append(token);
    }

    while (!kept.isEmpty() && dangling.match(kept.last()).hasMatch()) {
        kept.removeLast();
    }

    if (kept.isEmpty()) {
        return spaced;
    }

    if (kept.size() > 1) {
        const QRegularExpressionMatch found = year.match(kept.last());
        if (found.hasMatch()) {
            kept.last() = QStringLiteral("(") + found.captured(1) + QStringLiteral(")");
        }
    }

    return kept.join(QLatin1Char(' '));
}

}
