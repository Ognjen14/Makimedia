#include <QtTest>

#include <QFileInfo>
#include <QTemporaryDir>

#include "Metadata/FileNameParser.h"

class TestFileNameParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void readsAnEpisodeFromTheName_data();
    void readsAnEpisodeFromTheName();

    void readsAFilmFromTheName_data();
    void readsAFilmFromTheName();

    void splitsAnAlsoKnownAsName();
    void anEpisodeNumberWithNoSeasonStaysInTheTitle();
    void aYearBeforeAnEpisodeMarkerLeavesTheWholeNameAsTheOtherReading();

    void listsTheFoldersAboveAFile_data();
    void listsTheFoldersAboveAFile();
    void readsAShowFromAnAndroidVolume();

    void readsTheFolderWhenTheNameIsBare_data();
    void readsTheFolderWhenTheNameIsBare();

    void knownGaps_data();
    void knownGaps();

    void tellsJunkFromTitleWords_data();
    void tellsJunkFromTitleWords();

private:
    QString pathFor(const QString &relative) const;

    QTemporaryDir m_root;
};

void TestFileNameParser::initTestCase()
{
    QVERIFY2(m_root.isValid(), qPrintable(m_root.errorString()));
    QVERIFY(!QFileInfo::exists(pathFor(QString())));
}

QString TestFileNameParser::pathFor(const QString &relative) const
{
    return m_root.path() + QStringLiteral("/unplugged drive/") + relative;
}

void TestFileNameParser::readsAnEpisodeFromTheName_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("year");
    QTest::addColumn<int>("season");
    QTest::addColumn<int>("episode");

    QTest::newRow("dotted name, S01E01")
        << QStringLiteral("Breaking.Bad.S01E01.1080p.BluRay.x265-RARBG.mkv")
        << QStringLiteral("Breaking Bad") << 0 << 1 << 1;

    QTest::newRow("dotted name, S02E03")
        << QStringLiteral("Breaking.Bad.S02E03.1080p.BluRay.x265-RARBG.mkv")
        << QStringLiteral("Breaking Bad") << 0 << 2 << 3;

    QTest::newRow("specials, S00E01")
        << QStringLiteral("Only Fools and Horses (1981) - S00E01 - Episode 1 "
                          "(1080p BluRay x265 Ghost).mkv")
        << QStringLiteral("Only Fools and Horses") << 1981 << 0 << 1;

    QTest::newRow("specials, 0x05")
        << QStringLiteral("Doctor.Who.0x05.mkv")
        << QStringLiteral("Doctor Who") << 0 << 0 << 5;

    QTest::newRow("double episode in one file")
        << QStringLiteral("Drzavni.sluzbenik.S02E01E02.HDTV.1080p.x264."
                          "[ExYu-Subs].mp4")
        << QStringLiteral("Drzavni sluzbenik") << 0 << 2 << 1;

    QTest::newRow("double episode written with a dash")
        << QStringLiteral("Drzavni.sluzbenik.S02E03-E04.HDTV.1080p.mp4")
        << QStringLiteral("Drzavni sluzbenik") << 0 << 2 << 3;

    QTest::newRow("a year inside the show's name")
        << QStringLiteral("Reply 1988 - S01E01.mp4")
        << QStringLiteral("Reply") << 1988 << 1 << 1;

    QTest::newRow("cross pair 1x05")
        << QStringLiteral("Firefly 1x05.mkv")
        << QStringLiteral("Firefly") << 0 << 1 << 5;

    QTest::newRow("spelled out season and episode")
        << QStringLiteral("The Sopranos Season 1 Episode 4.mkv")
        << QStringLiteral("The Sopranos") << 0 << 1 << 4;

    QTest::newRow("year in brackets before the marker")
        << QStringLiteral("MINDHUNTER (2017) - S01E01 - Episode 1 "
                          "(1080p NF WEB-DL x265).mkv")
        << QStringLiteral("MINDHUNTER") << 2017 << 1 << 1;

    QTest::newRow("episode title in brackets after the marker")
        << QStringLiteral("Marvels The Punisher S01E01 3 AM "
                          "(1080p x265 Joy).mkv")
        << QStringLiteral("Marvels The Punisher") << 0 << 1 << 1;

    QTest::newRow("non english title")
        << QStringLiteral("Senke.nad.Balkanom.S01E01.1080p.WEBRip.x264.mp4")
        << QStringLiteral("Senke nad Balkanom") << 0 << 1 << 1;

    QTest::newRow("apostrophe dropped by the release name")
        << QStringLiteral("The.Queens.Gambit.S01E01.1080p.WEB.H264-GGWP.mkv")
        << QStringLiteral("The Queens Gambit") << 0 << 1 << 1;
}

void TestFileNameParser::readsAnEpisodeFromTheName()
{
    QFETCH(QString, fileName);
    QFETCH(QString, title);
    QFETCH(int, year);
    QFETCH(int, season);
    QFETCH(int, episode);

    const ParsedFileName parsed = FileNameParser::parse(fileName);

    QCOMPARE(parsed.title, title);
    QCOMPARE(parsed.year, year);
    QCOMPARE(parsed.season, season);
    QCOMPARE(parsed.episode, episode);
    QVERIFY(parsed.looksLikeEpisode());
}

void TestFileNameParser::readsAFilmFromTheName_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("year");

    QTest::newRow("dotted name with a year")
        << QStringLiteral("A.Complete.Unknown.2024.1080p.WEBRip.x264.AAC5.1.mp4")
        << QStringLiteral("A Complete Unknown") << 2024;

    QTest::newRow("edition phrase between title and year")
        << QStringLiteral("Fight.Club.10th.Anniversary.Edition.1999.1080p."
                          "BrRip.x264.mp4")
        << QStringLiteral("Fight Club") << 1999;

    QTest::newRow("a tag word standing alone inside the title")
        << QStringLiteral("Mad Max 1979 1080p BluRay x264.mkv")
        << QStringLiteral("Mad Max") << 1979;

    QTest::newRow("a tag word before the year, in brackets")
        << QStringLiteral("Mad Max (1979) [1080p] [x265].mkv")
        << QStringLiteral("Mad Max") << 1979;

    QTest::newRow("a tag word beside other tags, with no year to help")
        << QStringLiteral("Some Film WEB 1080p x264.mkv")
        << QStringLiteral("Some Film") << 0;

    QTest::newRow("a tag word alone, with no year to help")
        << QStringLiteral("Mad Max.mkv") << QStringLiteral("Mad Max") << 0;

    QTest::newRow("a junk word that is part of the title, then the year")
        << QStringLiteral("A.Complete.Unknown.2024.1080p.WEBRip.x264.AAC5.1.mp4")
        << QStringLiteral("A Complete Unknown") << 2024;

    QTest::newRow("a year inside the film's name")
        << QStringLiteral("The.Legend.of.1900.1998.2160p.UHD.BluRay.x265.mkv")
        << QStringLiteral("The Legend of 1900") << 1998;

    QTest::newRow("remaster tag after the year")
        << QStringLiteral("Forrest Gump 1994 REMASTERED 1080p BluRay x265.mkv")
        << QStringLiteral("Forrest Gump") << 1994;

    QTest::newRow("no year at all")
        << QStringLiteral("Captain.America.The.First.Avenger.1080p."
                          "BrRip.x264.mp4")
        << QStringLiteral("Captain America The First Avenger") << 0;

    QTest::newRow("apostrophe kept in the title")
        << QStringLiteral("Ocean's Eleven 2001 1080p BluRay x264.mp4")
        << QStringLiteral("Ocean's Eleven") << 2001;

    QTest::newRow("apostrophe and no year")
        << QStringLiteral("One.Flew.Over.The.Cuckoo's.Nest.1080p.BrRip.x264.mp4")
        << QStringLiteral("One Flew Over The Cuckoo's Nest") << 0;

    QTest::newRow("series name in front of the title")
        << QStringLiteral("Star.Wars.Return.of.the.Jedi.1983.REMASTERED."
                          "1080p.x265.mkv")
        << QStringLiteral("Star Wars Return of the Jedi") << 1983;

    QTest::newRow("sequel number")
        << QStringLiteral("The.Godfather.Part.2.1974.1080p.BrRip.x264.mp4")
        << QStringLiteral("The Godfather Part 2") << 1974;

    QTest::newRow("initials must not read as a season")
        << QStringLiteral("S.W.A.T. 2003 1080p BluRay x264.mp4")
        << QStringLiteral("S W A T") << 2003;

    QTest::newRow("a bare Season must not cut the title")
        << QStringLiteral("Season of the Witch 2011 1080p BluRay x264.mp4")
        << QStringLiteral("Season of the Witch") << 2011;

    QTest::newRow("digit inside a word is not an episode")
        << QStringLiteral("Se7en 1995 1080p BluRay x264.mp4")
        << QStringLiteral("Se7en") << 1995;

    QTest::newRow("numeric title with its own year")
        << QStringLiteral("1917 2019 1080p BluRay x264.mp4")
        << QStringLiteral("1917") << 2019;

    QTest::newRow("numeric title that is also a year")
        << QStringLiteral("2012 2009 1080p BluRay x264.mp4")
        << QStringLiteral("2012") << 2009;

    QTest::newRow("a filename that names nothing")
        << QStringLiteral("sample.mkv") << QStringLiteral("sample") << 0;

    QTest::newRow("a numbered dump")
        << QStringLiteral("video_01.mkv") << QStringLiteral("video 01") << 0;
}

void TestFileNameParser::readsAFilmFromTheName()
{
    QFETCH(QString, fileName);
    QFETCH(QString, title);
    QFETCH(int, year);

    const ParsedFileName parsed = FileNameParser::parse(fileName);

    QCOMPARE(parsed.title, title);
    QCOMPARE(parsed.year, year);
    QCOMPARE(parsed.season, 0);
    QCOMPARE(parsed.episode, 0);
    QVERIFY(!parsed.looksLikeEpisode());
}

void TestFileNameParser::splitsAnAlsoKnownAsName()
{
    const ParsedFileName parsed = FileNameParser::parse(
        QStringLiteral("La Casa De Papel AKA Money Heist S01E01.mkv"));

    QCOMPARE(parsed.title, QStringLiteral("La Casa De Papel"));
    QCOMPARE(parsed.alternativeTitle, QStringLiteral("Money Heist"));
    QCOMPARE(parsed.season, 1);
    QCOMPARE(parsed.episode, 1);
}

void TestFileNameParser::aYearBeforeAnEpisodeMarkerLeavesTheWholeNameAsTheOtherReading()
{
    const ParsedFileName reply =
        FileNameParser::parse(QStringLiteral("Reply 1988 - S01E01.mp4"));
    QCOMPARE(reply.title, QStringLiteral("Reply"));
    QCOMPARE(reply.year, 1988);
    QCOMPARE(reply.alternativeTitle, QStringLiteral("Reply 1988"));

    const ParsedFileName film = FileNameParser::parse(
        QStringLiteral("Forrest Gump 1994 1080p BluRay x265.mkv"));
    QVERIFY(film.alternativeTitle.isEmpty());

    const ParsedFileName plain = FileNameParser::parse(
        QStringLiteral("Breaking.Bad.S01E01.1080p.BluRay.x265-RARBG.mkv"));
    QVERIFY(plain.alternativeTitle.isEmpty());
}

void TestFileNameParser::anEpisodeNumberWithNoSeasonStaysInTheTitle()
{
    const ParsedFileName film = FileNameParser::parse(
        QStringLiteral("Star.Wars.Episode.5.1980.1080p.BluRay.x264.mkv"));
    QCOMPARE(film.title, QStringLiteral("Star Wars Episode 5"));
    QCOMPARE(film.titleIfEpisode, QStringLiteral("Star Wars"));
    QCOMPARE(film.year, 1980);
    QCOMPARE(film.season, 0);
    QCOMPARE(film.episode, 5);
    QVERIFY(!film.looksLikeEpisode());

    const ParsedFileName noYear =
        FileNameParser::parse(QStringLiteral("Star.Wars.Episode.4.mkv"));
    QCOMPARE(noYear.title, QStringLiteral("Star Wars Episode 4"));
    QCOMPARE(noYear.titleIfEpisode, QStringLiteral("Star Wars"));

    const ParsedFileName shortMarker = FileNameParser::parse(
        QStringLiteral("Tvrdjava.EP01.1080p.HDTV.H264.mp4"));
    QCOMPARE(shortMarker.title, QStringLiteral("Tvrdjava EP01"));
    QCOMPARE(shortMarker.titleIfEpisode, QStringLiteral("Tvrdjava"));

    const ParsedFileName seasonInTheName = FileNameParser::parse(
        QStringLiteral("Tvrdjava.S01.EP03.1080p.HDTV.H264.mp4"));
    QCOMPARE(seasonInTheName.title, QStringLiteral("Tvrdjava"));
    QVERIFY(seasonInTheName.titleIfEpisode.isEmpty());
    QCOMPARE(seasonInTheName.season, 1);
    QCOMPARE(seasonInTheName.episode, 3);

    const ParsedFileName markerFirst =
        FileNameParser::parse(QStringLiteral("E01 Rites of Passage.mp4"));
    QVERIFY(markerFirst.titleFromFallback);
    QVERIFY(markerFirst.titleIfEpisode.isEmpty());

    const ParsedFileName noSeasonFolder = FileNameParser::parsePath(
        pathFor(QStringLiteral("Star Wars/Star.Wars.Episode.5.1980.mkv")));
    QCOMPARE(noSeasonFolder.title, QStringLiteral("Star Wars Episode 5"));
    QCOMPARE(noSeasonFolder.season, 0);
    QVERIFY(!noSeasonFolder.looksLikeEpisode());
}

void TestFileNameParser::listsTheFoldersAboveAFile_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QStringList>("names");
    QTest::addColumn<QStringList>("handles");

    QTest::newRow("a windows library path")
        << QStringLiteral("E:/TV/The Wire/Season 1/e01.mkv")
        << QStringList({QStringLiteral("Season 1"), QStringLiteral("The Wire"),
                        QStringLiteral("TV")})
        << QStringList({QStringLiteral("E:/TV/The Wire/Season 1"),
                        QStringLiteral("E:/TV/The Wire"), QStringLiteral("E:/TV")});

    QTest::newRow("only the nearest three are read")
        << QStringLiteral("E:/a/b/c/d/e01.mkv")
        << QStringList({QStringLiteral("d"), QStringLiteral("c"),
                        QStringLiteral("b")})
        << QStringList({QStringLiteral("E:/a/b/c/d"), QStringLiteral("E:/a/b/c"),
                        QStringLiteral("E:/a/b")});

    QTest::newRow("a drive letter is not a folder")
        << QStringLiteral("E:/e01.mkv") << QStringList() << QStringList();

    QTest::newRow("backslashes")
        << QStringLiteral("E:\\TV\\Show\\e01.mkv")
        << QStringList({QStringLiteral("Show"), QStringLiteral("TV")})
        << QStringList({QStringLiteral("E:\\TV\\Show"), QStringLiteral("E:\\TV")});

    QTest::newRow("an absolute path")
        << QStringLiteral("/storage/emulated/0/Media/e01.mkv")
        << QStringList({QStringLiteral("Media"), QStringLiteral("0"),
                        QStringLiteral("emulated")})
        << QStringList({QStringLiteral("/storage/emulated/0/Media"),
                        QStringLiteral("/storage/emulated/0"),
                        QStringLiteral("/storage/emulated")});

    QTest::newRow("an android volume")
        << QStringLiteral("primary:Movies/Vikings/Season 1/E01.mkv")
        << QStringList({QStringLiteral("Season 1"), QStringLiteral("Vikings"),
                        QStringLiteral("Movies")})
        << QStringList({QStringLiteral("primary:Movies/Vikings/Season 1"),
                        QStringLiteral("primary:Movies/Vikings"),
                        QStringLiteral("primary:Movies")});

    QTest::newRow("a doubled separator")
        << QStringLiteral("primary:Movies/Vikings//E01.mkv")
        << QStringList({QStringLiteral("Vikings"), QStringLiteral("Movies")})
        << QStringList({QStringLiteral("primary:Movies/Vikings"),
                        QStringLiteral("primary:Movies")});

    QTest::newRow("a file with no folder")
        << QStringLiteral("e01.mkv") << QStringList() << QStringList();

    QTest::newRow("nothing") << QString() << QStringList() << QStringList();
}

void TestFileNameParser::listsTheFoldersAboveAFile()
{
    QFETCH(QString, path);
    QFETCH(QStringList, names);
    QFETCH(QStringList, handles);

    QStringList actualNames;
    QStringList actualHandles;
    const QList<FileNameParser::Ancestor> ancestors =
        FileNameParser::ancestorsOf(path);
    for (const FileNameParser::Ancestor &ancestor : ancestors) {
        actualNames.append(ancestor.name);
        actualHandles.append(ancestor.handle);
    }

    QCOMPARE(actualNames, names);
    QCOMPARE(actualHandles, handles);
}

void TestFileNameParser::readsAShowFromAnAndroidVolume()
{
    const ParsedFileName parsed = FileNameParser::parsePath(
        QStringLiteral("primary:Movies/Vikings/Season 1/E01 Rites of Passage.mp4"));

    QCOMPARE(parsed.title, QStringLiteral("Vikings"));
    QCOMPARE(parsed.season, 1);
    QCOMPARE(parsed.episode, 1);
    QVERIFY(parsed.looksLikeEpisode());
}

void TestFileNameParser::readsTheFolderWhenTheNameIsBare_data()
{
    QTest::addColumn<QString>("relativePath");
    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("season");
    QTest::addColumn<int>("episode");

    QTest::newRow("a specials file keeps its season 0 inside a season folder")
        << QStringLiteral("Only Fools and Horses/Season 1/"
                          "Only Fools and Horses S00E02.mkv")
        << QStringLiteral("Only Fools and Horses") << 0 << 2;

    QTest::newRow("folder title keeps its season marker out of the title")
        << QStringLiteral("Vikings - Season 1 1080p WEBRip x264 AC3 MultiSubs/"
                          "E01 Rites of Passage.mp4")
        << QStringLiteral("Vikings") << 1 << 1;

    QTest::newRow("second episode of the same folder")
        << QStringLiteral("Vikings - Season 1 1080p WEBRip x264 AC3 MultiSubs/"
                          "E02 Wrath of the Northmen.mp4")
        << QStringLiteral("Vikings") << 1 << 2;

    QTest::newRow("same show, second season folder")
        << QStringLiteral("Vikings - Season 2 1080p WEBRip x264 AC3 MultiSubs/"
                          "E01 Brother's War.mp4")
        << QStringLiteral("Vikings") << 2 << 1;

    QTest::newRow("joined pair under a bare season folder")
        << QStringLiteral("Peaky Blinders/S01/s01_02.mkv")
        << QStringLiteral("Peaky Blinders") << 1 << 2;

    QTest::newRow("joined pair, second season")
        << QStringLiteral("Peaky Blinders/S02/s02_01.mkv")
        << QStringLiteral("Peaky Blinders") << 2 << 1;

    QTest::newRow("full marker in the name, title from the folder")
        << QStringLiteral("Dark/Season 1/S01E01.mkv")
        << QStringLiteral("Dark") << 1 << 1;

    QTest::newRow("marker disagrees with nothing, second season")
        << QStringLiteral("Dark/Season 2/S02E05.mkv")
        << QStringLiteral("Dark") << 2 << 5;

    QTest::newRow("episode only, season from the folder")
        << QStringLiteral("Chernobyl/Season 1/E01.mkv")
        << QStringLiteral("Chernobyl") << 1 << 1;

    QTest::newRow("lower case folder and name")
        << QStringLiteral("The Wire/s01/e01.mkv")
        << QStringLiteral("The Wire") << 1 << 1;

    QTest::newRow("same show, marker in the name")
        << QStringLiteral("The Wire/S02/s02e01.mkv")
        << QStringLiteral("The Wire") << 2 << 1;

    QTest::newRow("season from a folder carrying release tags")
        << QStringLiteral("Tvrdjava S01.1080p.HDTV.H264/"
                          "Tvrdjava.EP01.1080p.HDTV.H264.[ExYuSubs].mp4")
        << QStringLiteral("Tvrdjava") << 1 << 1;

    QTest::newRow("spelled out episode, season from the folder")
        << QStringLiteral("Band of Brothers/Season 1/"
                          "Band.of.Brothers.Episode.4.mkv")
        << QStringLiteral("Band of Brothers") << 1 << 4;
}

void TestFileNameParser::readsTheFolderWhenTheNameIsBare()
{
    QFETCH(QString, relativePath);
    QFETCH(QString, title);
    QFETCH(int, season);
    QFETCH(int, episode);

    const ParsedFileName parsed = FileNameParser::parsePath(pathFor(relativePath));

    QCOMPARE(parsed.title, title);
    QCOMPARE(parsed.season, season);
    QCOMPARE(parsed.episode, episode);
    QVERIFY(parsed.looksLikeEpisode());
}

void TestFileNameParser::knownGaps_data()
{
    QTest::addColumn<QString>("relativePath");
    QTest::addColumn<QString>("title");
    QTest::addColumn<int>("season");

    QTest::newRow("a bare number is not read as an episode")
        << QStringLiteral("Better Call Saul/Season 1/01.mkv")
        << QStringLiteral("01") << 1;

    QTest::newRow("a combined 3x01 written as 301 is not read")
        << QStringLiteral("The Wire/Season 3/301.mkv")
        << QStringLiteral("301") << 3;
}

void TestFileNameParser::knownGaps()
{
    QFETCH(QString, relativePath);
    QFETCH(QString, title);
    QFETCH(int, season);

    const ParsedFileName parsed = FileNameParser::parsePath(pathFor(relativePath));

    QCOMPARE(parsed.title, title);
    QCOMPARE(parsed.season, season);
    QCOMPARE(parsed.episode, 0);
    QVERIFY(!parsed.looksLikeEpisode());
}

void TestFileNameParser::tellsJunkFromTitleWords_data()
{
    QTest::addColumn<QString>("token");
    QTest::addColumn<bool>("junk");

    QTest::newRow("resolution") << QStringLiteral("1080p") << true;
    QTest::newRow("ultra high definition") << QStringLiteral("2160p") << true;
    QTest::newRow("source") << QStringLiteral("bluray") << true;
    QTest::newRow("rip") << QStringLiteral("brrip") << true;
    QTest::newRow("web source") << QStringLiteral("webrip") << true;
    QTest::newRow("codec") << QStringLiteral("x264") << true;
    QTest::newRow("newer codec") << QStringLiteral("x265") << true;

    QTest::newRow("a release tag that is also a title word")
        << QStringLiteral("complete") << true;

    QTest::newRow("an ordinary title word")
        << QStringLiteral("unknown") << false;
    QTest::newRow("a word that appears in show titles")
        << QStringLiteral("dark") << false;
    QTest::newRow("a number word")
        << QStringLiteral("eleven") << false;
    QTest::newRow("a channel count arriving on its own")
        << QStringLiteral("5") << false;
}

void TestFileNameParser::tellsJunkFromTitleWords()
{
    QFETCH(QString, token);
    QFETCH(bool, junk);

    QCOMPARE(FileNameParser::isJunkToken(token), junk);
}

QTEST_APPLESS_MAIN(TestFileNameParser)

#include "tst_filenameparser.moc"
