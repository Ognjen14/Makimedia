#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

class AppSettings;
class QNetworkAccessManager;
class QNetworkReply;

class SubtitleSearch : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool available READ available CONSTANT FINAL)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged FINAL)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged FINAL)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged FINAL)
    Q_PROPERTY(int downloadsLeft READ downloadsLeft NOTIFY quotaChanged FINAL)

public:
    SubtitleSearch(AppSettings &settings, QObject *parent = nullptr);
    ~SubtitleSearch() override;

    bool available() const;
    bool signedIn() const;
    bool busy() const;
    QVariantList results() const;
    int downloadsLeft() const;

    static QString movieHash(const QString &videoPath);

    Q_INVOKABLE void signIn();
    Q_INVOKABLE void find(const QString &videoPath, qint64 tmdbId,
                          int season, int episode);
    Q_INVOKABLE void fetch(int index);
    Q_INVOKABLE void forget();

signals:
    void signedInChanged();
    void busyChanged();
    void resultsChanged();
    void quotaChanged();
    void failed(const QString &reason);
    void saved(const QString &path, const QString &displayName);

private:
    struct Candidate
    {
        qint64 fileId = 0;
        QString release;
        QString language;
        QString name;
        bool hashMatched = false;
        int downloads = 0;
    };

    void runSearch();
    void askQuota();
    void takeCandidates(const QByteArray &body);
    void requestLink(const Candidate &candidate);
    void collect(const QString &url, const Candidate &candidate);
    void store(const QByteArray &bytes, const Candidate &candidate);
    void stop(const QString &reason);
    void setBusy(bool busy);
    QNetworkReply *post(const QString &path, const QByteArray &body);
    QNetworkReply *get(const QString &url);

    AppSettings &m_settings;
    QNetworkAccessManager *m_network = nullptr;

    QString m_token;
    QString m_host;
    int m_left = -1;
    bool m_busy = false;

    QString m_videoPath;
    qint64 m_tmdbId = 0;
    int m_season = 0;
    int m_episode = 0;
    bool m_searchWhenSignedIn = false;

    QList<Candidate> m_candidates;
};
