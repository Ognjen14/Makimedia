#include <QtTest>

#include <QString>
#include <QUrl>

#include <algorithm>
#include <vector>

#include "Metadata/TmdbDtos.h"
#include "Metadata/TmdbImageUrl.h"

using Makimedia::Tmdb::ImageKind;
using Makimedia::Tmdb::TmdbImageConfigurationDto;
using Makimedia::Tmdb::TmdbImageUrl;

namespace {

TmdbImageConfigurationDto fullConfiguration()
{
    TmdbImageConfigurationDto configuration;
    configuration.baseUrl = "http://image.tmdb.org/t/p/";
    configuration.secureBaseUrl = "https://image.tmdb.org/t/p/";
    configuration.posterSizes = {"w92", "w154", "w185", "w342", "w500",
                                 "w780", "original"};
    configuration.stillSizes = {"w92", "w185", "w300", "original"};
    configuration.backdropSizes = {"w300", "w780", "w1280", "original"};
    configuration.profileSizes = {"w45", "w185", "h632", "original"};
    return configuration;
}

QString sizeFor(const TmdbImageConfigurationDto &configuration, int width)
{
    const auto size = TmdbImageUrl::selectPosterSize(configuration, width);
    return size.has_value() ? QString::fromStdString(*size) : QString();
}

}

class TestTmdbImageUrl : public QObject
{
    Q_OBJECT

private slots:
    void posterSizeTakesTheSmallestThatFits_data();
    void posterSizeTakesTheSmallestThatFits();
    void posterSizeClampsANonsenseWidth();
    void posterSizeFallsBackToTheWidestListed();
    void posterSizeSkipsWhatIsNotListed();
    void posterSizeGivesUpOnAnUnknownList();

    void posterUrlJoinsTheBaseTheSizeAndThePath();
    void posterUrlDoesNotMindHowManySlashes();
    void posterUrlAddsTheSlashTheBaseIsMissing();
    void posterUrlDropsAnyQueryOrFragment();
    void posterUrlIsEmptyWithoutWhatItNeeds_data();
    void posterUrlIsEmptyWithoutWhatItNeeds();

    void stillSizeHasItsOwnSet_data();
    void stillSizeHasItsOwnSet();
    void aStillNeverAsksForAPosterSize();
    void stillSizeNeverReachesForOriginal();

    void backdropSizeHasItsOwnSet_data();
    void backdropSizeHasItsOwnSet();
    void backdropSizeNeverReachesForOriginal();
    void backdropSizeGivesUpWhenOnlyOriginalIsListed();

    void widthsRunSmallestFirst();
    void widthForRoundsUpToTheLadder_data();
    void widthForRoundsUpToTheLadder();
    void everyWidthBuildsItsOwnSize();
};

void TestTmdbImageUrl::posterSizeTakesTheSmallestThatFits_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("expected");

    QTest::newRow("under the smallest") << 1 << QStringLiteral("w92");
    QTest::newRow("exactly the smallest") << 92 << QStringLiteral("w92");
    QTest::newRow("one past the smallest") << 93 << QStringLiteral("w154");
    QTest::newRow("exactly a middle size") << 185 << QStringLiteral("w185");
    QTest::newRow("one past a middle size") << 186 << QStringLiteral("w342");
    QTest::newRow("exactly the widest") << 780 << QStringLiteral("w780");
}

void TestTmdbImageUrl::posterSizeTakesTheSmallestThatFits()
{
    QFETCH(int, width);
    QFETCH(QString, expected);

    QCOMPARE(sizeFor(fullConfiguration(), width), expected);
}

void TestTmdbImageUrl::posterSizeClampsANonsenseWidth()
{
    QCOMPARE(sizeFor(fullConfiguration(), 0), QStringLiteral("w92"));
    QCOMPARE(sizeFor(fullConfiguration(), -100), QStringLiteral("w92"));
}

void TestTmdbImageUrl::posterSizeFallsBackToTheWidestListed()
{
    QCOMPARE(sizeFor(fullConfiguration(), 5000), QStringLiteral("w780"));

    TmdbImageConfigurationDto narrow = fullConfiguration();
    narrow.posterSizes = {"w92", "w154"};
    QCOMPARE(sizeFor(narrow, 5000), QStringLiteral("w154"));
}

void TestTmdbImageUrl::posterSizeSkipsWhatIsNotListed()
{
    TmdbImageConfigurationDto gapped = fullConfiguration();
    gapped.posterSizes = {"w92", "w154", "w342", "w500", "w780"};

    QCOMPARE(sizeFor(gapped, 185), QStringLiteral("w342"));
    QCOMPARE(sizeFor(gapped, 154), QStringLiteral("w154"));

    TmdbImageConfigurationDto tiny = fullConfiguration();
    tiny.posterSizes = {"w92"};
    QCOMPARE(sizeFor(tiny, 1000), QStringLiteral("w92"));
}

void TestTmdbImageUrl::posterSizeGivesUpOnAnUnknownList()
{
    TmdbImageConfigurationDto onlyOriginal = fullConfiguration();
    onlyOriginal.posterSizes = {"original"};
    QVERIFY(!TmdbImageUrl::selectPosterSize(onlyOriginal, 342).has_value());

    TmdbImageConfigurationDto none = fullConfiguration();
    none.posterSizes.clear();
    QVERIFY(!TmdbImageUrl::selectPosterSize(none, 342).has_value());

    QVERIFY(TmdbImageUrl::posterUrl(onlyOriginal, "/abc.jpg", 342).isEmpty());
}

void TestTmdbImageUrl::posterUrlJoinsTheBaseTheSizeAndThePath()
{
    const QUrl url = TmdbImageUrl::posterUrl(fullConfiguration(), "/abc.jpg", 342);

    QCOMPARE(url.toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w342/abc.jpg"));
}

void TestTmdbImageUrl::posterUrlDoesNotMindHowManySlashes()
{
    const QString expected =
        QStringLiteral("https://image.tmdb.org/t/p/w342/abc.jpg");

    QCOMPARE(TmdbImageUrl::posterUrl(fullConfiguration(), "abc.jpg", 342)
                 .toString(), expected);
    QCOMPARE(TmdbImageUrl::posterUrl(fullConfiguration(), "/abc.jpg", 342)
                 .toString(), expected);
    QCOMPARE(TmdbImageUrl::posterUrl(fullConfiguration(), "///abc.jpg", 342)
                 .toString(), expected);
}

void TestTmdbImageUrl::posterUrlAddsTheSlashTheBaseIsMissing()
{
    TmdbImageConfigurationDto unslashed = fullConfiguration();
    unslashed.secureBaseUrl = "https://image.tmdb.org/t/p";

    QCOMPARE(TmdbImageUrl::posterUrl(unslashed, "/abc.jpg", 342).toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w342/abc.jpg"));
}

void TestTmdbImageUrl::posterUrlDropsAnyQueryOrFragment()
{
    TmdbImageConfigurationDto noisy = fullConfiguration();
    noisy.secureBaseUrl = "https://image.tmdb.org/t/p/?api_key=secret#top";

    const QUrl url = TmdbImageUrl::posterUrl(noisy, "/abc.jpg", 342);

    QVERIFY(!url.hasQuery());
    QVERIFY(!url.hasFragment());
    QCOMPARE(url.toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w342/abc.jpg"));
}

void TestTmdbImageUrl::posterUrlIsEmptyWithoutWhatItNeeds_data()
{
    QTest::addColumn<QString>("secureBaseUrl");
    QTest::addColumn<QString>("path");

    QTest::newRow("no path")
        << QStringLiteral("https://image.tmdb.org/t/p/") << QString();
    QTest::newRow("no base")
        << QString() << QStringLiteral("/abc.jpg");
    QTest::newRow("an insecure base")
        << QStringLiteral("http://image.tmdb.org/t/p/")
        << QStringLiteral("/abc.jpg");
    QTest::newRow("a base that is not a url")
        << QStringLiteral("not a url at all") << QStringLiteral("/abc.jpg");
}

void TestTmdbImageUrl::posterUrlIsEmptyWithoutWhatItNeeds()
{
    QFETCH(QString, secureBaseUrl);
    QFETCH(QString, path);

    TmdbImageConfigurationDto configuration = fullConfiguration();
    configuration.secureBaseUrl = secureBaseUrl.toStdString();

    const QUrl url =
        TmdbImageUrl::posterUrl(configuration, path.toStdString(), 342);

    QVERIFY(url.isEmpty());
}

void TestTmdbImageUrl::stillSizeHasItsOwnSet_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("expected");

    QTest::newRow("under the smallest") << 1 << QStringLiteral("w92");
    QTest::newRow("exactly the smallest") << 92 << QStringLiteral("w92");
    QTest::newRow("one past the smallest") << 93 << QStringLiteral("w185");
    QTest::newRow("exactly a middle size") << 185 << QStringLiteral("w185");
    QTest::newRow("one past a middle size") << 186 << QStringLiteral("w300");
    QTest::newRow("exactly the widest named") << 300 << QStringLiteral("w300");
}

void TestTmdbImageUrl::stillSizeHasItsOwnSet()
{
    QFETCH(int, width);
    QFETCH(QString, expected);

    const QUrl url =
        TmdbImageUrl::stillUrl(fullConfiguration(), "/still.jpg", width);

    QCOMPARE(url.toString(),
             QStringLiteral("https://image.tmdb.org/t/p/%1/still.jpg")
                 .arg(expected));
}

void TestTmdbImageUrl::aStillNeverAsksForAPosterSize()
{
    for (const int width : {1, 92, 185, 300, 342, 500, 780, 5000}) {
        const QString url =
            TmdbImageUrl::stillUrl(fullConfiguration(), "/still.jpg", width)
                .toString();

        QVERIFY2(!url.contains(QStringLiteral("/w342/")), qPrintable(url));
        QVERIFY2(!url.contains(QStringLiteral("/w500/")), qPrintable(url));
        QVERIFY2(!url.contains(QStringLiteral("/w780/")), qPrintable(url));
        QVERIFY2(!url.contains(QStringLiteral("/w154/")), qPrintable(url));
    }
}

void TestTmdbImageUrl::stillSizeNeverReachesForOriginal()
{
    QCOMPARE(TmdbImageUrl::stillUrl(fullConfiguration(), "/still.jpg", 5000)
                 .toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w300/still.jpg"));

    TmdbImageConfigurationDto named = fullConfiguration();
    named.stillSizes = {"w92", "w185", "original"};
    QCOMPARE(TmdbImageUrl::stillUrl(named, "/still.jpg", 5000).toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w185/still.jpg"));

    TmdbImageConfigurationDto onlyOriginal = fullConfiguration();
    onlyOriginal.stillSizes = {"original"};
    QVERIFY(TmdbImageUrl::stillUrl(onlyOriginal, "/still.jpg", 185).isEmpty());

    TmdbImageConfigurationDto none = fullConfiguration();
    none.stillSizes.clear();
    QVERIFY(TmdbImageUrl::stillUrl(none, "/still.jpg", 185).isEmpty());
}

void TestTmdbImageUrl::backdropSizeHasItsOwnSet_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<QString>("expected");

    QTest::newRow("under the smallest") << 1 << QStringLiteral("w300");
    QTest::newRow("exactly the smallest") << 300 << QStringLiteral("w300");
    QTest::newRow("one past the smallest") << 301 << QStringLiteral("w780");
    QTest::newRow("exactly the middle") << 780 << QStringLiteral("w780");
    QTest::newRow("one past the middle") << 781 << QStringLiteral("w1280");
}

void TestTmdbImageUrl::backdropSizeHasItsOwnSet()
{
    QFETCH(int, width);
    QFETCH(QString, expected);

    const QUrl url =
        TmdbImageUrl::backdropUrl(fullConfiguration(), "/back.jpg", width);

    QCOMPARE(url.toString(),
             QStringLiteral("https://image.tmdb.org/t/p/%1/back.jpg")
                 .arg(expected));
}

void TestTmdbImageUrl::backdropSizeNeverReachesForOriginal()
{
    QCOMPARE(TmdbImageUrl::backdropUrl(fullConfiguration(), "/back.jpg", 5000)
                 .toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w1280/back.jpg"));

    TmdbImageConfigurationDto narrow = fullConfiguration();
    narrow.backdropSizes = {"w300", "original"};
    QCOMPARE(TmdbImageUrl::backdropUrl(narrow, "/back.jpg", 5000).toString(),
             QStringLiteral("https://image.tmdb.org/t/p/w300/back.jpg"));
}

void TestTmdbImageUrl::backdropSizeGivesUpWhenOnlyOriginalIsListed()
{
    TmdbImageConfigurationDto onlyOriginal = fullConfiguration();
    onlyOriginal.backdropSizes = {"original"};

    QVERIFY(TmdbImageUrl::backdropUrl(onlyOriginal, "/back.jpg", 780).isEmpty());

    TmdbImageConfigurationDto none = fullConfiguration();
    none.backdropSizes.clear();
    QVERIFY(TmdbImageUrl::backdropUrl(none, "/back.jpg", 780).isEmpty());
}

void TestTmdbImageUrl::widthsRunSmallestFirst()
{
    for (const ImageKind kind :
         {ImageKind::Poster, ImageKind::Still, ImageKind::Backdrop,
          ImageKind::Profile}) {
        const std::vector<int> widths = TmdbImageUrl::widths(kind);
        QVERIFY(!widths.empty());
        QVERIFY(std::is_sorted(widths.cbegin(), widths.cend()));
    }

    QVERIFY(TmdbImageUrl::widths(ImageKind::Poster)
            == (std::vector<int>{92, 154, 185, 342, 500, 780}));
    QVERIFY(TmdbImageUrl::widths(ImageKind::Still)
            == (std::vector<int>{92, 185, 300}));
    QVERIFY(TmdbImageUrl::widths(ImageKind::Backdrop)
            == (std::vector<int>{300, 780, 1280}));
    QVERIFY(TmdbImageUrl::widths(ImageKind::Profile)
            == (std::vector<int>{45, 185, 632}));
}

void TestTmdbImageUrl::widthForRoundsUpToTheLadder_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<int>("requested");
    QTest::addColumn<int>("expected");

    const int poster = int(ImageKind::Poster);
    const int still = int(ImageKind::Still);
    const int backdrop = int(ImageKind::Backdrop);

    QTest::newRow("poster under the smallest") << poster << 1 << 92;
    QTest::newRow("poster exactly a grid size") << poster << 342 << 342;
    QTest::newRow("poster one past a size") << poster << 186 << 342;
    QTest::newRow("poster exactly the details size") << poster << 500 << 500;
    QTest::newRow("poster past the widest") << poster << 5000 << 780;

    QTest::newRow("still exactly the widest") << still << 300 << 300;
    QTest::newRow("still at the home width") << still << 342 << 300;
    QTest::newRow("still at the television width") << still << 780 << 300;
    QTest::newRow("still one past the smallest") << still << 93 << 185;

    QTest::newRow("backdrop under the smallest") << backdrop << 1 << 300;
    QTest::newRow("backdrop at the home width") << backdrop << 342 << 780;
    QTest::newRow("backdrop exactly the phone size") << backdrop << 780 << 780;
    QTest::newRow("backdrop exactly the wide size") << backdrop << 1280 << 1280;
    QTest::newRow("backdrop past the widest") << backdrop << 5000 << 1280;
}

void TestTmdbImageUrl::widthForRoundsUpToTheLadder()
{
    QFETCH(int, kind);
    QFETCH(int, requested);
    QFETCH(int, expected);

    QCOMPARE(TmdbImageUrl::widthFor(ImageKind(kind), requested), expected);
}

void TestTmdbImageUrl::everyWidthBuildsItsOwnSize()
{
    for (const ImageKind kind :
         {ImageKind::Poster, ImageKind::Still, ImageKind::Backdrop}) {
        for (const int width : TmdbImageUrl::widths(kind)) {
            QCOMPARE(TmdbImageUrl::widthFor(kind, width), width);
            QCOMPARE(TmdbImageUrl::imageUrl(fullConfiguration(), kind,
                                            "/image.jpg", width).toString(),
                     QStringLiteral("https://image.tmdb.org/t/p/w%1/image.jpg")
                         .arg(width));
        }
    }
}

QTEST_APPLESS_MAIN(TestTmdbImageUrl)

#include "tst_tmdbimageurl.moc"
