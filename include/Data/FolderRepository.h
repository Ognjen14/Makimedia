#pragma once

#include "Data/ScanFolder.h"

#include <QList>
#include <QString>

class Database;

class FolderRepository
{
public:
    explicit FolderRepository(Database &database);

    qint64 add(const QString &handle, const QString &displayName);
    bool remove(qint64 id);
    QList<ScanFolder> all() const;
    ScanFolder byHandle(const QString &handle) const;
    ScanFolder byId(qint64 id) const;
    bool setAvailable(qint64 id, bool available);
    bool markScanned(qint64 id, const QDateTime &when);

private:
    Database &m_database;
};
