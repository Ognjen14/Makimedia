#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "Metadata/TmdbError.h"
#include "Metadata/TmdbJsonReader.h"

using Makimedia::Tmdb::TmdbError;
using Makimedia::Tmdb::TmdbJsonReader;

namespace {

const QString kField = QStringLiteral("field");

TmdbJsonReader readerWith(const QJsonValue &value)
{
    QJsonObject object;
    object.insert(kField, value);
    return TmdbJsonReader(object);
}

QJsonObject aFullObject()
{
    QJsonObject nested;
    nested.insert(QStringLiteral("iso_639_1"), QStringLiteral("en"));

    QJsonObject object;
    object.insert(QStringLiteral("id"), 1438);
    object.insert(QStringLiteral("vote_average"), 8.6);
    object.insert(QStringLiteral("name"), QStringLiteral("The Wire"));
    object.insert(QStringLiteral("adult"), false);
    object.insert(QStringLiteral("genre_ids"), QJsonArray({18, 80}));
    object.insert(QStringLiteral("spoken_language"), nested);
    return object;
}

enum Reader {
    Integer,
    Number,
    Text,
    Boolean,
    Array,
    Object
};

bool readOptional(TmdbJsonReader &reader, int which)
{
    switch (which) {
    case Integer:
        return reader.optionalInteger(kField).has_value();
    case Number:
        return reader.optionalNumber(kField).has_value();
    case Text:
        return reader.optionalString(kField).has_value();
    case Boolean:
        return reader.optionalBoolean(kField).has_value();
    case Array:
        return reader.optionalArray(kField).has_value();
    case Object:
        return reader.optionalObject(kField).has_value();
    }
    return false;
}

bool readRequired(TmdbJsonReader &reader, int which)
{
    switch (which) {
    case Integer:
        return reader.requiredInteger(kField).has_value();
    case Number:
        return reader.requiredNumber(kField).has_value();
    case Text:
        return reader.requiredString(kField).has_value();
    case Boolean:
        return reader.requiredBoolean(kField).has_value();
    case Array:
        return reader.requiredArray(kField).has_value();
    case Object:
        return reader.requiredObject(kField).has_value();
    }
    return false;
}

void addAReaderColumn()
{
    QTest::addColumn<int>("which");
    QTest::addColumn<QJsonValue>("value");
}

void addEveryReaderRow()
{
    QTest::addColumn<int>("which");

    QTest::newRow("integer") << int(Integer);
    QTest::newRow("number") << int(Number);
    QTest::newRow("string") << int(Text);
    QTest::newRow("boolean") << int(Boolean);
    QTest::newRow("array") << int(Array);
    QTest::newRow("object") << int(Object);
}

}

class TestTmdbJsonReader : public QObject
{
    Q_OBJECT

private slots:
    void aFreshReaderHasNoError();

    void everyTypeReadsItsOwnValue();
    void optionalReadsTheSameValues();

    void aMissingRequiredFieldIsAnError_data();
    void aMissingRequiredFieldIsAnError();
    void aMissingOptionalFieldIsNotAnError_data();
    void aMissingOptionalFieldIsNotAnError();

    void nullCountsAsMissing_data();
    void nullCountsAsMissing();

    void theWrongTypeIsAnErrorEvenWhenOptional_data();
    void theWrongTypeIsAnErrorEvenWhenOptional();

    void anIntegerHasToBeWhole();
    void anIntegerHasToFitInSixtyFourBits();
    void aWholeNumberAtTheEdgeStillReads();
    void aNumberKeepsItsFraction();
    void aStringComesBackAsUtf8();

    void theFirstErrorIsTheOneKept();
    void oneErrorStopsEverythingAfterIt();
    void theErrorSaysWhichFieldAndNothingMore();
};

void TestTmdbJsonReader::aFreshReaderHasNoError()
{
    TmdbJsonReader reader{QJsonObject()};

    QVERIFY(!reader.hasError());
    QVERIFY(!reader.error().has_value());
}

void TestTmdbJsonReader::everyTypeReadsItsOwnValue()
{
    TmdbJsonReader reader{aFullObject()};

    const auto id = reader.requiredInteger(QStringLiteral("id"));
    QVERIFY(id.has_value());
    QCOMPARE(*id, Q_INT64_C(1438));

    const auto vote = reader.requiredNumber(QStringLiteral("vote_average"));
    QVERIFY(vote.has_value());
    QCOMPARE(*vote, 8.6);

    const auto name = reader.requiredString(QStringLiteral("name"));
    QVERIFY(name.has_value());
    QCOMPARE(QString::fromStdString(*name), QStringLiteral("The Wire"));

    const auto adult = reader.requiredBoolean(QStringLiteral("adult"));
    QVERIFY(adult.has_value());
    QCOMPARE(*adult, false);

    const auto genres = reader.requiredArray(QStringLiteral("genre_ids"));
    QVERIFY(genres.has_value());
    QCOMPARE(genres->size(), 2);
    QCOMPARE(genres->at(0).toInt(), 18);

    const auto language = reader.requiredObject(QStringLiteral("spoken_language"));
    QVERIFY(language.has_value());
    QCOMPARE(language->value(QStringLiteral("iso_639_1")).toString(),
             QStringLiteral("en"));

    QVERIFY(!reader.hasError());
}

void TestTmdbJsonReader::optionalReadsTheSameValues()
{
    TmdbJsonReader reader{aFullObject()};

    QCOMPARE(*reader.optionalInteger(QStringLiteral("id")), Q_INT64_C(1438));
    QCOMPARE(*reader.optionalNumber(QStringLiteral("vote_average")), 8.6);
    QCOMPARE(QString::fromStdString(*reader.optionalString(QStringLiteral("name"))),
             QStringLiteral("The Wire"));
    QCOMPARE(*reader.optionalBoolean(QStringLiteral("adult")), false);
    QCOMPARE(reader.optionalArray(QStringLiteral("genre_ids"))->size(), 2);
    QVERIFY(reader.optionalObject(QStringLiteral("spoken_language")).has_value());

    QVERIFY(!reader.hasError());
}

void TestTmdbJsonReader::aMissingRequiredFieldIsAnError_data()
{
    addEveryReaderRow();
}

void TestTmdbJsonReader::aMissingRequiredFieldIsAnError()
{
    QFETCH(int, which);

    TmdbJsonReader reader{QJsonObject()};

    QVERIFY(!readRequired(reader, which));
    QVERIFY(reader.hasError());
    QCOMPARE(reader.error()->category(), TmdbError::Category::Parse);
}

void TestTmdbJsonReader::aMissingOptionalFieldIsNotAnError_data()
{
    addEveryReaderRow();
}

void TestTmdbJsonReader::aMissingOptionalFieldIsNotAnError()
{
    QFETCH(int, which);

    TmdbJsonReader reader{QJsonObject()};

    QVERIFY(!readOptional(reader, which));
    QVERIFY(!reader.hasError());
}

void TestTmdbJsonReader::nullCountsAsMissing_data()
{
    addAReaderColumn();

    QTest::newRow("integer") << int(Integer) << QJsonValue(QJsonValue::Null);
    QTest::newRow("number") << int(Number) << QJsonValue(QJsonValue::Null);
    QTest::newRow("string") << int(Text) << QJsonValue(QJsonValue::Null);
    QTest::newRow("boolean") << int(Boolean) << QJsonValue(QJsonValue::Null);
    QTest::newRow("array") << int(Array) << QJsonValue(QJsonValue::Null);
    QTest::newRow("object") << int(Object) << QJsonValue(QJsonValue::Null);
}

void TestTmdbJsonReader::nullCountsAsMissing()
{
    QFETCH(int, which);
    QFETCH(QJsonValue, value);

    TmdbJsonReader optional = readerWith(value);
    QVERIFY(!readOptional(optional, which));
    QVERIFY(!optional.hasError());

    TmdbJsonReader required = readerWith(value);
    QVERIFY(!readRequired(required, which));
    QVERIFY(required.hasError());
}

void TestTmdbJsonReader::theWrongTypeIsAnErrorEvenWhenOptional_data()
{
    addAReaderColumn();

    QTest::newRow("an integer asked of a string")
        << int(Integer) << QJsonValue(QStringLiteral("1438"));
    QTest::newRow("a number asked of a boolean")
        << int(Number) << QJsonValue(true);
    QTest::newRow("a string asked of a number")
        << int(Text) << QJsonValue(1438);
    QTest::newRow("a boolean asked of a number")
        << int(Boolean) << QJsonValue(1);
    QTest::newRow("an array asked of an object")
        << int(Array) << QJsonValue(QJsonObject());
    QTest::newRow("an object asked of an array")
        << int(Object) << QJsonValue(QJsonArray());
}

void TestTmdbJsonReader::theWrongTypeIsAnErrorEvenWhenOptional()
{
    QFETCH(int, which);
    QFETCH(QJsonValue, value);

    TmdbJsonReader optional = readerWith(value);
    QVERIFY(!readOptional(optional, which));
    QVERIFY(optional.hasError());
    QCOMPARE(optional.error()->category(), TmdbError::Category::Parse);

    TmdbJsonReader required = readerWith(value);
    QVERIFY(!readRequired(required, which));
    QVERIFY(required.hasError());
}

void TestTmdbJsonReader::anIntegerHasToBeWhole()
{
    TmdbJsonReader reader = readerWith(QJsonValue(3.5));

    QVERIFY(!reader.optionalInteger(kField).has_value());
    QVERIFY(reader.hasError());

    TmdbJsonReader asNumber = readerWith(QJsonValue(3.5));
    QCOMPARE(*asNumber.optionalNumber(kField), 3.5);
    QVERIFY(!asNumber.hasError());
}

void TestTmdbJsonReader::anIntegerHasToFitInSixtyFourBits()
{
    TmdbJsonReader tooBig = readerWith(QJsonValue(1.0e19));
    QVERIFY(!tooBig.optionalInteger(kField).has_value());
    QVERIFY(tooBig.hasError());

    TmdbJsonReader tooSmall = readerWith(QJsonValue(-1.0e19));
    QVERIFY(!tooSmall.optionalInteger(kField).has_value());
    QVERIFY(tooSmall.hasError());

    TmdbJsonReader asNumber = readerWith(QJsonValue(1.0e19));
    QCOMPARE(*asNumber.optionalNumber(kField), 1.0e19);
    QVERIFY(!asNumber.hasError());
}

void TestTmdbJsonReader::aWholeNumberAtTheEdgeStillReads()
{
    TmdbJsonReader high = readerWith(QJsonValue(9007199254740992.0));
    QCOMPARE(*high.optionalInteger(kField), Q_INT64_C(9007199254740992));
    QVERIFY(!high.hasError());

    TmdbJsonReader low = readerWith(QJsonValue(-9007199254740992.0));
    QCOMPARE(*low.optionalInteger(kField), Q_INT64_C(-9007199254740992));
    QVERIFY(!low.hasError());

    TmdbJsonReader zero = readerWith(QJsonValue(0));
    QCOMPARE(*zero.optionalInteger(kField), Q_INT64_C(0));
    QVERIFY(!zero.hasError());
}

void TestTmdbJsonReader::aNumberKeepsItsFraction()
{
    TmdbJsonReader reader = readerWith(QJsonValue(8.6));

    const auto value = reader.optionalNumber(kField);
    QVERIFY(value.has_value());
    QCOMPARE(*value, 8.6);

    TmdbJsonReader whole = readerWith(QJsonValue(8));
    QCOMPARE(*whole.optionalNumber(kField), 8.0);
}

void TestTmdbJsonReader::aStringComesBackAsUtf8()
{
    const QString title =
        QStringLiteral("Am") + QChar(0x00E9) + QStringLiteral("lie");

    TmdbJsonReader reader = readerWith(QJsonValue(title));

    const auto value = reader.optionalString(kField);
    QVERIFY(value.has_value());
    QCOMPARE(QString::fromStdString(*value), title);

    const std::string expected = std::string("Am") + "\xC3\xA9" + "lie";
    QCOMPARE(value->size(), expected.size());
    QVERIFY(*value == expected);

    TmdbJsonReader empty = readerWith(QJsonValue(QString()));
    const auto blank = empty.optionalString(kField);
    QVERIFY(blank.has_value());
    QVERIFY(blank->empty());
    QVERIFY(!empty.hasError());
}

void TestTmdbJsonReader::theFirstErrorIsTheOneKept()
{
    QJsonObject object;
    object.insert(QStringLiteral("first"), QStringLiteral("not a number"));
    object.insert(QStringLiteral("second"), QStringLiteral("also not"));

    TmdbJsonReader reader{object};

    QVERIFY(!reader.optionalInteger(QStringLiteral("first")).has_value());
    QVERIFY(!reader.optionalInteger(QStringLiteral("second")).has_value());

    const QString context =
        QString::fromStdString(reader.error()->userSafeContext());
    QVERIFY(context.contains(QStringLiteral("first")));
    QVERIFY(!context.contains(QStringLiteral("second")));
}

void TestTmdbJsonReader::oneErrorStopsEverythingAfterIt()
{
    QJsonObject object = aFullObject();
    object.insert(QStringLiteral("broken"), QStringLiteral("not a number"));

    TmdbJsonReader reader{object};

    QVERIFY(reader.requiredInteger(QStringLiteral("id")).has_value());

    QVERIFY(!reader.optionalInteger(QStringLiteral("broken")).has_value());
    QVERIFY(reader.hasError());

    QVERIFY(!reader.requiredInteger(QStringLiteral("id")).has_value());
    QVERIFY(!reader.requiredString(QStringLiteral("name")).has_value());
    QVERIFY(!reader.optionalBoolean(QStringLiteral("adult")).has_value());

    const QString context =
        QString::fromStdString(reader.error()->userSafeContext());
    QVERIFY(context.contains(QStringLiteral("broken")));
}

void TestTmdbJsonReader::theErrorSaysWhichFieldAndNothingMore()
{
    TmdbJsonReader reader{QJsonObject()};
    QVERIFY(!reader.requiredInteger(QStringLiteral("runtime")).has_value());

    QVERIFY(reader.hasError());

    const TmdbError &error = *reader.error();
    QCOMPARE(error.category(), TmdbError::Category::Parse);
    QVERIFY(!error.httpStatus().has_value());
    QVERIFY(!error.tmdbStatusCode().has_value());
    QVERIFY(!error.retryAfter().has_value());

    const QString context = QString::fromStdString(error.userSafeContext());
    QVERIFY(context.contains(QStringLiteral("runtime")));
    QVERIFY(context.contains(QStringLiteral("TMDB")));
}

QTEST_APPLESS_MAIN(TestTmdbJsonReader)

#include "tst_tmdbjsonreader.moc"
