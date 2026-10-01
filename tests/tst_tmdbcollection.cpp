#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "Metadata/TmdbClient.h"

using namespace Makimedia::Tmdb;

namespace {

QJsonObject movieDetails(const QJsonValue &collection)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), 165);
    object.insert(QStringLiteral("title"), QStringLiteral("Back to the Future Part II"));
    object.insert(QStringLiteral("release_date"), QStringLiteral("1989-11-22"));
    object.insert(QStringLiteral("runtime"), 108);
    object.insert(QStringLiteral("belongs_to_collection"), collection);
    return object;
}

QJsonObject part(int id, const QString &title, const QJsonValue &releaseDate)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("title"), title);
    object.insert(QStringLiteral("release_date"), releaseDate);
    object.insert(QStringLiteral("poster_path"), QStringLiteral("/p%1.jpg").arg(id));
    object.insert(QStringLiteral("backdrop_path"), QJsonValue::Null);
    return object;
}

}

class TestTmdbCollection : public QObject
{
    Q_OBJECT

private slots:
    void aFilmInACollectionCarriesItsReference();
    void aFilmWithoutACollectionCarriesNone();
    void aCollectionReadsItsNameArtworkAndFilms();
    void aBrokenFilmIsSkippedNotTheCollection();
    void aCollectionWithoutAnIdIsAnError();
};

void TestTmdbCollection::aFilmInACollectionCarriesItsReference()
{
    QJsonObject collection;
    collection.insert(QStringLiteral("id"), 264);
    collection.insert(QStringLiteral("name"), QStringLiteral("Back to the Future Collection"));
    collection.insert(QStringLiteral("poster_path"), QStringLiteral("/bttf.jpg"));
    collection.insert(QStringLiteral("backdrop_path"), QJsonValue::Null);

    const TmdbClient::TitleDetailsResult result =
        TmdbClient::parseMovieDetailsResponse(movieDetails(collection));

    const auto *details = std::get_if<TmdbTitleDetailsDto>(&result);
    QVERIFY(details);
    QVERIFY(details->collection.has_value());
    QCOMPARE(details->collection->id, TmdbId(264));
    QCOMPARE(details->collection->name, std::string("Back to the Future Collection"));
    QCOMPARE(details->collection->posterPath, std::optional<std::string>("/bttf.jpg"));
    QVERIFY(!details->collection->backdropPath.has_value());
}

void TestTmdbCollection::aFilmWithoutACollectionCarriesNone()
{
    const TmdbClient::TitleDetailsResult result =
        TmdbClient::parseMovieDetailsResponse(movieDetails(QJsonValue::Null));

    const auto *details = std::get_if<TmdbTitleDetailsDto>(&result);
    QVERIFY(details);
    QVERIFY(!details->collection.has_value());
    QCOMPARE(details->runtimeMinutes, 108);
}

void TestTmdbCollection::aCollectionReadsItsNameArtworkAndFilms()
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), 8091);
    object.insert(QStringLiteral("name"), QStringLiteral("Alien Collection"));
    object.insert(QStringLiteral("overview"), QStringLiteral("In space."));
    object.insert(QStringLiteral("poster_path"), QStringLiteral("/alien.jpg"));
    object.insert(QStringLiteral("backdrop_path"), QStringLiteral("/alien-bd.jpg"));
    object.insert(QStringLiteral("parts"), QJsonArray{
        part(679, QStringLiteral("Aliens"), QStringLiteral("1986-07-18")),
        part(348, QStringLiteral("Alien"), QStringLiteral("1979-05-25")),
        part(9999, QStringLiteral("Alien: Next"), QStringLiteral("")),
    });

    const TmdbClient::CollectionResult result = TmdbClient::parseCollectionResponse(object);

    const auto *collection = std::get_if<TmdbCollectionDto>(&result);
    QVERIFY(collection);
    QCOMPARE(collection->id, TmdbId(8091));
    QCOMPARE(collection->name, std::string("Alien Collection"));
    QCOMPARE(collection->overview, std::string("In space."));
    QCOMPARE(collection->posterPath, std::optional<std::string>("/alien.jpg"));
    QCOMPARE(collection->backdropPath, std::optional<std::string>("/alien-bd.jpg"));
    QCOMPARE(collection->parts.size(), std::size_t(3));
    QCOMPARE(collection->parts.at(0).id, TmdbId(679));
    QCOMPARE(collection->parts.at(0).releaseDate, std::optional<std::string>("1986-07-18"));
    QCOMPARE(collection->parts.at(1).posterPath, std::optional<std::string>("/p348.jpg"));
    QVERIFY(!collection->parts.at(1).backdropPath.has_value());
    QVERIFY(!collection->parts.at(2).releaseDate.has_value());
}

void TestTmdbCollection::aBrokenFilmIsSkippedNotTheCollection()
{
    QJsonObject withoutId;
    withoutId.insert(QStringLiteral("title"), QStringLiteral("Nobody"));

    QJsonObject object;
    object.insert(QStringLiteral("id"), 8091);
    object.insert(QStringLiteral("name"), QStringLiteral("Alien Collection"));
    object.insert(QStringLiteral("parts"), QJsonArray{
        withoutId,
        QStringLiteral("not an object"),
        part(348, QStringLiteral("Alien"), QJsonValue::Null),
    });

    const TmdbClient::CollectionResult result = TmdbClient::parseCollectionResponse(object);

    const auto *collection = std::get_if<TmdbCollectionDto>(&result);
    QVERIFY(collection);
    QCOMPARE(collection->parts.size(), std::size_t(1));
    QCOMPARE(collection->parts.front().id, TmdbId(348));
    QVERIFY(!collection->parts.front().releaseDate.has_value());
}

void TestTmdbCollection::aCollectionWithoutAnIdIsAnError()
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), QStringLiteral("Alien Collection"));

    const TmdbClient::CollectionResult result = TmdbClient::parseCollectionResponse(object);

    QVERIFY(std::holds_alternative<TmdbError>(result));
}

QTEST_GUILESS_MAIN(TestTmdbCollection)

#include "tst_tmdbcollection.moc"
