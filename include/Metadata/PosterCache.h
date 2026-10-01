#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

class PosterCache : public QObject
{
    Q_OBJECT

public:
    explicit PosterCache(QNetworkAccessManager &network, QObject *parent = nullptr);
    PosterCache(QNetworkAccessManager &network,
                const QString &cacheDir,
                QObject *parent = nullptr);

    QString urlFor(const QString &remoteUrl, const QString &key);

    bool fetch(const QString &remoteUrl, const QString &key);

    bool isUsable() const;

    bool isCached(const QString &key) const;

    QString localUrl(const QString &key) const;

    bool store(const QString &key, const QByteArray &bytes);

    void clear();

    static bool looksLikeAnImage(const QByteArray &bytes);
    static QString defaultDirectory();

signals:
    void posterSaved(const QString &key, const QString &localUrl);
    void posterFailed(const QString &key);

private:
    QString filePathFor(const QString &key) const;
    QString usablePathFor(const QString &key) const;
    bool download(const QString &remoteUrl, const QString &key);

    QNetworkAccessManager &m_network;
    QString m_cacheDir;
    QHash<QString, bool> m_inFlight;
    int m_generation = 0;
    mutable QHash<QString, QString> m_known;
};
