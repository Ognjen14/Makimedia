#include "Library/ProbeService.h"

#include "MmLog.h"
#include "Platform/IMediaSource.h"
#include "Player/VideoTraits.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent>

#include <utility>

#include <mpv/client.h>

namespace {

const int kMaxWaitMs = 15000;
const int kFailuresLogged = 5;

QString stringProperty(mpv_handle *mpv, const char *name)
{
    char *value = nullptr;
    if (mpv_get_property(mpv, name, MPV_FORMAT_STRING, &value) < 0 || !value) {
        return QString();
    }
    const QString result = QString::fromUtf8(value);
    mpv_free(value);
    return result;
}

qint64 intProperty(mpv_handle *mpv, const char *name)
{
    qint64 value = 0;
    if (mpv_get_property(mpv, name, MPV_FORMAT_INT64, &value) < 0) {
        return 0;
    }
    return value;
}

double doubleProperty(mpv_handle *mpv, const char *name)
{
    double value = 0.0;
    if (mpv_get_property(mpv, name, MPV_FORMAT_DOUBLE, &value) < 0) {
        return 0.0;
    }
    return value;
}

bool flagProperty(mpv_handle *mpv, const QByteArray &name)
{
    int value = 0;
    if (mpv_get_property(mpv, name.constData(), MPV_FORMAT_FLAG, &value) < 0) {
        return false;
    }
    return value != 0;
}

void countTracks(mpv_handle *mpv, ProbeInfo *info)
{
    const qint64 count = intProperty(mpv, "track-list/count");
    for (qint64 i = 0; i < count; ++i) {
        const QByteArray base = QByteArrayLiteral("track-list/")
            + QByteArray::number(i);
        if (flagProperty(mpv, base + QByteArrayLiteral("/external"))) {
            continue;
        }

        const QString type = stringProperty(
            mpv, (base + QByteArrayLiteral("/type")).constData());
        if (type == QLatin1String("audio")) {
            ++info->audioTrackCount;
        } else if (type == QLatin1String("sub")) {
            ++info->subtitleTrackCount;
        }
    }
}

ProbeInfo probeFile(const QString &handle, const QString &path)
{
    ProbeInfo info;
    info.handle = handle;

    mpv_handle *mpv = mpv_create();
    if (!mpv) {
        return info;
    }

    mpv_set_option_string(mpv, "vo", "null");
    mpv_set_option_string(mpv, "ao", "null");
    mpv_set_option_string(mpv, "hwdec", "no");
    mpv_set_option_string(mpv, "config", "no");
    mpv_set_option_string(mpv, "terminal", "no");
    mpv_set_option_string(mpv, "osc", "no");
    mpv_set_option_string(mpv, "pause", "yes");
    mpv_set_option_string(mpv, "sid", "no");
    mpv_set_option_string(mpv, "sub-auto", "no");

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        return info;
    }

    const QByteArray pathUtf8 = path.toUtf8();
    const char *loadArgs[] = {"loadfile", pathUtf8.constData(), nullptr};
    if (mpv_command(mpv, loadArgs) < 0) {
        mpv_terminate_destroy(mpv);
        return info;
    }

    QElapsedTimer timer;
    timer.start();
    bool ready = false;

    while (timer.elapsed() < kMaxWaitMs) {
        mpv_event *event = mpv_wait_event(mpv, 0.25);
        if (!event) {
            continue;
        }
        if (event->event_id == MPV_EVENT_PLAYBACK_RESTART
            || event->event_id == MPV_EVENT_FILE_LOADED) {
            ready = true;
            break;
        }
        if (event->event_id == MPV_EVENT_END_FILE
            || event->event_id == MPV_EVENT_SHUTDOWN) {
            break;
        }
    }

    if (ready) {
        info.durationSeconds = doubleProperty(mpv, "duration");
        info.container = stringProperty(mpv, "file-format");
        info.videoCodec = stringProperty(mpv, "video-codec");
        info.audioCodec = stringProperty(mpv, "audio-codec-name");
        info.width = int(intProperty(mpv, "width"));
        info.height = int(intProperty(mpv, "height"));
        info.hdr = VideoTraits::isHdr(
            stringProperty(mpv, "video-params/primaries"),
            stringProperty(mpv, "video-params/gamma"));
        countTracks(mpv, &info);

        info.valid = info.durationSeconds > 0.0;
    }

    mpv_terminate_destroy(mpv);
    return info;
}

}

ProbeService::ProbeService(QObject *parent)
    : QObject(parent)
{
}

ProbeService::~ProbeService() = default;

void ProbeService::setMediaSource(IMediaSource *mediaSource)
{
    m_mediaSource = mediaSource;
}

void ProbeService::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;

    if (!enabled) {
        for (const ProbeRequest &item : std::as_const(m_queue)) {
            m_seen.remove(item.handle);
            m_waiting.remove(item.handle);
        }
        for (const ProbeRequest &item : std::as_const(m_held)) {
            m_seen.remove(item.handle);
            m_waiting.remove(item.handle);
        }
        m_queue.clear();
        m_held.clear();
        if (!m_running) {
            m_probedOk = 0;
            m_probedFailed = 0;
            m_passTimer.invalidate();
        }
        MM_LOG_I() << "file probing disabled on this device";
        emit pendingChanged();
    }
}

bool ProbeService::isEnabled() const
{
    return m_enabled;
}

void ProbeService::setDeferred(bool deferred)
{
    if (m_deferred == deferred) {
        return;
    }
    m_deferred = deferred;

    if (deferred) {
        while (!m_queue.isEmpty()) {
            m_held.enqueue(m_queue.dequeue());
        }
        MM_LOG_I() << "holding file probing while metadata is fetched";
        return;
    }

    if (m_held.isEmpty()) {
        MM_LOG_I() << "file probing released";
        return;
    }

    while (!m_held.isEmpty()) {
        m_queue.enqueue(m_held.dequeue());
    }

    MM_LOG_I() << "metadata done, probing" << m_queue.size() << "held files";
    emit pendingChanged();
    startNext();
}

void ProbeService::request(const QString &handle, const QString &path)
{
    if (!m_enabled || handle.isEmpty() || path.isEmpty()) {
        return;
    }
    if (m_seen.contains(handle)) {
        return;
    }
    m_seen.insert(handle);
    m_waiting.insert(handle);

    if (m_deferred) {
        m_held.enqueue({handle, path});
        return;
    }

    m_queue.enqueue({handle, path});
    emit pendingChanged();
    startNext();
}

void ProbeService::requestAll(const QList<ProbeRequest> &files)
{
    if (files.isEmpty()) {
        return;
    }

    const int before = pending();
    for (const ProbeRequest &file : files) {
        if (m_waiting.contains(file.handle)) {
            continue;
        }
        m_seen.remove(file.handle);
        request(file.handle, file.path);
    }

    const int added = pending() - before;
    if (added > 0) {
        MM_LOG_I() << "queued" << added << "files for probing";
    }
}

int ProbeService::pending() const
{
    return m_queue.size() + m_held.size() + (m_running ? 1 : 0);
}

void ProbeService::startNext()
{
    if (m_running || m_queue.isEmpty()) {
        return;
    }
    if (!m_passTimer.isValid()) {
        m_passTimer.start();
    }

    const ProbeRequest item = m_queue.dequeue();

    QString source = item.path;
    if (m_mediaSource) {
        source = m_mediaSource->mpvUrl(item.handle);
        if (source.isEmpty()) {
            m_running = true;
            ProbeInfo failed;
            failed.handle = item.handle;
            QMetaObject::invokeMethod(this, [this, failed]() {
                m_running = false;
                onProbed(failed);
                startNext();
            }, Qt::QueuedConnection);
            return;
        }
    }

    startWorker(item.handle, source);
}

void ProbeService::startWorker(const QString &handle, const QString &source)
{
    m_running = true;

    auto *watcher = new QFutureWatcher<ProbeInfo>(this);
    connect(watcher, &QFutureWatcher<ProbeInfo>::finished, this,
            [this, watcher]() {
        const ProbeInfo info = watcher->result();
        watcher->deleteLater();
        m_running = false;
        onProbed(info);
        startNext();
    });

    watcher->setFuture(QtConcurrent::run(probeFile, handle, source));
}

void ProbeService::onProbed(const ProbeInfo &info)
{
    m_waiting.remove(info.handle);

    if (info.valid) {
        ++m_probedOk;
    } else {
        ++m_probedFailed;
        if (m_probedFailed <= kFailuresLogged) {
            MM_LOG_W() << "could not probe" << info.handle
                       << "- it will not be tried again until the next scan";
        }
    }

    emit probed(info);
    emit pendingChanged();

    if (m_queue.isEmpty() && !m_running) {
        if (m_probedFailed > kFailuresLogged) {
            MM_LOG_W() << "and" << (m_probedFailed - kFailuresLogged)
                       << "more could not be probed";
        }
        MM_LOG_I() << "probe pass done:" << m_probedOk << "probed,"
                   << m_probedFailed << "failed, in"
                   << m_passTimer.elapsed() << "ms";
        m_probedOk = 0;
        m_probedFailed = 0;
        m_passTimer.invalidate();
    }
}
