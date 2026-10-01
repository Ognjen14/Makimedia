#include "Data/SubtitleRepository.h"

#include "Data/Database.h"
#include "MmLog.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

SubtitleRepository::SubtitleRepository(Database &database)
    : m_database(database)
{
}

bool SubtitleRepository::attach(const QString &fileHandle,
                                const QString &subHandle,
                                const QString &displayName,
                                const QString &language,
                                const QString &origin)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO external_subtitles "
        "  (file_handle, sub_handle, display_name, language, added_at, origin) "
        "VALUES (:file, :sub, :name, :lang, :added, :origin) "
        "ON CONFLICT(file_handle, sub_handle) DO UPDATE SET "
        "  display_name = excluded.display_name,"
        "  language = excluded.language,"
        "  origin = excluded.origin"));
    query.bindValue(QStringLiteral(":origin"), origin);
    query.bindValue(QStringLiteral(":file"), fileHandle);
    query.bindValue(QStringLiteral(":sub"), subHandle);
    query.bindValue(QStringLiteral(":name"),
                    displayName.isNull() ? QStringLiteral("") : displayName);
    query.bindValue(QStringLiteral(":lang"),
                    language.isNull() ? QStringLiteral("") : language);
    query.bindValue(QStringLiteral(":added"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!query.exec()) {
        MM_LOG_E() << "could not attach subtitle" << subHandle << "to" << fileHandle
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "subtitle attached" << displayName << "to" << fileHandle;
    return true;
}

bool SubtitleRepository::detach(const QString &fileHandle, const QString &subHandle)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM external_subtitles "
        "WHERE file_handle = :file AND sub_handle = :sub"));
    query.bindValue(QStringLiteral(":file"), fileHandle);
    query.bindValue(QStringLiteral(":sub"), subHandle);

    if (!query.exec()) {
        MM_LOG_E() << "could not detach subtitle" << subHandle
                   << query.lastError().text();
        return false;
    }

    MM_LOG_I() << "subtitle detached" << subHandle << "from" << fileHandle;
    return true;
}

bool SubtitleRepository::detachAll(const QString &fileHandle)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "DELETE FROM external_subtitles WHERE file_handle = :file"));
    query.bindValue(QStringLiteral(":file"), fileHandle);

    if (!query.exec()) {
        MM_LOG_E() << "could not detach subtitles from" << fileHandle
                   << query.lastError().text();
        return false;
    }
    return true;
}

QList<ExternalSubtitle> SubtitleRepository::forFile(const QString &fileHandle) const
{
    QList<ExternalSubtitle> result;

    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "SELECT file_handle, sub_handle, display_name, language, origin "
        "FROM external_subtitles WHERE file_handle = :file ORDER BY id"));
    query.bindValue(QStringLiteral(":file"), fileHandle);

    if (!query.exec()) {
        MM_LOG_E() << "could not read subtitles for" << fileHandle
                   << query.lastError().text();
        return result;
    }

    while (query.next()) {
        ExternalSubtitle sub;
        sub.fileHandle = query.value(QStringLiteral("file_handle")).toString();
        sub.subHandle = query.value(QStringLiteral("sub_handle")).toString();
        sub.displayName = query.value(QStringLiteral("display_name")).toString();
        sub.language = query.value(QStringLiteral("language")).toString();
        sub.origin = query.value(QStringLiteral("origin")).toString();
        result.append(sub);
    }

    return result;
}
