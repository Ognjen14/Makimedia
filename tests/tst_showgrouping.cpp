#include <QtTest>

#include "Metadata/ShowGrouping.h"

namespace {

const QString kRoot = QStringLiteral("F:/Video Player Test");

QString at(const QString &relative)
{
    return kRoot + QLatin1Char('/') + relative;
}

QStringList atAll(const QStringList &relatives)
{
    QStringList handles;
    for (const QString &relative : relatives) {
        handles.append(at(relative));
    }
    return handles;
}

QStringList titlesOf(const QList<ShowGrouping::Show> &shows)
{
    QStringList titles;
    for (const ShowGrouping::Show &show : shows) {
        titles.append(show.title);
    }
    return titles;
}

const QStringList kFilms = {
    QStringLiteral("Movies/1917 2019 1080p BluRay x264.mp4"),
    QStringLiteral("Movies/2012 2009 1080p BluRay x264.mp4"),
    QStringLiteral("Movies/S.W.A.T. 2003 1080p BluRay x264.mp4"),
    QStringLiteral("Movies/Season of the Witch 2011 1080p BluRay x264.mp4"),
    QStringLiteral("Movies/sample.mkv"),
    QStringLiteral("Movies/video_01.mkv"),
    QStringLiteral("Movies/Fight Club (1999) [1080p]/"
                   "Fight.Club.10th.Anniversary.Edition.1999.1080p.BrRip.x264.mp4")
};

const QStringList kTheWire = {
    QStringLiteral("The Wire/s01/e01.mkv"),
    QStringLiteral("The Wire/S02/s02e01.mkv"),
    QStringLiteral("The Wire/Season 3/301.mkv")
};

const QStringList kVikings = {
    QStringLiteral("Vikings - Season 1 1080p WEBRip x264 AC3 MultiSubs/"
                   "E01 Rites of Passage.mp4"),
    QStringLiteral("Vikings - Season 1 1080p WEBRip x264 AC3 MultiSubs/"
                   "E02 Wrath of the Northmen.mp4"),
    QStringLiteral("Vikings - Season 2 1080p WEBRip x264 AC3 MultiSubs/"
                   "E01 Brother's War.mp4")
};

}

class TestShowGrouping : public QObject
{
    Q_OBJECT

private slots:
    void nothingToGroup();
    void groupsEverySeasonOfAShowTogether();
    void groupsSeasonFoldersThatCarryTheTitle();
    void leavesFilmsAlone();
    void readsAShowTheParserCannotRead();
    void aSelfDescribingNameBeatsAGenericFolder();
    void aBareNameDefersToItsFolder();
    void oneEntryPerShow();
    void sortsByTitle();
    void keepsAnAbsolutePath();
    void ignoresAFileWithNoFolder();
    void groupsOnThePathWhenTheHandleHasNoFolders();
    void carriesTheNameOfEveryFile();
    void namesAFileAfterItsPathWhenNoNameIsGiven();
    void saysWhereTheNameCameFromAndWhatMadeThemEpisodes();
    void saysWhenTheNameCameOffTheFilesThemselves();
    void aLoneFileWithNoSeasonIsNotAShow();
    void aFolderThatCallsItselfAFilmSetIsNotAShow();
    void severalNumberedEpisodesWithNoSeasonAreStillAShow();

    void readsABareEpisodeNumberOncePinned_data();
    void readsABareEpisodeNumberOncePinned();
};

void TestShowGrouping::nothingToGroup()
{
    QVERIFY(ShowGrouping::group(QStringList()).isEmpty());
}

void TestShowGrouping::groupsEverySeasonOfAShowTogether()
{
    const QList<ShowGrouping::Show> shows =
        ShowGrouping::group(atAll(kTheWire));

    QCOMPARE(shows.size(), 1);

    const ShowGrouping::Show &wire = shows.first();
    QCOMPARE(wire.title, QStringLiteral("The Wire"));
    QCOMPARE(wire.fileHandles.size(), 3);
    QCOMPARE(wire.seasons, QList<int>({1, 2, 3}));
    QCOMPARE(wire.folderHandles,
             QStringList({at(QStringLiteral("The Wire"))}));
}

void TestShowGrouping::groupsSeasonFoldersThatCarryTheTitle()
{
    const QList<ShowGrouping::Show> shows =
        ShowGrouping::group(atAll(kVikings));

    QCOMPARE(shows.size(), 1);

    const ShowGrouping::Show &vikings = shows.first();
    QCOMPARE(vikings.title, QStringLiteral("Vikings"));
    QCOMPARE(vikings.fileHandles.size(), 3);
    QCOMPARE(vikings.seasons, QList<int>({1, 2}));
    QCOMPARE(vikings.folderHandles.size(), 2);
}

void TestShowGrouping::leavesFilmsAlone()
{
    QVERIFY(ShowGrouping::group(atAll(kFilms)).isEmpty());
}

void TestShowGrouping::readsAShowTheParserCannotRead()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(
        atAll({QStringLiteral("Better Call Saul/Season 1/01.mkv")}));

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().title, QStringLiteral("Better Call Saul"));
    QCOMPARE(shows.first().seasons, QList<int>({1}));
}

void TestShowGrouping::aSelfDescribingNameBeatsAGenericFolder()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(atAll({
        QStringLiteral("TV Shows/Breaking.Bad.S01E01.1080p.BluRay.x265.mkv"),
        QStringLiteral("TV Shows/Breaking.Bad.S02E03.1080p.BluRay.x265.mkv"),
        QStringLiteral("TV Shows/Firefly 1x05.mkv")
    }));

    QCOMPARE(titlesOf(shows),
             QStringList({QStringLiteral("Breaking Bad"),
                          QStringLiteral("Firefly")}));
    QCOMPARE(shows.first().seasons, QList<int>({1, 2}));
}

void TestShowGrouping::aBareNameDefersToItsFolder()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(
        atAll({QStringLiteral("The Wire/Season 3/301.mkv")}));

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().title, QStringLiteral("The Wire"));
}

void TestShowGrouping::oneEntryPerShow()
{
    QStringList handles = atAll(kFilms);
    handles += atAll(kTheWire);
    handles += atAll(kVikings);
    handles += atAll({
        QStringLiteral("Peaky Blinders/S01/s01_02.mkv"),
        QStringLiteral("Peaky Blinders/S02/s02_01.mkv"),
        QStringLiteral("Chernobyl/Season 1/E01.mkv"),
        QStringLiteral("Chernobyl/Season 1/E02.mkv"),
        QStringLiteral("Better Call Saul/Season 1/01.mkv"),
        QStringLiteral("Tvrdjava S01.1080p.HDTV.H264/"
                       "Tvrdjava.EP01.1080p.HDTV.H264.[ExYuSubs].mp4")
    });

    const QList<ShowGrouping::Show> shows = ShowGrouping::group(handles);

    QCOMPARE(titlesOf(shows),
             QStringList({QStringLiteral("Better Call Saul"),
                          QStringLiteral("Chernobyl"),
                          QStringLiteral("Peaky Blinders"),
                          QStringLiteral("The Wire"),
                          QStringLiteral("Tvrdjava"),
                          QStringLiteral("Vikings")}));

    for (const ShowGrouping::Show &show : shows) {
        QVERIFY(show.isValid());
    }
}

void TestShowGrouping::sortsByTitle()
{
    QStringList handles = atAll(kVikings);
    handles += atAll(kTheWire);

    const QList<ShowGrouping::Show> shows = ShowGrouping::group(handles);

    QCOMPARE(titlesOf(shows),
             QStringList({QStringLiteral("The Wire"),
                          QStringLiteral("Vikings")}));
}

void TestShowGrouping::keepsAnAbsolutePath()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(
        {QStringLiteral("/storage/emulated/0/Media/The Wire/s01/e01.mkv")});

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().folderHandles,
             QStringList({QStringLiteral(
                 "/storage/emulated/0/Media/The Wire")}));
}

void TestShowGrouping::ignoresAFileWithNoFolder()
{
    QVERIFY(ShowGrouping::group({QStringLiteral("e01.mkv")}).isEmpty());
}

void TestShowGrouping::groupsOnThePathWhenTheHandleHasNoFolders()
{
    const QList<ShowGrouping::File> files = {
        {QStringLiteral("content://media/1234-5678/video/media/101"),
         QStringLiteral("1234-5678:TV/Vikings/Season 1/E01 Rites of Passage.mp4")},
        {QStringLiteral("content://media/1234-5678/video/media/102"),
         QStringLiteral("1234-5678:TV/Vikings/Season 2/E01 Brother's War.mp4")}
    };

    const QList<ShowGrouping::Show> shows = ShowGrouping::group(files);

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().title, QStringLiteral("Vikings"));
    QCOMPARE(shows.first().seasons, QList<int>({1, 2}));
    QCOMPARE(shows.first().fileHandles,
             QStringList({files.at(0).handle, files.at(1).handle}));
    QCOMPARE(shows.first().folderHandles,
             QStringList({QStringLiteral("1234-5678:TV/Vikings")}));
}

void TestShowGrouping::carriesTheNameOfEveryFile()
{
    const QList<ShowGrouping::File> files = {
        {QStringLiteral("content://media/external/video/media/101"),
         QStringLiteral("1234-5678:TV/Vikings/Season 1/E01 Rites of Passage.mp4"),
         QStringLiteral("E01 Rites of Passage.mp4")},
        {QStringLiteral("content://media/external/video/media/102"),
         QStringLiteral("1234-5678:TV/Vikings/Season 1/E02 Wrath of the Northmen.mp4"),
         QStringLiteral("E02 Wrath of the Northmen.mp4")}
    };

    const QList<ShowGrouping::Show> shows = ShowGrouping::group(files);

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().fileNames,
             QStringList({QStringLiteral("E01 Rites of Passage.mp4"),
                          QStringLiteral("E02 Wrath of the Northmen.mp4")}));
    QCOMPARE(shows.first().fileNames.size(), shows.first().fileHandles.size());
}

void TestShowGrouping::namesAFileAfterItsPathWhenNoNameIsGiven()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(
        {QStringLiteral("F:/Media/The Wire/Season 1/e01.mkv")});

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().fileNames, QStringList{QStringLiteral("e01.mkv")});
}

void TestShowGrouping::saysWhereTheNameCameFromAndWhatMadeThemEpisodes()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(atAll(kTheWire));

    QCOMPARE(shows.size(), 1);
    const ShowGrouping::Show &wire = shows.first();
    QVERIFY(wire.titleFromFolder);
    QCOMPARE(wire.titleFolder, QStringLiteral("The Wire"));
    QVERIFY(wire.seasonFolders);
    QVERIFY(wire.episodeMarkers);
}

void TestShowGrouping::saysWhenTheNameCameOffTheFilesThemselves()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(atAll({
        QStringLiteral("Downloads/Better.Call.Saul.S01E01.1080p.mkv"),
        QStringLiteral("Downloads/Better.Call.Saul.S01E02.1080p.mkv")
    }));

    QCOMPARE(shows.size(), 1);
    QVERIFY(!shows.first().titleFromFolder);
    QVERIFY(shows.first().titleFolder.isEmpty());
    QVERIFY(shows.first().episodeMarkers);
}

void TestShowGrouping::aLoneFileWithNoSeasonIsNotAShow()
{
    QVERIFY(ShowGrouping::group(atAll({
        QStringLiteral("Rush Hour Trilogy 1998,2001,2007 1080p BluRay HEVC "
                       "x265 5.1 BONE/Star.Wars.Episode.5.mkv")
    })).isEmpty());
}

void TestShowGrouping::aFolderThatCallsItselfAFilmSetIsNotAShow()
{
    QVERIFY(ShowGrouping::group(atAll({
        QStringLiteral("Star Wars Saga/Star.Wars.Episode.4.mkv"),
        QStringLiteral("Star Wars Saga/Star.Wars.Episode.5.mkv")
    })).isEmpty());
}

void TestShowGrouping::severalNumberedEpisodesWithNoSeasonAreStillAShow()
{
    const QList<ShowGrouping::Show> shows = ShowGrouping::group(atAll({
        QStringLiteral("Planet Earth/Episode 1.mkv"),
        QStringLiteral("Planet Earth/Episode 2.mkv")
    }));

    QCOMPARE(shows.size(), 1);
    QCOMPARE(shows.first().title, QStringLiteral("Planet Earth"));
    QVERIFY(shows.first().seasons.isEmpty());
}

void TestShowGrouping::readsABareEpisodeNumberOncePinned_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("season");
    QTest::addColumn<int>("episode");

    QTest::newRow("a bare number is the episode")
        << QStringLiteral("01.mkv") << 1 << 1;

    QTest::newRow("a bare number in a later season")
        << QStringLiteral("07.mkv") << 4 << 7;

    QTest::newRow("two digits above nine")
        << QStringLiteral("12.mkv") << 1 << 12;

    QTest::newRow("three digits led by the season")
        << QStringLiteral("301.mkv") << 3 << 1;

    QTest::newRow("three digits led by the season, later episode")
        << QStringLiteral("214.mkv") << 2 << 14;

    QTest::newRow("three digits that disagree with the season")
        << QStringLiteral("301.mkv") << 1 << 0;

    QTest::newRow("four digits is not an episode")
        << QStringLiteral("1917.mkv") << 1 << 0;

    QTest::newRow("a name that is not a number")
        << QStringLiteral("sample.mkv") << 1 << 0;

    QTest::newRow("a name that already carries its marker")
        << QStringLiteral("s01e01.mkv") << 1 << 0;

    QTest::newRow("no season means no context to read it in")
        << QStringLiteral("01.mkv") << 0 << 0;
}

void TestShowGrouping::readsABareEpisodeNumberOncePinned()
{
    QFETCH(QString, fileName);
    QFETCH(int, season);
    QFETCH(int, episode);

    QCOMPARE(ShowGrouping::episodeFromBareName(fileName, season), episode);
}

QTEST_APPLESS_MAIN(TestShowGrouping)

#include "tst_showgrouping.moc"
