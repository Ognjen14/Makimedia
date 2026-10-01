#include <QtTest>

#include <QDateTime>
#include <QScopedPointer>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

#include "Data/Database.h"
#include "Data/FolderRepository.h"

class TestFolderRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void addingTheSameFolderTwiceKeepsOneRow();
    void aDifferentCaseOrTrailingSlashIsTheSameFolder();
    void aDriveRootKeepsItsSlash();
    void removingAFolderLeavesNothingBehind();
    void aMissIsNotAHalfFilledFolder();
    void availabilityAndScanTimeAreStored();
    void foldersAreListedByTheirFoldedName();

private:
    int rowsIn(const QString &table);
    bool exec(const QString &statement);

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<FolderRepository> m_folders;
};

void TestFolderRepository::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_database.reset(new Database);
    QVERIFY(m_database->open(m_dir->path() + QStringLiteral("/library.sqlite")));

    m_folders.reset(new FolderRepository(*m_database));
}

void TestFolderRepository::cleanup()
{
    m_folders.reset();
    m_database.reset();
    m_dir.reset();
}

int TestFolderRepository::rowsIn(const QString &table)
{
    QSqlQuery query(m_database->handle());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

bool TestFolderRepository::exec(const QString &statement)
{
    QSqlQuery query(m_database->handle());
    return query.exec(statement);
}

void TestFolderRepository::addingTheSameFolderTwiceKeepsOneRow()
{
    const qint64 first = m_folders->add(QStringLiteral("F:/Media"), QStringLiteral("Media"));
    QVERIFY(first > 0);

    QCOMPARE(m_folders->add(QStringLiteral("F:/Media"), QStringLiteral("Media")), first);
    QCOMPARE(rowsIn(QStringLiteral("folders")), 1);
}

void TestFolderRepository::aDifferentCaseOrTrailingSlashIsTheSameFolder()
{
    const qint64 first = m_folders->add(QStringLiteral("F:/Media"), QStringLiteral("Media"));

    QCOMPARE(m_folders->add(QStringLiteral("f:/media/"), QStringLiteral("media")), first);
    QCOMPARE(m_folders->add(QStringLiteral("F:/MEDIA//"), QStringLiteral("MEDIA")), first);
    QCOMPARE(rowsIn(QStringLiteral("folders")), 1);

    const qint64 films = m_folders->add(QStringLiteral("G:/Films/"), QStringLiteral("Films"));
    QVERIFY(films != first);
    QCOMPARE(m_folders->byId(films).handle, QStringLiteral("G:/Films"));
    QCOMPARE(m_folders->add(QStringLiteral("G:/Films"), QStringLiteral("Films")), films);
    QCOMPARE(rowsIn(QStringLiteral("folders")), 2);
}

void TestFolderRepository::aDriveRootKeepsItsSlash()
{
    const qint64 root = m_folders->add(QStringLiteral("F:/"), QStringLiteral("F:"));
    QVERIFY(root > 0);
    QCOMPARE(m_folders->byId(root).handle, QStringLiteral("F:/"));
    QCOMPARE(m_folders->add(QStringLiteral("f:/"), QStringLiteral("F:")), root);

    const qint64 unixRoot = m_folders->add(QStringLiteral("/"), QStringLiteral("/"));
    QVERIFY(unixRoot > 0);
    QCOMPARE(m_folders->byId(unixRoot).handle, QStringLiteral("/"));
}

void TestFolderRepository::removingAFolderLeavesNothingBehind()
{
    const qint64 folder = m_folders->add(QStringLiteral("F:/Media"), QStringLiteral("Media"));
    const qint64 other = m_folders->add(QStringLiteral("G:/Films"), QStringLiteral("Films"));

    QVERIFY(exec(QStringLiteral(
        "INSERT INTO files (id, folder_id, handle, display_name, added) VALUES"
        " (1, %1, 'F:/Media/a.mkv', 'a.mkv', 1),"
        " (2, %2, 'G:/Films/b.mkv', 'b.mkv', 1)").arg(folder).arg(other)));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds, duration_seconds,"
        " watched, last_played) VALUES (1, 60, 3600, 0, 1), (2, 60, 3600, 0, 1)")));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO media (id, tmdb_id, kind, title) VALUES (1, 1438, 'tv', 'The Wire')")));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO file_media (file_id, media_id, confidence, suggested) VALUES"
        " (1, 1, 1.0, 0), (2, 1, 1.0, 0)")));
    QVERIFY(exec(QStringLiteral(
        "INSERT INTO external_subtitles (file_handle, sub_handle, display_name, added_at)"
        " VALUES ('F:/Media/a.mkv', 'F:/Media/a.srt', 'English', 'x'),"
        "        ('G:/Films/b.mkv', 'G:/Films/b.srt', 'English', 'x')")));

    QVERIFY(m_folders->remove(folder));

    QVERIFY(!m_folders->byId(folder).isValid());
    QCOMPARE(rowsIn(QStringLiteral("folders")), 1);
    QCOMPARE(rowsIn(QStringLiteral("files")), 1);
    QCOMPARE(rowsIn(QStringLiteral("playback_state")), 1);
    QCOMPARE(rowsIn(QStringLiteral("file_media")), 1);
    QCOMPARE(rowsIn(QStringLiteral("external_subtitles")), 1);
    QCOMPARE(rowsIn(QStringLiteral("media")), 1);
}

void TestFolderRepository::aMissIsNotAHalfFilledFolder()
{
    m_folders->add(QStringLiteral("F:/Media"), QStringLiteral("Media"));

    const ScanFolder byId = m_folders->byId(12345);
    QVERIFY(!byId.isValid());
    QVERIFY(byId.handle.isEmpty());
    QVERIFY(byId.displayName.isEmpty());

    const ScanFolder byHandle = m_folders->byHandle(QStringLiteral("F:/Nowhere"));
    QVERIFY(!byHandle.isValid());
    QVERIFY(byHandle.handle.isEmpty());
}

void TestFolderRepository::availabilityAndScanTimeAreStored()
{
    const qint64 id = m_folders->add(QStringLiteral("F:/Media"), QStringLiteral("Media"));
    QVERIFY(m_folders->byId(id).available);
    QVERIFY(!m_folders->byId(id).lastScanned.isValid());

    const QDateTime when = QDateTime::fromSecsSinceEpoch(1757900000);
    QVERIFY(m_folders->setAvailable(id, false));
    QVERIFY(m_folders->markScanned(id, when));

    const ScanFolder folder = m_folders->byHandle(QStringLiteral("F:/Media"));
    QCOMPARE(folder.id, id);
    QVERIFY(!folder.available);
    QCOMPARE(folder.lastScanned, when);
}

void TestFolderRepository::foldersAreListedByTheirFoldedName()
{
    m_folders->add(QStringLiteral("F:/Zene"), QStringLiteral("Zene"));
    m_folders->add(QStringLiteral("F:/Cuvari"), QStringLiteral("Čuvari"));
    m_folders->add(QStringLiteral("F:/Crna"), QStringLiteral("crna"));

    QStringList names;
    const QList<ScanFolder> folders = m_folders->all();
    for (const ScanFolder &folder : folders) {
        names.append(folder.displayName);
    }

    QCOMPARE(names, QStringList({QStringLiteral("crna"), QStringLiteral("Čuvari"),
                                 QStringLiteral("Zene")}));
}

QTEST_GUILESS_MAIN(TestFolderRepository)

#include "tst_folderrepository.moc"
