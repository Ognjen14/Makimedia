#include "Player/SeekPreviewService.h"

#include "MmLog.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

#include <mpv/client.h>

namespace {

const int kPreviewWidth = 240;

const int kSeekWaitMs = 1500;

bool grabAt(mpv_handle *mpv, double seconds, const QString &outputPath)
{
    if (!mpv) {
        return false;
    }

    const QByteArray target = QByteArray::number(seconds, 'f', 3);
    const char *seekArgs[] = {"seek", target.constData(),
                              "absolute+keyframes", nullptr};
    if (mpv_command(mpv, seekArgs) < 0) {
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    bool ready = false;

    while (timer.elapsed() < kSeekWaitMs) {
        mpv_event *event = mpv_wait_event(mpv, 0.05);
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

    if (!ready) {
        return false;
    }

    QFile::remove(outputPath);

    const QByteArray outUtf8 = QDir::toNativeSeparators(outputPath).toUtf8();
    const char *shotArgs[] = {"screenshot-to-file", outUtf8.constData(),
                              "video", nullptr};
    if (mpv_command(mpv, shotArgs) < 0) {
        return false;
    }

    return QFileInfo::exists(outputPath);
}

}

SeekPreviewService::SeekPreviewService(QObject *parent)
    : QObject(parent)
{
    m_outputDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                  + QStringLiteral("/seek-preview");

    if (!QDir().mkpath(m_outputDir)) {
        MM_LOG_W() << "could not create the seek preview cache at" << m_outputDir;
        m_outputDir.clear();
        return;
    }

    discardFrames();
}

SeekPreviewService::~SeekPreviewService()
{
    if (m_watcher) {
        m_watcher->waitForFinished();
    }
    teardown();
    discardFrames();
}

bool SeekPreviewService::available() const
{
    return !m_frameUrl.isEmpty();
}

QString SeekPreviewService::frameUrl() const
{
    return m_frameUrl;
}

double SeekPreviewService::framePosition() const
{
    return m_framePosition;
}

void SeekPreviewService::openFile(const QString &path)
{
    if (m_path == path) {
        return;
    }

    close();

    if (path.startsWith(QLatin1String("fdclose://"))
        || path.startsWith(QLatin1String("fd://"))) {
        MM_LOG_I() << "no seek preview for a handed-over descriptor";
        return;
    }

    m_path = path;
}

void SeekPreviewService::close()
{
    m_path.clear();
    m_loadFailed = false;
    m_havePending = false;

    if (!m_frameUrl.isEmpty()) {
        m_frameUrl.clear();
        emit frameChanged();
        emit availableChanged();
    }

    if (!m_running) {
        teardown();
        discardFrames();
    }
}

void SeekPreviewService::discardFrames()
{
    if (m_outputDir.isEmpty()) {
        return;
    }

    QDir dir(m_outputDir);
    const QStringList frames =
        dir.entryList(QStringList{QStringLiteral("frame-*.jpg")}, QDir::Files);
    for (const QString &frame : frames) {
        dir.remove(frame);
    }
}

void SeekPreviewService::teardown()
{
    m_loadedPath.clear();

    if (!m_mpv) {
        return;
    }
    mpv_terminate_destroy(m_mpv);
    m_mpv = nullptr;
}

bool SeekPreviewService::ensureLoaded()
{
    if (m_mpv && m_loadedPath == m_path) {
        return true;
    }

    if (m_mpv) {
        teardown();
    }

    if (m_path.isEmpty() || m_outputDir.isEmpty() || m_loadFailed) {
        return false;
    }

    mpv_handle *mpv = mpv_create();
    if (!mpv) {
        m_loadFailed = true;
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
    mpv_set_option_string(mpv, "hr-seek", "no");
    const QByteArray scaleFilter =
        QStringLiteral("scale=%1:-2").arg(kPreviewWidth).toUtf8();
    mpv_set_option_string(mpv, "vf", scaleFilter.constData());
    mpv_set_option_string(mpv, "screenshot-format", "jpg");
    mpv_set_option_string(mpv, "screenshot-jpeg-quality", "75");

    if (mpv_initialize(mpv) < 0) {
        mpv_terminate_destroy(mpv);
        m_loadFailed = true;
        return false;
    }

    const QByteArray pathUtf8 = m_path.toUtf8();
    const char *loadArgs[] = {"loadfile", pathUtf8.constData(), nullptr};
    if (mpv_command(mpv, loadArgs) < 0) {
        mpv_terminate_destroy(mpv);
        m_loadFailed = true;
        return false;
    }

    m_mpv = mpv;
    m_loadedPath = m_path;
    MM_LOG_I() << "seek preview instance opened for" << m_path;
    return true;
}

void SeekPreviewService::requestFrame(double seconds)
{
    if (m_path.isEmpty() || m_loadFailed || seconds < 0.0) {
        return;
    }

    m_pendingSeconds = seconds;
    m_havePending = true;
    startNext();
}

void SeekPreviewService::startNext()
{
    if (m_running || !m_havePending) {
        return;
    }
    if (!ensureLoaded()) {
        m_havePending = false;
        return;
    }

    m_havePending = false;
    m_inFlightSeconds = m_pendingSeconds;
    m_running = true;

    m_slot = m_slot == 0 ? 1 : 0;
    const QString output = m_outputDir + QStringLiteral("/frame-%1.jpg").arg(m_slot);

    if (!m_watcher) {
        m_watcher = new QFutureWatcher<bool>(this);
        connect(m_watcher, &QFutureWatcher<bool>::finished, this, [this]() {
            onGrabbed(m_watcher->result());
        });
    }

    mpv_handle *mpv = m_mpv;
    const double at = m_inFlightSeconds;
    m_watcher->setFuture(QtConcurrent::run([mpv, at, output]() {
        return grabAt(mpv, at, output);
    }));
}

void SeekPreviewService::onGrabbed(bool ok)
{
    m_running = false;

    const QString output =
        m_outputDir + QStringLiteral("/frame-%1.jpg").arg(m_slot);

    if (m_path.isEmpty()) {
        teardown();
        discardFrames();
        return;
    }

    if (m_loadedPath != m_path) {
        teardown();
        startNext();
        return;
    }

    if (ok) {
        m_frameUrl = QUrl::fromLocalFile(output).toString();
        m_framePosition = m_inFlightSeconds;
        emit frameChanged();
        emit availableChanged();
    }

    startNext();
}
