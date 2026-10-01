#include <QtTest>

#include <QDateTime>
#include <QScopedPointer>
#include <QSqlQuery>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>

#include "Data/Database.h"
#include "Data/FileRepository.h"
#include "Library/BrowseOptions.h"

namespace {

LibraryFile fileNamed(qint64 id, const QString &name)
{
    LibraryFile file;
    file.id = id;
    file.folderId = 1;
    file.handle = QStringLiteral("F:/Media/") + name;
    file.parentHandle = QStringLiteral("F:/Media");
    file.displayName = name;
    file.sizeBytes = 100;
    return file;
}

LibraryFile played(LibraryFile file, double position, double duration,
                   bool watched, qint64 lastPlayed)
{
    file.playback.fileId = file.id;
    file.playback.positionSeconds = position;
    file.playback.durationSeconds = duration;
    file.playback.watched = watched;
    if (lastPlayed > 0) {
        file.playback.lastPlayed = QDateTime::fromSecsSinceEpoch(lastPlayed);
    }
    return file;
}

LibraryFile matched(LibraryFile file, const QString &title, bool suggested)
{
    file.matchedTitle = title;
    file.matchedKind = QStringLiteral("movie");
    file.matchSuggested = suggested;
    return file;
}

QList<LibraryFile> theCast()
{
    return {
        fileNamed(1, QStringLiteral("never-played.mkv")),
        played(fileNamed(2, QStringLiteral("half-watched.mkv")),
               1800.0, 3600.0, false, 5000),
        played(fileNamed(3, QStringLiteral("nearly-done.mkv")),
               3500.0, 3600.0, false, 6000),
        played(fileNamed(4, QStringLiteral("marked-watched.mkv")),
               0.0, 3600.0, true, 7000),
        matched(fileNamed(5, QStringLiteral("matched.mkv")),
                QStringLiteral("The Wire"), false),
        matched(fileNamed(6, QStringLiteral("guessed.mkv")),
                QStringLiteral("Treme"), true),
    };
}

QList<LibraryFile> theShelf()
{
    QList<LibraryFile> files;

    LibraryFile banana = fileNamed(1, QStringLiteral("banana.mkv"));
    banana.sizeBytes = 300;
    files.append(played(banana, 10.0, 3600.0, false, 3000));

    LibraryFile apple = fileNamed(2, QStringLiteral("Apple.mkv"));
    apple.sizeBytes = 100;
    files.append(played(apple, 10.0, 3600.0, false, 5000));

    LibraryFile cherry = fileNamed(3, QStringLiteral("cherry.mkv"));
    cherry.sizeBytes = 300;
    files.append(played(cherry, 10.0, 3600.0, false, 1000));

    LibraryFile date = fileNamed(4, QStringLiteral("date.mkv"));
    date.sizeBytes = 200;
    files.append(played(date, 10.0, 3600.0, false, 4000));

    return files;
}

QStringList namesOf(const QList<LibraryFile> &files)
{
    QStringList names;
    names.reserve(files.size());
    for (const LibraryFile &file : files) {
        names.append(file.displayName);
    }
    return names;
}

}

class TestBrowseOptions : public QObject
{
    Q_OBJECT

private slots:
    void theStoredNumbersNeverMove();

    void eachFilterKeepsItsOwn_data();
    void eachFilterKeepsItsOwn();

    void eachSortOrdersItsOwn_data();
    void eachSortOrdersItsOwn();

    void tiesAreBrokenByTheNewestFirst();
    void titleSortFollowsWhatTheTileShows();
    void aModeFromNowhereBehavesLikeTheDefault();
    void anEmptyLibrarySortsAndFiltersToNothing();
    void filteringNeverEditsWhatItKeeps();

    void finishedAndStartedReadThePlaybackState();

    void cleanup();

    void theFileBadgeAgreesWithEverything();
    void theUnmatchedBadgeAgreesWithTheUnmatchedFilter();
    void theContinueBadgeAgreesOnPlainRows();

private:
    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<FileRepository> m_files;

    void openDatabase();
    qint64 addFile(const QString &name);
    bool play(qint64 fileId, double position, double duration, bool watched,
              qint64 lastPlayed);
    bool matchTo(qint64 fileId, const QString &title, bool suggested);
};

void TestBrowseOptions::theStoredNumbersNeverMove()
{
    QCOMPARE(int(BrowseOptions::RecentlyAdded), 0);
    QCOMPARE(int(BrowseOptions::Title), 1);
    QCOMPARE(int(BrowseOptions::LastPlayed), 2);
    QCOMPARE(int(BrowseOptions::FileSize), 3);

    QCOMPARE(int(BrowseOptions::Everything), 0);
    QCOMPARE(int(BrowseOptions::Unwatched), 1);
    QCOMPARE(int(BrowseOptions::InProgress), 2);
    QCOMPARE(int(BrowseOptions::Watched), 3);
    QCOMPARE(int(BrowseOptions::Unmatched), 4);
    QCOMPARE(int(BrowseOptions::Suggested), 5);
}

void TestBrowseOptions::eachFilterKeepsItsOwn_data()
{
    QTest::addColumn<int>("filter");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("everything")
        << int(BrowseOptions::Everything)
        << QStringList({QStringLiteral("guessed.mkv"),
                        QStringLiteral("matched.mkv"),
                        QStringLiteral("marked-watched.mkv"),
                        QStringLiteral("nearly-done.mkv"),
                        QStringLiteral("half-watched.mkv"),
                        QStringLiteral("never-played.mkv")});

    QTest::newRow("unwatched")
        << int(BrowseOptions::Unwatched)
        << QStringList({QStringLiteral("guessed.mkv"),
                        QStringLiteral("matched.mkv"),
                        QStringLiteral("never-played.mkv")});

    QTest::newRow("in progress")
        << int(BrowseOptions::InProgress)
        << QStringList({QStringLiteral("half-watched.mkv")});

    QTest::newRow("watched")
        << int(BrowseOptions::Watched)
        << QStringList({QStringLiteral("marked-watched.mkv"),
                        QStringLiteral("nearly-done.mkv")});

    QTest::newRow("unmatched")
        << int(BrowseOptions::Unmatched)
        << QStringList({QStringLiteral("marked-watched.mkv"),
                        QStringLiteral("nearly-done.mkv"),
                        QStringLiteral("half-watched.mkv"),
                        QStringLiteral("never-played.mkv")});

    QTest::newRow("suggested")
        << int(BrowseOptions::Suggested)
        << QStringList({QStringLiteral("guessed.mkv")});
}

void TestBrowseOptions::eachFilterKeepsItsOwn()
{
    QFETCH(int, filter);
    QFETCH(QStringList, expected);

    const QList<LibraryFile> kept =
        BrowseOptions::apply(theCast(), BrowseOptions::RecentlyAdded, filter);

    QCOMPARE(namesOf(kept), expected);

    for (const LibraryFile &file : theCast()) {
        QCOMPARE(BrowseOptions::keeps(filter, file),
                 expected.contains(file.displayName));
    }
}

void TestBrowseOptions::eachSortOrdersItsOwn_data()
{
    QTest::addColumn<int>("sort");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("recently added")
        << int(BrowseOptions::RecentlyAdded)
        << QStringList({QStringLiteral("date.mkv"),
                        QStringLiteral("cherry.mkv"),
                        QStringLiteral("Apple.mkv"),
                        QStringLiteral("banana.mkv")});

    QTest::newRow("title")
        << int(BrowseOptions::Title)
        << QStringList({QStringLiteral("Apple.mkv"),
                        QStringLiteral("banana.mkv"),
                        QStringLiteral("cherry.mkv"),
                        QStringLiteral("date.mkv")});

    QTest::newRow("last played")
        << int(BrowseOptions::LastPlayed)
        << QStringList({QStringLiteral("Apple.mkv"),
                        QStringLiteral("date.mkv"),
                        QStringLiteral("banana.mkv"),
                        QStringLiteral("cherry.mkv")});

    QTest::newRow("file size")
        << int(BrowseOptions::FileSize)
        << QStringList({QStringLiteral("cherry.mkv"),
                        QStringLiteral("banana.mkv"),
                        QStringLiteral("date.mkv"),
                        QStringLiteral("Apple.mkv")});
}

void TestBrowseOptions::eachSortOrdersItsOwn()
{
    QFETCH(int, sort);
    QFETCH(QStringList, expected);

    QCOMPARE(namesOf(BrowseOptions::apply(theShelf(), sort,
                                          BrowseOptions::Everything)),
             expected);
}

void TestBrowseOptions::tiesAreBrokenByTheNewestFirst()
{
    QList<LibraryFile> twins;

    LibraryFile older = fileNamed(1, QStringLiteral("same.mkv"));
    older.sizeBytes = 500;
    twins.append(played(older, 10.0, 3600.0, false, 4000));

    LibraryFile newer = fileNamed(2, QStringLiteral("same.mkv"));
    newer.sizeBytes = 500;
    twins.append(played(newer, 10.0, 3600.0, false, 4000));

    for (const int sort : {int(BrowseOptions::RecentlyAdded),
                           int(BrowseOptions::Title),
                           int(BrowseOptions::LastPlayed),
                           int(BrowseOptions::FileSize)}) {
        const QList<LibraryFile> sorted =
            BrowseOptions::apply(twins, sort, BrowseOptions::Everything);
        QCOMPARE(sorted.size(), 2);
        QCOMPARE(sorted.first().id, Q_INT64_C(2));
        QCOMPARE(sorted.last().id, Q_INT64_C(1));

        const QList<LibraryFile> reversed =
            BrowseOptions::apply({twins.last(), twins.first()}, sort,
                                 BrowseOptions::Everything);
        QCOMPARE(reversed.first().id, Q_INT64_C(2));

        QVERIFY(BrowseOptions::isBefore(sort, twins.last(), twins.first()));
        QVERIFY(!BrowseOptions::isBefore(sort, twins.first(), twins.last()));
    }
}

void TestBrowseOptions::titleSortFollowsWhatTheTileShows()
{
    const QList<LibraryFile> files = {
        matched(fileNamed(1, QStringLiteral("aaa.mkv")), QStringLiteral("Zodiac"), false),
        matched(fileNamed(2, QStringLiteral("zzz.mkv")), QStringLiteral("Alien"), false),
        fileNamed(3, QStringLiteral("Čuvari.mkv")),
        fileNamed(4, QStringLiteral("banana.mkv"))
    };

    const QStringList expected = {
        QStringLiteral("zzz.mkv"),
        QStringLiteral("banana.mkv"),
        QStringLiteral("Čuvari.mkv"),
        QStringLiteral("aaa.mkv")
    };

    QCOMPARE(namesOf(BrowseOptions::apply(files, BrowseOptions::Title,
                                          BrowseOptions::Everything)),
             expected);

    QVERIFY(BrowseOptions::isBefore(BrowseOptions::Title, files.at(1), files.at(0)));
    QVERIFY(BrowseOptions::isBefore(BrowseOptions::Title, files.at(3), files.at(2)));
    QVERIFY(BrowseOptions::isBefore(BrowseOptions::Title, files.at(2), files.at(0)));
}

void TestBrowseOptions::aModeFromNowhereBehavesLikeTheDefault()
{
    const QStringList byDefault =
        namesOf(BrowseOptions::apply(theShelf(), BrowseOptions::RecentlyAdded,
                                     BrowseOptions::Everything));

    QCOMPARE(namesOf(BrowseOptions::apply(theShelf(), 99, 99)), byDefault);
    QCOMPARE(namesOf(BrowseOptions::apply(theShelf(), -1, -1)), byDefault);

    for (const LibraryFile &file : theShelf()) {
        QVERIFY(BrowseOptions::keeps(99, file));
    }
}

void TestBrowseOptions::anEmptyLibrarySortsAndFiltersToNothing()
{
    for (int sort = 0; sort <= int(BrowseOptions::FileSize); ++sort) {
        for (int filter = 0; filter <= int(BrowseOptions::Suggested); ++filter) {
            QVERIFY(BrowseOptions::apply(QList<LibraryFile>(), sort, filter)
                        .isEmpty());
        }
    }
}

void TestBrowseOptions::filteringNeverEditsWhatItKeeps()
{
    const QList<LibraryFile> before = theCast();
    const QList<LibraryFile> kept =
        BrowseOptions::apply(before, BrowseOptions::Title,
                             BrowseOptions::Everything);

    QCOMPARE(kept.size(), before.size());

    for (const LibraryFile &file : kept) {
        bool found = false;
        for (const LibraryFile &original : before) {
            if (original.id == file.id) {
                QVERIFY(original == file);
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }
}

void TestBrowseOptions::finishedAndStartedReadThePlaybackState()
{
    const LibraryFile fresh = fileNamed(1, QStringLiteral("a.mkv"));
    QVERIFY(!BrowseOptions::isFinished(fresh));
    QVERIFY(!BrowseOptions::isStarted(fresh));

    const LibraryFile barelyBegun =
        played(fileNamed(2, QStringLiteral("b.mkv")), 1.0, 3600.0, false, 1000);
    QVERIFY(!BrowseOptions::isFinished(barelyBegun));
    QVERIFY(!BrowseOptions::isStarted(barelyBegun));

    const LibraryFile begun =
        played(fileNamed(7, QStringLiteral("g.mkv")), 31.0, 3600.0, false, 1000);
    QVERIFY(!BrowseOptions::isFinished(begun));
    QVERIFY(BrowseOptions::isStarted(begun));

    const LibraryFile atThreshold =
        played(fileNamed(3, QStringLiteral("c.mkv")), 3420.0, 3600.0, false, 1000);
    QVERIFY(BrowseOptions::isFinished(atThreshold));

    const LibraryFile justUnder =
        played(fileNamed(4, QStringLiteral("d.mkv")), 3419.0, 3600.0, false, 1000);
    QVERIFY(!BrowseOptions::isFinished(justUnder));

    const LibraryFile noDuration =
        played(fileNamed(5, QStringLiteral("e.mkv")), 500.0, 0.0, false, 1000);
    QVERIFY(!BrowseOptions::isFinished(noDuration));
    QVERIFY(BrowseOptions::isStarted(noDuration));

    const LibraryFile flagged =
        played(fileNamed(6, QStringLiteral("f.mkv")), 0.0, 3600.0, true, 1000);
    QVERIFY(BrowseOptions::isFinished(flagged));
    QVERIFY(!BrowseOptions::isStarted(flagged));
}

void TestBrowseOptions::openDatabase()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_database.reset(new Database);
    QVERIFY(m_database->open(m_dir->path() + QStringLiteral("/library.sqlite")));

    QSqlQuery folder(m_database->handle());
    QVERIFY(folder.exec(QStringLiteral(
        "INSERT INTO folders (id, handle, display_name, added)"
        " VALUES (1, 'F:/Media', 'Media', 1)")));

    m_files.reset(new FileRepository(*m_database));
}

qint64 TestBrowseOptions::addFile(const QString &name)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, parent_handle, display_name,"
        "                   size_bytes, added)"
        " VALUES (1, :handle, 'F:/Media', :name, 100, 1)"));
    query.bindValue(QStringLiteral(":handle"),
                    QStringLiteral("F:/Media/") + name);
    query.bindValue(QStringLiteral(":name"), name);

    if (!query.exec()) {
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool TestBrowseOptions::play(qint64 fileId, double position, double duration,
                             bool watched, qint64 lastPlayed)
{
    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "INSERT INTO playback_state (file_id, position_seconds,"
        "                            duration_seconds, watched, last_played)"
        " VALUES (:file, :position, :duration, :watched, :played)"));
    query.bindValue(QStringLiteral(":file"), fileId);
    query.bindValue(QStringLiteral(":position"), position);
    query.bindValue(QStringLiteral(":duration"), duration);
    query.bindValue(QStringLiteral(":watched"), watched ? 1 : 0);
    query.bindValue(QStringLiteral(":played"),
                    lastPlayed > 0 ? QVariant(lastPlayed) : QVariant());
    return query.exec();
}

bool TestBrowseOptions::matchTo(qint64 fileId, const QString &title,
                                bool suggested)
{
    static int nextTmdbId = 500;

    QSqlQuery media(m_database->handle());
    media.prepare(QStringLiteral(
        "INSERT INTO media (tmdb_id, kind, title) VALUES (:tmdb, 'movie', :title)"));
    media.bindValue(QStringLiteral(":tmdb"), ++nextTmdbId);
    media.bindValue(QStringLiteral(":title"), title);
    if (!media.exec()) {
        return false;
    }

    QSqlQuery link(m_database->handle());
    link.prepare(QStringLiteral(
        "INSERT INTO file_media (file_id, media_id, confidence, suggested)"
        " VALUES (:file, :media, 1.0, :suggested)"));
    link.bindValue(QStringLiteral(":file"), fileId);
    link.bindValue(QStringLiteral(":media"), media.lastInsertId().toLongLong());
    link.bindValue(QStringLiteral(":suggested"), suggested ? 1 : 0);
    return link.exec();
}

void TestBrowseOptions::cleanup()
{
    m_files.reset();
    m_database.reset();
    m_dir.reset();
}

void TestBrowseOptions::theFileBadgeAgreesWithEverything()
{
    openDatabase();

    for (int i = 1; i <= 5; ++i) {
        QVERIFY(addFile(QStringLiteral("f%1.mkv").arg(i)) > 0);
    }

    QSqlQuery gone(m_database->handle());
    QVERIFY(gone.exec(QStringLiteral(
        "UPDATE files SET missing = 1 WHERE display_name = 'f5.mkv'")));

    const QList<LibraryFile> shown =
        BrowseOptions::apply(m_files->all(), BrowseOptions::RecentlyAdded,
                             BrowseOptions::Everything);

    QCOMPARE(shown.size(), 4);
    QCOMPARE(m_files->count(), 4);
}

void TestBrowseOptions::theUnmatchedBadgeAgreesWithTheUnmatchedFilter()
{
    openDatabase();

    const qint64 a = addFile(QStringLiteral("a.mkv"));
    const qint64 b = addFile(QStringLiteral("b.mkv"));
    QVERIFY(addFile(QStringLiteral("c.mkv")) > 0);

    QVERIFY(matchTo(a, QStringLiteral("The Wire"), false));
    QVERIFY(matchTo(b, QStringLiteral("Treme"), true));

    const QList<LibraryFile> unmatched =
        BrowseOptions::apply(m_files->all(), BrowseOptions::RecentlyAdded,
                             BrowseOptions::Unmatched);

    QCOMPARE(unmatched.size(), 1);
    QCOMPARE(m_files->unmatchedCount(), 1);
    QCOMPARE(m_files->unmatched().size(), unmatched.size());

    const QList<LibraryFile> suggested =
        BrowseOptions::apply(m_files->all(), BrowseOptions::RecentlyAdded,
                             BrowseOptions::Suggested);
    QCOMPARE(namesOf(suggested), QStringList({QStringLiteral("b.mkv")}));
}

void TestBrowseOptions::theContinueBadgeAgreesOnPlainRows()
{
    openDatabase();

    const qint64 begun = addFile(QStringLiteral("begun.mkv"));
    const qint64 alsoBegun = addFile(QStringLiteral("also-begun.mkv"));
    const qint64 done = addFile(QStringLiteral("done.mkv"));
    QVERIFY(addFile(QStringLiteral("untouched.mkv")) > 0);

    QVERIFY(play(begun, 600.0, 3600.0, false, 5000));
    QVERIFY(play(alsoBegun, 1200.0, 3600.0, false, 6000));
    QVERIFY(play(done, 3600.0, 3600.0, true, 7000));

    const QList<LibraryFile> inProgress =
        BrowseOptions::apply(m_files->all(), BrowseOptions::RecentlyAdded,
                             BrowseOptions::InProgress);

    QCOMPARE(namesOf(inProgress),
             QStringList({QStringLiteral("also-begun.mkv"),
                          QStringLiteral("begun.mkv")}));
    QCOMPARE(m_files->continueWatchingCount(), 2);
    QCOMPARE(m_files->continueWatching(10).size(), inProgress.size());
}

QTEST_GUILESS_MAIN(TestBrowseOptions)

#include "tst_browseoptions.moc"
