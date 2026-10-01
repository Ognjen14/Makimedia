#include "Platform/FilesystemScanner.h"

#include "MmLog.h"
#include "Platform/IMediaSource.h"
#include "Platform/MediaFileInfo.h"

#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QtConcurrent>

#ifdef Q_OS_ANDROID
#  include "Platform/Android/AndroidMediaSource.h"
#endif

FilesystemScanner::FilesystemScanner(IMediaSource *mediaSource, QObject *parent)
    : IScanner(parent)
    , m_mediaSource(mediaSource)
{
    connect(&m_watcher, &QFutureWatcher<QList<MediaFileInfo>>::finished,
            this, &FilesystemScanner::onScanComplete);
}

FilesystemScanner::~FilesystemScanner()
{
    cancel();
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

bool FilesystemScanner::isScanning() const
{
    return m_watcher.isRunning();
}

void FilesystemScanner::cancel()
{
    if (!m_watcher.isRunning()) {
        return;
    }
    MM_LOG_I() << "scan cancel requested for" << m_activeRoot;
    m_cancelRequested.storeRelease(1);
}

void FilesystemScanner::scan(const QString &rootHandle)
{
    if (m_watcher.isRunning()) {
        MM_LOG_W() << "scan already running for" << m_activeRoot
                   << "ignoring request for" << rootHandle;
        return;
    }

    if (m_mediaSource && !m_mediaSource->isRootAvailable(rootHandle)) {
        MM_LOG_W() << "scan skipped, root unavailable" << rootHandle;
        emit rootUnavailable(rootHandle);
        return;
    }

    m_activeRoot = rootHandle;
    m_cancelRequested.storeRelease(0);

    MM_LOG_I() << "scan started for" << rootHandle;
    emit scanStarted(rootHandle);

    QAtomicInteger<int> *cancelFlag = &m_cancelRequested;

    m_filesSeen.storeRelease(0);

    QAtomicInteger<int> *seenCounter = &m_filesSeen;

    m_watcher.setFuture(QtConcurrent::run([this, rootHandle, cancelFlag, seenCounter]() {
        QList<MediaFileInfo> pending;
        int total = 0;
        int skippedSystem = 0;
        QElapsedTimer timer;
        timer.start();
        QElapsedTimer sinceBatch;
        sinceBatch.start();

#ifdef Q_OS_ANDROID
        if (AndroidMediaSource::isMediaStoreRoot(rootHandle)) {
            const QString volume = AndroidMediaSource::mediaStoreVolume(rootHandle);
            const QList<MediaFileInfo> found =
                AndroidMediaSource::videosOnVolume(volume);

            seenCounter->storeRelease(int(found.size()));
            MM_LOG_I() << "MediaStore scan finished for" << rootHandle
                       << "found" << found.size()
                       << "in" << timer.elapsed() << "ms";
            return found;
        }
#endif

        const QStringList suffixes = MediaFormats::videoSuffixes();

        QDirIterator it(rootHandle,
                        QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

        while (it.hasNext()) {
            if (cancelFlag->loadAcquire() != 0) {
                break;
            }

            const QString path = it.next();
            const QFileInfo entry = it.fileInfo();

            if (!suffixes.contains(entry.suffix().toLower())) {
                continue;
            }

            if (MediaFormats::isInSystemFolder(path)) {
                ++skippedSystem;
                continue;
            }

            if (!MediaFormats::looksLikeVideoFile(path)) {
                continue;
            }

            MediaFileInfo info;
            info.handle = entry.absoluteFilePath();
            info.parentHandle = entry.absolutePath();
            info.displayName = entry.fileName();
            info.sizeBytes = entry.size();
            info.modified = entry.lastModified();
            info.isFolder = false;
            pending.append(info);
            ++total;
            seenCounter->storeRelease(total);

            if (pending.size() >= 200 || sinceBatch.elapsed() >= 250) {
                const QList<MediaFileInfo> batch = pending;
                const int seen = total;
                pending.clear();
                sinceBatch.restart();
                QMetaObject::invokeMethod(this, [this, rootHandle, batch, seen]() {
                    emit batchReady(rootHandle, batch);
                    emit scanProgress(rootHandle, seen);
                }, Qt::QueuedConnection);
            }
        }

        MM_LOG_I() << "scan walk finished for" << rootHandle
                   << "found" << total
                   << "in" << timer.elapsed() << "ms, skipped" << skippedSystem
                   << "in recycle bins and system folders";
        return pending;
    }));
}

void FilesystemScanner::onScanComplete()
{
    const QList<MediaFileInfo> leftover = m_watcher.result();
    const QString root = m_activeRoot;
    const int total = m_filesSeen.loadAcquire();
    m_activeRoot.clear();

    if (m_cancelRequested.loadAcquire() != 0) {
        m_cancelRequested.storeRelease(0);
        MM_LOG_I() << "scan cancelled for" << root << "after" << total << "files";
        emit scanCancelled(root);
        return;
    }

    if (!leftover.isEmpty()) {
        emit batchReady(root, leftover);
    }
    emit scanProgress(root, total);
    MM_LOG_D() << "scan finished for" << root << "with" << total << "files";
    emit scanFinished(root, total);
}
