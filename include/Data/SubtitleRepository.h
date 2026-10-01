#pragma once

#include <QList>
#include <QString>

class Database;

struct ExternalSubtitle
{
    QString fileHandle;
    QString subHandle;
    QString displayName;
    QString language;
    QString origin;

    bool downloaded() const { return origin == QLatin1String("downloaded"); }
    bool isValid() const { return !subHandle.isEmpty(); }
};

class SubtitleRepository
{
public:
    explicit SubtitleRepository(Database &database);

    bool attach(const QString &fileHandle,
                const QString &subHandle,
                const QString &displayName,
                const QString &language,
                const QString &origin = QStringLiteral("manual"));
    bool detach(const QString &fileHandle, const QString &subHandle);
    bool detachAll(const QString &fileHandle);
    QList<ExternalSubtitle> forFile(const QString &fileHandle) const;

private:
    Database &m_database;
};
