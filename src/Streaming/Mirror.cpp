#include "Streaming/Mirror.h"

#include "Data/Database.h"
#include "MmLog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

const QString kInfoFile = QStringLiteral("mirror.json");
const QString kLibraryFile = QStringLiteral("library.sqlite");

bool safeId(const QString &serverId)
{
    if (serverId.isEmpty() || serverId.size() > 64) {
        return false;
    }
    for (const QChar c : serverId) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

}

namespace Mirror {

QString defaultRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/servers");
}

QString directoryFor(const QString &root, const QString &serverId)
{
    if (!safeId(serverId)) {
        return QString();
    }
    return root + QLatin1Char('/') + serverId;
}

QString libraryPath(const QString &root, const QString &serverId)
{
    const QString dir = directoryFor(root, serverId);
    return dir.isEmpty() ? QString() : dir + QLatin1Char('/') + kLibraryFile;
}

Stored readInfo(const QString &root, const QString &serverId)
{
    Stored stored;
    const QString dir = directoryFor(root, serverId);
    if (dir.isEmpty()) {
        return stored;
    }

    QFile file(dir + QLatin1Char('/') + kInfoFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return stored;
    }
    const QJsonObject info = QJsonDocument::fromJson(file.readAll()).object();
    stored.serverId = serverId;
    stored.name = info.value(QStringLiteral("name")).toString();
    stored.revision = info.value(QStringLiteral("revision")).toString();
    stored.bytes = info.value(QStringLiteral("bytes")).toInteger();
    stored.schemaVersion = info.value(QStringLiteral("schema")).toInt();
    return stored;
}

bool writeInfo(const QString &root, const Stored &stored)
{
    const QString dir = directoryFor(root, stored.serverId);
    if (dir.isEmpty() || !QDir().mkpath(dir)) {
        return false;
    }

    const QJsonObject info{
        { QStringLiteral("name"), stored.name },
        { QStringLiteral("revision"), stored.revision },
        { QStringLiteral("bytes"), double(stored.bytes) },
        { QStringLiteral("schema"), stored.schemaVersion }
    };

    const QString path = dir + QLatin1Char('/') + kInfoFile;
    const QString temp = path + QStringLiteral(".new");
    QFile file(temp);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(info).toJson(QJsonDocument::Compact));
    file.close();
    QFile::remove(path);
    return QFile::rename(temp, path);
}

QList<Stored> all(const QString &root)
{
    QList<Stored> found;
    const QFileInfoList dirs = QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                        QDir::Name);
    for (const QFileInfo &dir : dirs) {
        const QString serverId = dir.fileName();
        if (!safeId(serverId) || !QFile::exists(libraryPath(root, serverId))) {
            continue;
        }
        Stored stored = readInfo(root, serverId);
        stored.serverId = serverId;
        stored.bytes = QFileInfo(libraryPath(root, serverId)).size();
        found.append(stored);
    }
    return found;
}

bool forget(const QString &root, const QString &serverId)
{
    const QString dir = directoryFor(root, serverId);
    if (dir.isEmpty()) {
        return false;
    }
    const bool removed = QDir(dir).removeRecursively();
    MM_LOG_I() << "mirror: forgot the library of" << serverId << removed;
    return removed;
}

bool looksLikeSqlite(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return file.read(16) == QByteArray("SQLite format 3\0", 16);
}

void removeWithSidecars(const QString &path)
{
    QFile::remove(path);
    QFile::remove(path + QStringLiteral("-wal"));
    QFile::remove(path + QStringLiteral("-shm"));
    QFile::remove(path + QStringLiteral("-journal"));
}

bool carryPlayback(const QString &fromPath, const QString &toPath, int *carried,
                   QString *error)
{
    *carried = 0;

    Database target;
    if (!target.open(toPath)) {
        *error = QStringLiteral("the new library could not be opened");
        return false;
    }

    if (fromPath.isEmpty() || !QFile::exists(fromPath)) {
        target.close();
        return true;
    }

    bool ok = true;
    {
        QSqlQuery attach(target.handle());
        attach.prepare(QStringLiteral("ATTACH DATABASE :path AS old"));
        attach.bindValue(QStringLiteral(":path"), fromPath);
        if (!attach.exec()) {
            *error = QStringLiteral("the old library could not be read: ")
                     + attach.lastError().text();
            ok = false;
        }

        QSqlQuery copy(target.handle());
        if (ok && !copy.exec(QStringLiteral(
                "INSERT OR REPLACE INTO main.playback_state"
                " (file_id, position_seconds, duration_seconds, watched_seconds,"
                "  watched, last_played)"
                " SELECT nf.id, p.position_seconds, p.duration_seconds,"
                "  p.watched_seconds, p.watched, p.last_played"
                " FROM old.playback_state p"
                " JOIN old.files f ON f.id = p.file_id"
                " JOIN main.files nf ON nf.handle = f.handle"))) {
            *error = QStringLiteral("progress could not be carried: ")
                     + copy.lastError().text();
            ok = false;
        }
        if (ok) {
            *carried = copy.numRowsAffected();
        }
        copy.finish();
        attach.finish();

        QSqlQuery detach(target.handle());
        detach.exec(QStringLiteral("DETACH DATABASE old"));
    }

    target.close();
    if (ok) {
        MM_LOG_I() << "mirror: carried the progress of" << *carried
                   << "files into the new library";
    }
    return ok;
}

}
