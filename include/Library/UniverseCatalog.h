#pragma once

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace UniverseCatalog {

struct Film
{
    qint64 tmdbId = 0;
    QString title;
    QDate released;
    QString phase;
};

struct Universe
{
    QString key;
    QString name;
    QString posterPath;
    QString backdropPath;
    QString description;
    QSet<qint64> collections;
    QList<Film> storyOrder;
    QStringList titleKeywords;
    QStringList titleExcludes;
    QStringList excludeGenres;

    bool contains(qint64 filmTmdbId,
                  qint64 collectionId,
                  const QString &title = QString(),
                  const QString &originalTitle = QString(),
                  const QString &genres = QString()) const;
};

QList<Universe> parse(const QByteArray &json, QString *error = nullptr);

QStringList ownedPosters(const QList<Universe> &universes,
                         const QHash<qint64, qint64> &collectionByOwnedFilm);
QStringList ownedBackdrops(const QList<Universe> &universes,
                           const QHash<qint64, qint64> &collectionByOwnedFilm);
QList<Universe> load(const QString &path);

}
