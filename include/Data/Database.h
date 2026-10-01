#pragma once

#include <QSqlDatabase>
#include <QString>

class Database
{
public:
    Database();
    ~Database();

    bool open(const QString &filePath = QString());
    void close();
    bool isOpen() const;

    QSqlDatabase handle() const;
    QString filePath() const;

    int schemaVersion() const;

    bool transaction();
    bool commit();
    bool rollback();

    static QString defaultFilePath();
    static int targetSchemaVersion();

private:
    bool applyPragmas();
    bool migrate();
    bool migrateToVersion1();
    bool migrateToVersion2();
    bool migrateToVersion3();
    bool migrateToVersion4();
    bool migrateToVersion5();
    bool migrateToVersion6();
    bool migrateToVersion7();
    bool migrateToVersion8();
    bool migrateToVersion9();
    bool migrateToVersion10();
    bool migrateToVersion11();
    bool migrateToVersion12();
    bool migrateToVersion13();
    bool migrateToVersion14();
    bool migrateToVersion15();
    bool migrateToVersion16();
    bool migrateToVersion17();
    bool migrateToVersion18();
    bool migrateToVersion19();
    bool migrateToVersion20();
    bool migrateToVersion21();
    bool migrateToVersion22();
    bool foldColumn(const QString &table, const QString &textColumn,
                    const QString &keyColumn);
    bool hasColumn(const QString &table, const QString &column);
    bool setSchemaVersion(int version);
    bool execOrLog(const QString &statement);

    QString m_connectionName;
    QString m_filePath;
    bool m_open = false;
    int m_transactionDepth = 0;
};
