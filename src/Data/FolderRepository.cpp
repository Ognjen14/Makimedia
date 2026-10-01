#include "Data/FolderRepository.h"

#include "Data/Database.h"
#include "MmLog.h"
#include "TextFold.h"

#include <algorithm>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

ScanFolder folderFromQuery(const QSqlQuery &query)
{
    ScanFolder folder;
    folder.id = query.value(QStringLiteral("id")).toLongLong();
    folder.handle = query.value(QStringLiteral("handle")).toString();
    folder.displayName = query.value(QStringLiteral("display_name")).toString();

    const QVariant scanned = query.value(QStringLiteral("last_scanned"));
    if (!scanned.isNull()) {
        folder.lastScanned = QDateTime::fromSecsSinceEpoch(scanned.toLongLong());
    }

    folder.available = query.value(QStringLiteral("available")).toInt() != 0;

    const QVariant added = query.value(QStringLiteral("added"));
    if (!added.isNull()) {
        folder.added = QDateTime::fromSecsSinceEpoch(added.toLongLong());
    }
    return folder;
}

QString withoutTrailingSeparator(const QString &handle)
{
    QString result = handle;
    while (result.size() > 1
           && (result.endsWith(QLatin1Char('/')) || result.endsWith(QLatin1Char('\\')))
           && !(result.size() == 3 && result.at(1) == QLatin1Char(':'))) {
        result.chop(1);
    }
    return result;
}

}

FolderRepository::FolderRepository(Database &database)
    : m_database(database)
{
}

qint64 FolderRepository::add(const QString &handle, const QString &displayName)
{
    const QString stored = withoutTrailingSeparator(handle);

    const QList<ScanFolder> existing = all();
    for (const ScanFolder &folder : existing) {
        if (withoutTrailingSeparator(folder.handle).compare(stored, Qt::CaseInsensitive) == 0) {
            MM_LOG_I() << "scan folder already present, reusing" << folder.handle
                       << "for" << handle;
            return folder.id;
        }
    }

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO folders (handle, display_name, available, added) "
        "VALUES (:handle, :display_name, 1, :added)"));
    query.bindValue(QStringLiteral(":handle"), stored);
    query.bindValue(QStringLiteral(":display_name"), displayName);
    query.bindValue(QStringLiteral(":added"),
                    QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

    if (!query.exec()) {
        MM_LOG_E() << "could not add scan folder" << handle
                   << query.lastError().text();
        return -1;
    }

    const qint64 id = query.lastInsertId().toLongLong();
    MM_LOG_I() << "scan folder added" << handle << "id" << id;
    return id;
}

bool FolderRepository::remove(qint64 id)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM folders WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        MM_LOG_E() << "could not remove scan folder" << id
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "scan folder removed" << id;
    return true;
}

QList<ScanFolder> FolderRepository::all() const
{
    QList<ScanFolder> folders;

    QSqlQuery query(m_database.handle());
    if (!query.exec(QStringLiteral(
            "SELECT id, handle, display_name, last_scanned, available, added "
            "FROM folders ORDER BY id"))) {
        MM_LOG_E() << "could not list scan folders" << query.lastError().text();
        return folders;
    }

    while (query.next()) {
        folders.append(folderFromQuery(query));
    }

    std::stable_sort(folders.begin(), folders.end(),
                     [](const ScanFolder &a, const ScanFolder &b) {
        return TextFold::compare(a.displayName, b.displayName) < 0;
    });
    return folders;
}

ScanFolder FolderRepository::byHandle(const QString &handle) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT id, handle, display_name, last_scanned, available, added "
        "FROM folders WHERE handle = :handle"));
    query.bindValue(QStringLiteral(":handle"), handle);

    if (!query.exec() || !query.next()) {
        return ScanFolder();
    }
    return folderFromQuery(query);
}

ScanFolder FolderRepository::byId(qint64 id) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT id, handle, display_name, last_scanned, available, added "
        "FROM folders WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec() || !query.next()) {
        return ScanFolder();
    }
    return folderFromQuery(query);
}

bool FolderRepository::setAvailable(qint64 id, bool available)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE folders SET available = :available WHERE id = :id"));
    query.bindValue(QStringLiteral(":available"), available ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        MM_LOG_E() << "could not update folder availability" << id
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "scan folder" << id
               << (available ? "marked available" : "marked unavailable");
    return true;
}

bool FolderRepository::markScanned(qint64 id, const QDateTime &when)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "UPDATE folders SET last_scanned = :when WHERE id = :id"));
    query.bindValue(QStringLiteral(":when"), when.toSecsSinceEpoch());
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        MM_LOG_E() << "could not record scan time for folder" << id
                   << query.lastError().text();
        return false;
    }
    return true;
}
