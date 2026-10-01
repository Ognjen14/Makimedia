#pragma once

#include "Platform/MediaFileInfo.h"

#include <QList>
#include <QObject>
#include <QString>

class IScanner : public QObject
{
    Q_OBJECT

public:
    explicit IScanner(QObject *parent = nullptr) : QObject(parent) {}
    ~IScanner() override = default;

    virtual void scan(const QString &rootHandle) = 0;
    virtual void cancel() = 0;
    virtual bool isScanning() const = 0;

signals:
    void scanStarted(const QString &rootHandle);
    void batchReady(const QString &rootHandle, const QList<MediaFileInfo> &files);
    void scanProgress(const QString &rootHandle, int filesSeen);
    void scanFinished(const QString &rootHandle, int filesFound);
    void scanCancelled(const QString &rootHandle);
    void rootUnavailable(const QString &rootHandle);
};
