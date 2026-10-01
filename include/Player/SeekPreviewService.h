#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>

struct mpv_handle;

class SeekPreviewService : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY availableChanged FINAL)
    Q_PROPERTY(QString frameUrl READ frameUrl NOTIFY frameChanged FINAL)
    Q_PROPERTY(double framePosition READ framePosition NOTIFY frameChanged FINAL)

public:
    explicit SeekPreviewService(QObject *parent = nullptr);
    ~SeekPreviewService() override;

    bool available() const;
    QString frameUrl() const;
    double framePosition() const;

    Q_INVOKABLE void openFile(const QString &path);
    Q_INVOKABLE void close();

    Q_INVOKABLE void requestFrame(double seconds);

signals:
    void availableChanged();
    void frameChanged();

private:
    bool ensureLoaded();
    void teardown();
    void discardFrames();
    void startNext();
    void onGrabbed(bool ok);

    QString m_path;
    QString m_loadedPath;
    QString m_outputDir;
    mpv_handle *m_mpv = nullptr;
    bool m_loadFailed = false;

    bool m_running = false;
    bool m_havePending = false;
    double m_pendingSeconds = 0.0;
    double m_inFlightSeconds = 0.0;

    int m_slot = 0;
    QString m_frameUrl;
    double m_framePosition = 0.0;

    QFutureWatcher<bool> *m_watcher = nullptr;
};
