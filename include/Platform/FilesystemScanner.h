#pragma once

#include "Platform/IScanner.h"

#include <QAtomicInteger>
#include <QFutureWatcher>

class IMediaSource;

class FilesystemScanner : public IScanner
{
    Q_OBJECT

public:
    explicit FilesystemScanner(IMediaSource *mediaSource, QObject *parent = nullptr);
    ~FilesystemScanner() override;

    void scan(const QString &rootHandle) override;
    void cancel() override;
    bool isScanning() const override;

private:
    void onScanComplete();

    IMediaSource *m_mediaSource = nullptr;
    QString m_activeRoot;
    QAtomicInteger<int> m_filesSeen = 0;
    QAtomicInteger<int> m_cancelRequested{0};
    QFutureWatcher<QList<MediaFileInfo>> m_watcher;
};
