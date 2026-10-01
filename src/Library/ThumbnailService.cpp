#include "Library/ThumbnailService.h"

#include "MmLog.h"
#include "Platform/IMediaSource.h"

#ifdef Q_OS_ANDROID
#  include "Platform/Android/AndroidMediaSource.h"
#endif

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

#include <utility>

#include <mpv/client.h>

namespace {

const int kMaxWaitMs = 15000;

bool grabFrame(const QString &path, const QString &outputPath)
{
    mpv_handle *mpv = mpv_create();
    if (!mpv) {
        return false;
    }

    mpv_set_option_string(mpv, "vo", "null");
    mpv_set_option_string(mpv, "ao", "null");
    mpv_set_option_string(mpv, "audio", "no");
    mpv_set_option_string(mpv, "sub", "no");
    mpv_set_option_string(mpv, "hwdec", "no");
    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "osc", "no");
    mpv_set_option_string(mpv, "pause", "yes");
    mpv_set_option_string(mpv, "start", "25%");
    mpv_set_option_string(mpv, "vf", "scale=480:-2");
    mpv_set_option_string(mpv, "screenshot-format", "jpg");
    mpv_set_option_string(mpv, "screenshot-jpeg-quality", "80");

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        return false;
    }

    const QByteArray pathUtf8 = path.toUtf8();
    const char *loadArgs[] = {"loadfile", pathUtf8.constData(), nullptr};
    if (mpv_command(mpv, loadArgs) < 0) {
        mpv_terminate_destroy(mpv);
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    bool ready = false;

    while (timer.elapsed() < kMaxWaitMs) {
        mpv_event *event = mpv_wait_event(mpv, 0.25);
        if (!event) {
            continue;
        }
        if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            ready = true;
            break;
        }
        if (event->event_id == MPV_EVENT_END_FILE
            || event->event_id == MPV_EVENT_SHUTDOWN) {
            break;
        }
    }

    bool ok = false;
    if (ready) {
        const QByteArray outUtf8 = QDir::toNativeSeparators(outputPath).toUtf8();
        const char *shotArgs[] = {"screenshot-to-file", outUtf8.constData(),
                                  "video", nullptr};
        ok = mpv_command(mpv, shotArgs) >= 0;
    }

    mpv_terminate_destroy(mpv);

    if (ok && !QFileInfo::exists(outputPath)) {
        ok = false;
    }
    return ok;
}

}

ThumbnailService::ThumbnailService(QObject *parent)
    : QObject(parent)
{
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                 + QStringLiteral("/thumbnails");

    if (!QDir().mkpath(m_cacheDir)) {
        MM_LOG_E() << "could not create the thumbnail cache at" << m_cacheDir;
        m_cacheDir.clear();
        return;
    }

    MM_LOG_I() << "thumbnail cache at" << m_cacheDir;
}

ThumbnailService::~ThumbnailService() = default;

QString ThumbnailService::cacheFilePath(const QString &handle) const
{
    if (m_cacheDir.isEmpty()) {
        return QString();
    }

    const QByteArray digest = QCryptographicHash::hash(handle.toUtf8(),
                                                       QCryptographicHash::Sha1);
    return m_cacheDir + QLatin1Char('/') + QString::fromLatin1(digest.toHex())
           + QStringLiteral(".jpg");
}

QString ThumbnailService::cachedUrl(const QString &handle) const
{
    const auto known = m_urls.constFind(handle);
    if (known != m_urls.constEnd()) {
        return known.value();
    }

    const QString file = cacheFilePath(handle);
    if (file.isEmpty()) {
        return QString();
    }

    const QString url = QFileInfo::exists(file)
        ? QUrl::fromLocalFile(file).toString()
        : QString();
    m_urls.insert(handle, url);
    return url;
}

bool ThumbnailService::discard(const QString &handle)
{
    if (handle.isEmpty() || m_discarded.contains(handle)) {
        return false;
    }
    m_discarded.insert(handle);

    const QString file = cacheFilePath(handle);
    if (file.isEmpty() || !QFileInfo::exists(file)) {
        return false;
    }

    if (!QFile::remove(file)) {
        MM_LOG_W() << "could not delete the redundant thumbnail" << file;
        return false;
    }
    m_urls.insert(handle, QString());

    m_seen.remove(handle);
    return true;
}

void ThumbnailService::request(const QString &handle, const QString &path)
{
    if (!m_enabled || m_cacheDir.isEmpty() || handle.isEmpty()
        || path.isEmpty()) {
        return;
    }
    if (m_seen.contains(handle)) {
        return;
    }
    if (!cachedUrl(handle).isEmpty()) {
        return;
    }

    m_discarded.remove(handle);
    m_seen.insert(handle, true);

    if (m_deferred) {
        m_held.enqueue({handle, path});
        return;
    }

    m_queue.enqueue({handle, path});
    startNext();
}

void ThumbnailService::setWanted(const QStringList &handles)
{
    m_wanted = handles;
    const QSet<QString> wanted(handles.cbegin(), handles.cend());
    m_wantedSet = wanted;
    m_wantedDirty = false;

    int dropped = 0;
    const auto keepWanted = [this, &wanted, &dropped](QQueue<PendingItem> &queue) {
        QQueue<PendingItem> kept;
        for (const PendingItem &item : std::as_const(queue)) {
            if (wanted.contains(item.handle)) {
                kept.enqueue(item);
            } else {
                m_seen.remove(item.handle);
                ++dropped;
            }
        }
        queue = kept;
    };
    keepWanted(m_queue);
    keepWanted(m_held);

    if (dropped > 0) {
        MM_LOG_I() << "dropped" << dropped << "thumbnails no list shows any more";
    }

    const QStringList current = m_wanted;
    for (const QString &handle : current) {
        request(handle, handle);
    }
}

void ThumbnailService::want(const QStringList &handles)
{
    for (const QString &handle : handles) {
        if (handle.isEmpty() || m_wantedSet.contains(handle)) {
            continue;
        }
        m_wantedSet.insert(handle);
        m_wanted.append(handle);
        request(handle, handle);
    }

    if (m_wantedDirty && m_wanted.size() > 2 * m_wantedSet.size() + 64) {
        compactWanted();
    }
}

void ThumbnailService::unwant(const QStringList &handles)
{
    QSet<QString> gone;
    for (const QString &handle : handles) {
        if (m_wantedSet.remove(handle)) {
            gone.insert(handle);
        }
    }

    if (gone.isEmpty()) {
        return;
    }
    m_wantedDirty = true;

    int dropped = 0;
    const auto dropGone = [this, &gone, &dropped](QQueue<PendingItem> &queue) {
        QQueue<PendingItem> kept;
        for (const PendingItem &item : std::as_const(queue)) {
            if (gone.contains(item.handle)) {
                m_seen.remove(item.handle);
                ++dropped;
            } else {
                kept.enqueue(item);
            }
        }
        queue = kept;
    };

    if (!m_queue.isEmpty()) {
        dropGone(m_queue);
    }
    if (!m_held.isEmpty()) {
        dropGone(m_held);
    }

    if (dropped > 0) {
        MM_LOG_I() << "dropped" << dropped << "thumbnails no list shows any more";
    }
}

void ThumbnailService::compactWanted()
{
    if (!m_wantedDirty) {
        return;
    }

    QStringList kept;
    kept.reserve(m_wantedSet.size());
    QSet<QString> added;
    for (const QString &handle : std::as_const(m_wanted)) {
        if (m_wantedSet.contains(handle) && !added.contains(handle)) {
            added.insert(handle);
            kept.append(handle);
        }
    }

    m_wanted = kept;
    m_wantedDirty = false;
}

void ThumbnailService::requestWanted()
{
    compactWanted();
    const QStringList current = m_wanted;
    for (const QString &handle : current) {
        if (!m_discarded.contains(handle)) {
            request(handle, handle);
        }
    }
}

void ThumbnailService::setDeferred(bool deferred)
{
    if (m_deferred == deferred) {
        return;
    }
    m_deferred = deferred;

    if (deferred) {
        while (!m_queue.isEmpty()) {
            m_held.enqueue(m_queue.dequeue());
        }
        MM_LOG_I() << "holding thumbnail generation while metadata is fetched";
        return;
    }

    if (!m_unresolved.isEmpty()) {
        for (const QString &handle : std::as_const(m_unresolved)) {
            m_seen.remove(handle);
        }
        MM_LOG_I() << m_unresolved.size()
                   << "thumbnails that had no playable url can be asked for again";
        m_unresolved.clear();
    }

    if (m_held.isEmpty()) {
        MM_LOG_I() << "thumbnail generation released";
        requestWanted();
        return;
    }

    int matched = 0;
    while (!m_held.isEmpty()) {
        const PendingItem item = m_held.dequeue();
        if (m_discarded.contains(item.handle)) {
            m_seen.remove(item.handle);
            ++matched;
            continue;
        }
        m_queue.enqueue(item);
    }

    MM_LOG_I() << "metadata done, generating" << m_queue.size()
               << "held thumbnails, dropped" << matched << "now matched";
    requestWanted();
    startNext();
}

void ThumbnailService::setMediaSource(IMediaSource *mediaSource)
{
    m_mediaSource = mediaSource;
}

void ThumbnailService::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;

    if (!enabled) {
        for (const PendingItem &item : std::as_const(m_queue)) {
            m_seen.remove(item.handle);
        }
        for (const PendingItem &item : std::as_const(m_held)) {
            m_seen.remove(item.handle);
        }
        m_queue.clear();
        m_held.clear();
        MM_LOG_I() << "thumbnail generation disabled on this device";
        return;
    }

    requestWanted();
}

void ThumbnailService::startNext()
{
    while (!m_running && !m_queue.isEmpty()) {
        const PendingItem item = m_queue.dequeue();
        const QString output = cacheFilePath(item.handle);
        if (output.isEmpty()) {
            return;
        }

#ifdef Q_OS_ANDROID
        if (AndroidMediaSource::writeSystemThumbnail(item.handle, output, 480)) {
            notifyGenerated(item.handle, true);
            continue;
        }
#endif

        QString source = item.path;
        if (m_mediaSource) {
            source = m_mediaSource->mpvUrl(item.handle);
            if (source.isEmpty()) {
                MM_LOG_W() << "could not resolve a playable url for"
                           << item.handle << "- trying again after the next hold";
                m_unresolved.insert(item.handle);
                notifyGenerated(item.handle, false);
                continue;
            }
        }

        startWorker(item.handle, source, output);
        return;
    }
}

void ThumbnailService::startWorker(const QString &handle, const QString &source,
                                   const QString &output)
{
    m_running = true;

    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this,
            [this, watcher, handle]() {
        const bool ok = watcher->result();
        watcher->deleteLater();
        onGenerated(handle, ok);
    });

    watcher->setFuture(QtConcurrent::run(grabFrame, source, output));
}

void ThumbnailService::notifyGenerated(const QString &handle, bool ok)
{
    if (ok) {
        m_urls.remove(handle);
        const QString url = cachedUrl(handle);
        MM_LOG_D() << "thumbnail generated for" << handle;
        emit thumbnailReady(handle, url);
    } else {
        MM_LOG_W() << "could not generate a thumbnail for" << handle;
    }
}

void ThumbnailService::onGenerated(const QString &handle, bool ok)
{
    m_running = false;
    notifyGenerated(handle, ok);
    startNext();
}

void ThumbnailService::clearCache()
{
    if (m_cacheDir.isEmpty()) {
        return;
    }

    QDir dir(m_cacheDir);
    const QStringList files = dir.entryList(QStringList{QStringLiteral("*.jpg")},
                                            QDir::Files);
    int removed = 0;
    for (const QString &file : files) {
        if (dir.remove(file)) {
            ++removed;
        }
    }

    m_seen.clear();
    m_discarded.clear();
    m_queue.clear();
    m_held.clear();
    m_urls.clear();
    m_unresolved.clear();
    MM_LOG_I() << "cleared" << removed << "cached thumbnails";

    requestWanted();
}
