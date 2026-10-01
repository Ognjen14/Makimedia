#pragma once

#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class MirrorSync : public QObject
{
    Q_OBJECT

public:
    explicit MirrorSync(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~MirrorSync() override;

    void setRoot(const QString &root);
    QString root() const { return m_root; }

    void start(const QString &baseUrl, const QString &serverId, const QString &serverName);
    void cancel();
    bool running() const;

    void setPretendNewerLibrary(bool pretend) { m_pretendNewer = pretend; }
    bool pretendNewerLibrary() const { return m_pretendNewer; }

signals:
    void downloading(bool firstTime);
    void progress(qint64 received, qint64 total);
    void ready(const QString &libraryPath, bool changed);
    void failed(const QString &reason, bool newerSchema);

private:
    void onLibraryInfo();
    void onSnapshotData();
    void onSnapshotDone();
    void fail(const QString &reason, bool newerSchema = false);
    QString tempPath() const;

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QFile m_temp;
    QString m_root;
    QString m_base;
    QString m_serverId;
    QString m_serverName;
    QString m_revision;
    qint64 m_bytes = 0;
    int m_schema = 0;
    bool m_pretendNewer = false;
};
