#include <QtTest>

#include "Library/SubtitleNaming.h"

class TestSubtitleNaming : public QObject
{
    Q_OBJECT

private slots:
    void baseNameDropsTheFolderAndTheExtension_data();
    void baseNameDropsTheFolderAndTheExtension();

    void languageOfReadsOnlyARealLanguageTag_data();
    void languageOfReadsOnlyARealLanguageTag();

    void titleForKeepsWhatSetsTwoFilesApart_data();
    void titleForKeepsWhatSetsTwoFilesApart();

    void twoSubtitlesInOneLanguageStayDistinct();

    void belongsToVideoTakesOnlyNamesThatStartWithIt_data();
    void belongsToVideoTakesOnlyNamesThatStartWithIt();

    void aSubsFolderBelongsToTheOnlyVideoBesideIt_data();
    void aSubsFolderBelongsToTheOnlyVideoBesideIt();

    void describeNamesWhatIsLeftAfterTheVideo_data();
    void describeNamesWhatIsLeftAfterTheVideo();
};

void TestSubtitleNaming::baseNameDropsTheFolderAndTheExtension_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("forward slashes") << QStringLiteral("F:/Subs/Dune.en.srt")
                                     << QStringLiteral("Dune.en");
    QTest::newRow("backslashes") << QStringLiteral("C:\\Subs\\Dune.srt")
                                 << QStringLiteral("Dune");
    QTest::newRow("a percent-encoded document uri")
        << QStringLiteral("content://com.android.externalstorage.documents/document/"
                          "primary%3AMovies%2FDune.en.forced.srt")
        << QStringLiteral("Dune.en.forced");
    QTest::newRow("no extension") << QStringLiteral("Dune") << QStringLiteral("Dune");
    QTest::newRow("a leading dot") << QStringLiteral(".hidden") << QStringLiteral(".hidden");
}

void TestSubtitleNaming::baseNameDropsTheFolderAndTheExtension()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(SubtitleNaming::baseName(input), expected);
}

void TestSubtitleNaming::languageOfReadsOnlyARealLanguageTag_data()
{
    QTest::addColumn<QString>("baseName");
    QTest::addColumn<QString>("expected");

    QTest::newRow("two letters") << QStringLiteral("Dune.2021.en") << QStringLiteral("English");
    QTest::newRow("two capitals") << QStringLiteral("Dune.2021.EN") << QStringLiteral("English");
    QTest::newRow("three letters") << QStringLiteral("Dune.hrv") << QStringLiteral("Croatian");
    QTest::newRow("full name with a tag")
        << QStringLiteral("Dune.English.SDH") << QStringLiteral("English");
    QTest::newRow("forced after the language")
        << QStringLiteral("Dune.en.forced") << QStringLiteral("English");
    QTest::newRow("capitals in a caps name") << QStringLiteral("Movie.IT") << QStringLiteral("Italian");
    QTest::newRow("hindi spelled out") << QStringLiteral("Dune.hin") << QStringLiteral("Hindi");
    QTest::newRow("three letter greek") << QStringLiteral("Movie.gre") << QStringLiteral("Greek");
    QTest::newRow("arabic after a release group")
        << QStringLiteral("Novak.Djokovic.2026.1080p-[YTS.GG - YTS.BZ].ara")
        << QStringLiteral("Arabic");
    QTest::newRow("dutch three letters") << QStringLiteral("Dune.nld") << QStringLiteral("Dutch");
    QTest::newRow("estonian two letters") << QStringLiteral("Dune.et") << QStringLiteral("Estonian");
    QTest::newRow("Et as a title word") << QStringLiteral("Movie.Et") << QString();

    QTest::newRow("HI is hearing impaired, not Hindi")
        << QStringLiteral("Dune.2021.HI") << QString();
    QTest::newRow("a word that happens to be a code") << QStringLiteral("Just.Say.No") << QString();
    QTest::newRow("It as a title word") << QStringLiteral("Movie.It") << QString();
    QTest::newRow("id is not Indonesian") << QStringLiteral("Episode.id") << QString();
    QTest::newRow("a code on its own") << QStringLiteral("en") << QString();
    QTest::newRow("nothing to read") << QStringLiteral("Dune") << QString();
}

void TestSubtitleNaming::languageOfReadsOnlyARealLanguageTag()
{
    QFETCH(QString, baseName);
    QFETCH(QString, expected);

    QCOMPARE(SubtitleNaming::languageOf(baseName), expected);
}

void TestSubtitleNaming::titleForKeepsWhatSetsTwoFilesApart_data()
{
    QTest::addColumn<QString>("baseName");
    QTest::addColumn<QString>("language");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << QStringLiteral("Dune.en") << QStringLiteral("English")
                           << QStringLiteral("English");
    QTest::newRow("forced") << QStringLiteral("Dune.en.forced") << QStringLiteral("English")
                            << QStringLiteral("English (forced)");
    QTest::newRow("sdh") << QStringLiteral("Dune.en.sdh") << QStringLiteral("English")
                         << QStringLiteral("English (SDH)");
    QTest::newRow("forced and hearing impaired")
        << QStringLiteral("Dune.en.forced.HI") << QStringLiteral("English")
        << QStringLiteral("English (forced, SDH)");
    QTest::newRow("no language keeps the name")
        << QStringLiteral("Dune.2021.HI") << QString() << QStringLiteral("Dune.2021.HI");
    QTest::newRow("nothing at all") << QString() << QString()
                                    << QStringLiteral("External subtitle");
}

void TestSubtitleNaming::titleForKeepsWhatSetsTwoFilesApart()
{
    QFETCH(QString, baseName);
    QFETCH(QString, language);
    QFETCH(QString, expected);

    QCOMPARE(SubtitleNaming::titleFor(baseName, language), expected);
}

void TestSubtitleNaming::twoSubtitlesInOneLanguageStayDistinct()
{
    const QString plain = QStringLiteral("Dune.en");
    const QString forced = QStringLiteral("Dune.en.forced");

    QCOMPARE(SubtitleNaming::languageOf(plain), SubtitleNaming::languageOf(forced));
    QVERIFY(SubtitleNaming::titleFor(plain, SubtitleNaming::languageOf(plain))
            != SubtitleNaming::titleFor(forced, SubtitleNaming::languageOf(forced)));
}

void TestSubtitleNaming::belongsToVideoTakesOnlyNamesThatStartWithIt_data()
{
    QTest::addColumn<QString>("video");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<bool>("belongs");

    const QString rushHour =
        QStringLiteral("F:/Subtitle Test/Rush Hour 1998 1080p BluRay HEVC x265 5.1 BONE.mkv");
    const QString stem = QStringLiteral("Rush Hour 1998 1080p BluRay HEVC x265 5.1 BONE");

    QTest::newRow("the same name") << rushHour << stem + QStringLiteral(".srt") << true;
    QTest::newRow("a numbered copy") << rushHour << stem + QStringLiteral(" (2).srt") << true;
    QTest::newRow("a language after the name")
        << rushHour << stem + QStringLiteral(".en.srt") << true;
    QTest::newRow("a language and forced")
        << rushHour << stem + QStringLiteral(".English.forced.ass") << true;
    QTest::newRow("different case")
        << rushHour << QStringLiteral("rush hour 1998 1080p bluray hevc x265 5.1 bone.SRT") << true;
    QTest::newRow("a document uri from a granted folder")
        << rushHour
        << QStringLiteral("content://com.android.externalstorage.documents/tree/"
                          "primary%3AMovies/document/primary%3AMovies%2F"
                          "Rush%20Hour%201998%201080p%20BluRay%20HEVC%20x265%205.1%20BONE.en.srt")
        << true;
    QTest::newRow("a release group in brackets")
        << QStringLiteral("Novak.Djokovic.The.Wolf.In.Winter.2026.1080p.WEBRip.x264.AAC5.1-[YTS.GG - YTS.BZ].mp4")
        << QStringLiteral("Novak.Djokovic.The.Wolf.In.Winter.2026.1080p.WEBRip.x264.AAC5.1-[YTS.GG - YTS.BZ].SDH.eng.HI.srt")
        << true;

    QTest::newRow("only the title") << rushHour << QStringLiteral("Rush Hour.en.srt") << false;
    QTest::newRow("only the title, forced")
        << rushHour << QStringLiteral("Rush Hour.en.forced.srt") << false;
    QTest::newRow("the title with another year")
        << rushHour << QStringLiteral("Rush Hour.2021.HI.srt") << false;
    QTest::newRow("the name running on into a word")
        << rushHour << stem + QStringLiteral("X.srt") << false;
    QTest::newRow("another video") << rushHour << stem + QStringLiteral(".mkv") << false;
    QTest::newRow("not a subtitle") << rushHour << stem + QStringLiteral(".nfo") << false;
    QTest::newRow("no video name") << QString() << stem + QStringLiteral(".srt") << false;
}

void TestSubtitleNaming::belongsToVideoTakesOnlyNamesThatStartWithIt()
{
    QFETCH(QString, video);
    QFETCH(QString, subtitle);
    QFETCH(bool, belongs);

    QCOMPARE(SubtitleNaming::belongsToVideo(video, subtitle), belongs);
}

void TestSubtitleNaming::aSubsFolderBelongsToTheOnlyVideoBesideIt_data()
{
    QTest::addColumn<int>("place");
    QTest::addColumn<bool>("onlyVideo");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<bool>("belongs");

    const int beside = int(SubtitleNaming::Place::BesideVideo);
    const int subs = int(SubtitleNaming::Place::SubtitleFolder);
    const int named = int(SubtitleNaming::Place::FolderNamedAfterVideo);
    const QString fullName =
        QStringLiteral("Novak.Djokovic.The.Wolf.In.Winter.2026.1080p.WEBRip.x264.AAC5.1-[YTS.GG - YTS.BZ].en.srt");

    QTest::newRow("a language name beside the video")
        << beside << true << QStringLiteral("English.srt") << false;
    QTest::newRow("a language name in Subs, one video")
        << subs << true << QStringLiteral("Subs/English.srt") << true;
    QTest::newRow("a language name in Subs, several videos")
        << subs << false << QStringLiteral("Subs/English.srt") << false;
    QTest::newRow("the full name in Subs, several videos")
        << subs << false << QStringLiteral("Subs/") + fullName << true;
    QTest::newRow("a folder named after the video")
        << named << false << QStringLiteral("Subs/Novak/2_English.srt") << true;
    QTest::newRow("not a subtitle in a named folder")
        << named << false << QStringLiteral("Subs/Novak/notes.txt") << false;
    QTest::newRow("not a subtitle in Subs")
        << subs << true << QStringLiteral("Subs/readme.txt") << false;
}

void TestSubtitleNaming::aSubsFolderBelongsToTheOnlyVideoBesideIt()
{
    QFETCH(int, place);
    QFETCH(bool, onlyVideo);
    QFETCH(QString, subtitle);
    QFETCH(bool, belongs);

    const QString video =
        QStringLiteral("Novak.Djokovic.The.Wolf.In.Winter.2026.1080p.WEBRip.x264.AAC5.1-[YTS.GG - YTS.BZ].mp4");
    QCOMPARE(SubtitleNaming::belongsToVideo(video, subtitle,
                                            SubtitleNaming::Place(place), onlyVideo),
             belongs);
}

void TestSubtitleNaming::describeNamesWhatIsLeftAfterTheVideo_data()
{
    QTest::addColumn<QString>("video");
    QTest::addColumn<QString>("subtitle");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("code");

    const QString novak =
        QStringLiteral("Novak.Djokovic.The.Wolf.In.Winter.2026.1080p.WEBRip.x264.AAC5.1-[YTS.GG - YTS.BZ].mp4");
    const QString rushHour =
        QStringLiteral("Rush Hour 1998 1080p BluRay HEVC x265 5.1 BONE.mkv");

    QTest::newRow("a bare language") << novak << QStringLiteral("Subs/English.srt")
                                     << QStringLiteral("English") << QStringLiteral("en");
    QTest::newRow("forced first") << novak << QStringLiteral("Subs/Forced.eng.srt")
                                  << QStringLiteral("English (forced)") << QStringLiteral("en");
    QTest::newRow("hearing impaired on both sides")
        << novak << QStringLiteral("Subs/SDH.eng.HI.srt")
        << QStringLiteral("English (SDH)") << QStringLiteral("en");
    QTest::newRow("a regional variant")
        << novak << QStringLiteral("Subs/Latin American.spa.srt")
        << QStringLiteral("Spanish (Latin American)") << QStringLiteral("es");
    QTest::newRow("brazilian portuguese")
        << novak << QStringLiteral("Subs/Brazilian.por.srt")
        << QStringLiteral("Portuguese (Brazilian)") << QStringLiteral("pt");
    QTest::newRow("simplified chinese")
        << novak << QStringLiteral("Subs/Simplified.chi.srt")
        << QStringLiteral("Chinese (Simplified)") << QStringLiteral("zh");
    QTest::newRow("a numbered language") << novak << QStringLiteral("Subs/Novak/2_English.srt")
                                         << QStringLiteral("English") << QStringLiteral("en");
    QTest::newRow("dutch by its terminology code")
        << novak << QStringLiteral("Subs/nld.srt")
        << QStringLiteral("Dutch") << QStringLiteral("nl");
    QTest::newRow("estonian") << novak << QStringLiteral("Subs/est.srt")
                              << QStringLiteral("Estonian") << QStringLiteral("et");
    QTest::newRow("latvian") << novak << QStringLiteral("Subs/lav.srt")
                             << QStringLiteral("Latvian") << QStringLiteral("lv");
    QTest::newRow("lithuanian") << novak << QStringLiteral("Subs/lit.srt")
                                << QStringLiteral("Lithuanian") << QStringLiteral("lt");
    QTest::newRow("a code after the full name")
        << novak
        << QStringLiteral("Novak.Djokovic.The.Wolf.In.Winter.2026.1080p.WEBRip.x264.AAC5.1-[YTS.GG - YTS.BZ].sr.srt")
        << QStringLiteral("Serbian") << QStringLiteral("sr");
    QTest::newRow("exactly the video name")
        << rushHour << QStringLiteral("Rush Hour 1998 1080p BluRay HEVC x265 5.1 BONE.srt")
        << QStringLiteral("SRT") << QString();
    QTest::newRow("a numbered copy keeps what sets it apart")
        << rushHour << QStringLiteral("Rush Hour 1998 1080p BluRay HEVC x265 5.1 BONE (2).srt")
        << QStringLiteral("SRT (2)") << QString();
    QTest::newRow("a release name says nothing about language")
        << QStringLiteral("The Lord of the Rings - The Fellowship of the Ring (2001) "
                          "Extended RM4K (1080p BluRay x265 10bit Tigole).mkv")
        << QStringLiteral("The Lord of the Rings - The Fellowship of the Ring (2001) "
                          "Extended RM4K (1080p BluRay x265 10bit Tigole).srt")
        << QStringLiteral("SRT") << QString();
    QTest::newRow("a vobsub index named after the film")
        << rushHour << QStringLiteral("Rush Hour 1998 1080p BluRay HEVC x265 5.1 BONE.idx")
        << QStringLiteral("IDX") << QString();
    QTest::newRow("a name of its own is kept")
        << rushHour << QStringLiteral("Subs/my own notes.srt")
        << QStringLiteral("my own notes") << QString();
}

void TestSubtitleNaming::describeNamesWhatIsLeftAfterTheVideo()
{
    QFETCH(QString, video);
    QFETCH(QString, subtitle);
    QFETCH(QString, title);
    QFETCH(QString, code);

    const SubtitleNaming::Description description = SubtitleNaming::describe(video, subtitle);
    QCOMPARE(description.title, title);
    QCOMPARE(description.code, code);
}

QTEST_APPLESS_MAIN(TestSubtitleNaming)

#include "tst_subtitlenaming.moc"
