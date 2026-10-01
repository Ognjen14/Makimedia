#pragma once

#include <QList>
#include <QString>

namespace Mirror {

struct Stored
{
    QString serverId;
    QString name;
    QString revision;
    qint64 bytes = 0;
    int schemaVersion = 0;
};

QString defaultRoot();
QString directoryFor(const QString &root, const QString &serverId);
QString libraryPath(const QString &root, const QString &serverId);

Stored readInfo(const QString &root, const QString &serverId);
bool writeInfo(const QString &root, const Stored &stored);
QList<Stored> all(const QString &root);
bool forget(const QString &root, const QString &serverId);

bool looksLikeSqlite(const QString &path);
bool carryPlayback(const QString &fromPath, const QString &toPath, int *carried,
                   QString *error);

void removeWithSidecars(const QString &path);

}
