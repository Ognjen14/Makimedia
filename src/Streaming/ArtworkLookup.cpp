#include "Streaming/ArtworkLookup.h"

#include "Metadata/TmdbImageUrl.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <vector>

namespace {

using Makimedia::Tmdb::ImageKind;
using Makimedia::Tmdb::TmdbImageUrl;

QString usableFile(const QString &cacheDir, const QString &key)
{
    const QString path = cacheDir + QLatin1Char('/') + ArtworkLookup::cacheFileName(key);
    const QFileInfo info(path);
    return info.isFile() && info.size() > 0 ? path : QString();
}

}

namespace ArtworkLookup {

QString cacheFileName(const QString &key)
{
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),
                                                        QCryptographicHash::Sha1).toHex())
           + QStringLiteral(".jpg");
}

QString fileFor(const QString &cacheDir, const QString &key)
{
    if (cacheDir.isEmpty() || key.isEmpty() || key.size() > 512) {
        return QString();
    }

    const QString exact = usableFile(cacheDir, key);
    if (!exact.isEmpty()) {
        return exact;
    }

    QString prefix;
    ImageKind kind = ImageKind::Poster;
    if (key.startsWith(QLatin1String("still:"))) {
        prefix = QStringLiteral("still:");
        kind = ImageKind::Still;
    } else if (key.startsWith(QLatin1String("backdrop:"))) {
        prefix = QStringLiteral("backdrop:");
        kind = ImageKind::Backdrop;
    } else if (key.startsWith(QLatin1String("profile:"))) {
        prefix = QStringLiteral("profile:");
        kind = ImageKind::Profile;
    }

    const qsizetype at = key.lastIndexOf(QLatin1Char('@'));
    if (at <= prefix.size()) {
        return QString();
    }
    const QString path = key.mid(prefix.size(), at - prefix.size());

    const std::vector<int> widths = TmdbImageUrl::widths(kind);
    for (auto width = widths.crbegin(); width != widths.crend(); ++width) {
        const QString other = usableFile(cacheDir,
                                         prefix + path + QLatin1Char('@') + QString::number(*width));
        if (!other.isEmpty()) {
            return other;
        }
    }
    return QString();
}

}
