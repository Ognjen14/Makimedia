#include <QtTest>

#include "Data/FileRepository.h"
#include "Library/ListText.h"

namespace {

LibraryFile played(double fileLength, double playedLength, double position, bool watched)
{
    LibraryFile file;
    file.id = 1;
    file.durationSeconds = fileLength;
    file.playback.fileId = 1;
    file.playback.durationSeconds = playedLength;
    file.playback.positionSeconds = position;
    file.playback.watched = watched;
    return file;
}

}

class TestListText : public QObject
{
    Q_OBJECT

private slots:
    void durationReadsLikeAListing_data();
    void durationReadsLikeAListing();

    void timeLeftTrustsTheProbedLength();
    void aWatchedFileHasNoTimeLeft();

    void locationShowsTheLastTwoFolders_data();
    void locationShowsTheLastTwoFolders();

    void aFileNameReadsLikeATitle_data();
    void aFileNameReadsLikeATitle();
};

void TestListText::durationReadsLikeAListing_data()
{
    QTest::addColumn<double>("seconds");
    QTest::addColumn<QString>("expected");

    QTest::newRow("unknown") << 0.0 << QString();
    QTest::newRow("negative") << -5.0 << QString();
    QTest::newRow("under a minute") << 45.0 << QStringLiteral("<1m");
    QTest::newRow("just under a minute") << 59.9 << QStringLiteral("<1m");
    QTest::newRow("one minute") << 60.0 << QStringLiteral("1m");
    QTest::newRow("just under an hour") << 3599.0 << QStringLiteral("59m");
    QTest::newRow("one hour") << 3600.0 << QStringLiteral("1h 0m");
    QTest::newRow("an hour and a half") << 5400.0 << QStringLiteral("1h 30m");
}

void TestListText::durationReadsLikeAListing()
{
    QFETCH(double, seconds);
    QFETCH(QString, expected);

    QCOMPARE(ListText::duration(seconds), expected);
}

void TestListText::timeLeftTrustsTheProbedLength()
{
    QCOMPARE(ListText::remaining(played(3000.0, 3600.0, 600.0, false)),
             QStringLiteral("40:00"));
    QCOMPARE(ListText::remaining(played(0.0, 3600.0, 600.0, false)),
             QStringLiteral("50:00"));
    QCOMPARE(ListText::remaining(played(7200.0, 0.0, 600.0, false)),
             QStringLiteral("1:50:00"));
    QCOMPARE(ListText::remaining(played(3000.0, 3600.0, 3200.0, false)), QString());
    QCOMPARE(ListText::remaining(played(0.0, 0.0, 600.0, false)), QString());
}

void TestListText::aWatchedFileHasNoTimeLeft()
{
    QCOMPARE(ListText::remaining(played(3600.0, 3600.0, 0.0, true)), QString());
    QCOMPARE(ListText::remaining(played(3600.0, 3600.0, 600.0, true)), QString());
}

void TestListText::locationShowsTheLastTwoFolders_data()
{
    QTest::addColumn<QString>("parent");
    QTest::addColumn<QString>("expected");

    QTest::newRow("a windows path") << QStringLiteral("F:/Media/Shows")
                                    << QStringLiteral("Media / Shows");
    QTest::newRow("backslashes") << QStringLiteral("F:\\Media\\Shows\\S01")
                                 << QStringLiteral("Shows / S01");
    QTest::newRow("a drive on its own") << QStringLiteral("F:") << QStringLiteral("F:");
    QTest::newRow("one folder") << QStringLiteral("Media") << QStringLiteral("Media");
    QTest::newRow("android internal storage")
        << QStringLiteral("/storage/emulated/0/Movies") << QStringLiteral("0 / Movies");
    QTest::newRow("android volume and relative path")
        << QStringLiteral("primary:Movies/The Wire") << QStringLiteral("Movies / The Wire");
    QTest::newRow("a removable volume")
        << QStringLiteral("1234-5678:TV") << QStringLiteral("TV");
    QTest::newRow("a volume root") << QStringLiteral("primary:") << QString();
    QTest::newRow("nothing") << QString() << QString();
}

void TestListText::locationShowsTheLastTwoFolders()
{
    QFETCH(QString, parent);
    QFETCH(QString, expected);

    QCOMPARE(ListText::location(parent), expected);
}

void TestListText::aFileNameReadsLikeATitle_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("title");

    QTest::newRow("nothing") << "" << "";
    QTest::newRow("release name")
        << "The.Dark.Knight.2008.1080p.BluRay.x264.mkv" << "The Dark Knight (2008)";
    QTest::newRow("bracketed group")
        << "Rocky (1976) [YTS].mp4" << "Rocky (1976)";
    QTest::newRow("underscores and a path")
        << "F:/Media/Some_Home_Video.mp4" << "Some Home Video";
    QTest::newRow("backslashed path")
        << "D:\\Films\\Heat.1995.REMUX.mkv" << "Heat (1995)";
    QTest::newRow("dangling dash")
        << "Movie - 720p.mkv" << "Movie";
    QTest::newRow("only junk keeps the name")
        << "1080p.mkv" << "1080p";
    QTest::newRow("a year alone stays a word")
        << "2012.mkv" << "2012";
    QTest::newRow("episode code is kept")
        << "Severance.S02E03.2160p.ATVP.WEB-DL.mkv" << "Severance S02E03";
    QTest::newRow("accented name")
        << "Žene.S01E01.mkv" << "Žene S01E01";
}

void TestListText::aFileNameReadsLikeATitle()
{
    QFETCH(QString, fileName);
    QFETCH(QString, title);

    QCOMPARE(ListText::prettyTitle(fileName), title);
}

QTEST_APPLESS_MAIN(TestListText)

#include "tst_listtext.moc"
