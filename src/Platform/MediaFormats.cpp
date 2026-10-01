#include "Platform/MediaFileInfo.h"

#include <QFile>
#include <QFileInfo>

namespace MediaFormats {

const QStringList &videoSuffixes()
{
    static const QStringList suffixes = {
        QStringLiteral("mkv"), QStringLiteral("mp4"),  QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("m4v"),
        QStringLiteral("ts"),  QStringLiteral("m2ts"), QStringLiteral("mts"),
        QStringLiteral("wmv"), QStringLiteral("flv"),  QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("ogv"), QStringLiteral("3gp"),
        QStringLiteral("divx"), QStringLiteral("vob"), QStringLiteral("rmvb")
    };
    return suffixes;
}

bool isVideoFile(const QString &pathOrName)
{
    if (pathOrName.isEmpty()) {
        return false;
    }
    return videoSuffixes().contains(QFileInfo(pathOrName).suffix().toLower());
}

bool looksLikeVideoFile(const QString &path)
{
    if (!isVideoFile(path)) {
        return false;
    }

    static const QStringList ambiguous = { QStringLiteral("ts") };
    if (!ambiguous.contains(QFileInfo(path).suffix().toLower())) {
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray head = file.read(377);
    if (head.size() < 377) {
        return false;
    }

    const char sync = char(0x47);
    return head.at(0) == sync && head.at(188) == sync && head.at(376) == sync;
}

bool isInSystemFolder(const QString &path)
{
    static const QStringList folders = {
        QStringLiteral("$recycle.bin"), QStringLiteral("recycler"),
        QStringLiteral("system volume information"),
        QStringLiteral("lost.dir"),
        QStringLiteral("lost+found"), QStringLiteral(".trashes"),
        QStringLiteral(".spotlight-v100"), QStringLiteral(".fseventsd")
    };

    const QStringList segments = QString(path).replace(QLatin1Char('\\'), QLatin1Char('/'))
                                     .split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        const QString folded = segment.toLower();
        if (folders.contains(folded) || folded.startsWith(QLatin1String(".trash-"))) {
            return true;
        }
    }
    return false;
}

}
