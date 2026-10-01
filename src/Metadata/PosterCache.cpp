#include "Metadata/PosterCache.h"

#include "MmLog.h"
#include "Streaming/StreamProtocol.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

PosterCache::PosterCache(QNetworkAccessManager &network, QObject *parent)
    : PosterCache(network, defaultDirectory(), parent)
{
}

QString PosterCache::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           + QStringLiteral("/posters");
}

PosterCache::PosterCache(QNetworkAccessManager &network,
                         const QString &cacheDir,
                         QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_cacheDir(cacheDir)
{
    if (m_cacheDir.isEmpty() || !QDir().mkpath(m_cacheDir)) {
        MM_LOG_E() << "could not create the poster cache directory" << m_cacheDir;
        m_cacheDir.clear();
        return;
    }

    MM_LOG_I() << "poster cache at" << m_cacheDir;
}

QString PosterCache::filePathFor(const QString &key) const
{
    if (m_cacheDir.isEmpty() || key.isEmpty()) {
        return QString();
    }

    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(),
                                                     QCryptographicHash::Sha1);
    return m_cacheDir + QLatin1Char('/') + QString::fromLatin1(hash.toHex())
           + QStringLiteral(".jpg");
}

QString PosterCache::usablePathFor(const QString &key) const
{
    const auto known = m_known.constFind(key);
    if (known != m_known.constEnd()) {
        return known.value();
    }

    const QString path = filePathFor(key);
    if (path.isEmpty()) {
        return QString();
    }

    QString usable;
    const QFileInfo info(path);
    if (info.exists()) {
        if (info.size() > 0) {
            usable = path;
        } else {
            QFile::remove(path);
            MM_LOG_W() << "removed an empty cached poster so it is fetched again"
                       << key;
        }
    }

    m_known.insert(key, usable);
    return usable;
}

bool PosterCache::isCached(const QString &key) const
{
    return !usablePathFor(key).isEmpty();
}

QString PosterCache::localUrl(const QString &key) const
{
    const QString path = usablePathFor(key);
    if (path.isEmpty()) {
        return QString();
    }
    return QUrl::fromLocalFile(path).toString();
}

QString PosterCache::urlFor(const QString &remoteUrl, const QString &key)
{
    if (remoteUrl.isEmpty()) {
        return QString();
    }

    if (m_cacheDir.isEmpty() || key.isEmpty()) {
        return remoteUrl;
    }

    const QString cached = localUrl(key);
    if (!cached.isEmpty()) {
        return cached;
    }

    download(remoteUrl, key);
    return remoteUrl;
}

bool PosterCache::fetch(const QString &remoteUrl, const QString &key)
{
    if (remoteUrl.isEmpty() || m_cacheDir.isEmpty() || key.isEmpty()
        || isCached(key)) {
        return false;
    }
    return download(remoteUrl, key);
}

bool PosterCache::isUsable() const
{
    return !m_cacheDir.isEmpty();
}

bool PosterCache::store(const QString &key, const QByteArray &bytes)
{
    const QString path = filePathFor(key);
    if (path.isEmpty()) {
        return false;
    }

    if (!looksLikeAnImage(bytes)) {
        MM_LOG_W() << "poster download is not an image, not caching it" << key
                   << bytes.size() << "bytes";
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        MM_LOG_W() << "could not write the poster to" << path << file.errorString();
        return false;
    }

    if (file.write(bytes) != bytes.size() || !file.commit()) {
        MM_LOG_W() << "could not finish writing the poster to" << path
                   << file.errorString();
        return false;
    }

    m_known.insert(key, path);
    emit posterSaved(key, QUrl::fromLocalFile(path).toString());
    return true;
}

bool PosterCache::download(const QString &remoteUrl, const QString &key)
{
    if (m_inFlight.contains(key)) {
        return false;
    }
    m_inFlight.insert(key, true);

    QNetworkRequest request{QUrl(remoteUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);

    const int generation = m_generation;
    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, key, generation]() {
        reply->deleteLater();

        if (generation != m_generation) {
            MM_LOG_I() << "poster arrived after the cache was cleared, dropped" << key;
            return;
        }
        m_inFlight.remove(key);

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::ContentNotFoundError) {
                MM_LOG_D() << "no picture at" << key
                           << "where it was asked for - trying elsewhere";
            } else {
                MM_LOG_W() << "poster download failed" << key
                           << StreamProtocol::withoutToken(reply->errorString());
            }
            emit posterFailed(key);
            return;
        }

        if (!store(key, reply->readAll())) {
            emit posterFailed(key);
        }
    });
    return true;
}

void PosterCache::clear()
{
    ++m_generation;
    m_inFlight.clear();
    m_known.clear();

    if (m_cacheDir.isEmpty()) {
        return;
    }

    QDir dir(m_cacheDir);
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString &name : files) {
        dir.remove(name);
    }

    MM_LOG_I() << "poster cache cleared," << files.size() << "files removed";
}

bool PosterCache::looksLikeAnImage(const QByteArray &bytes)
{
    static const QByteArray jpeg = QByteArray::fromHex("ffd8ff");
    static const QByteArray png = QByteArray::fromHex("89504e470d0a1a0a");
    static const QByteArray gif = QByteArrayLiteral("GIF8");

    if (bytes.startsWith(jpeg) || bytes.startsWith(png) || bytes.startsWith(gif)) {
        return true;
    }

    return bytes.size() >= 12
        && bytes.startsWith("RIFF")
        && bytes.mid(8, 4) == "WEBP";
}
