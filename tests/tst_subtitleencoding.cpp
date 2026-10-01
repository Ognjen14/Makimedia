#include <QtTest>

#include "Library/SubtitleEncoding.h"

class TestSubtitleEncoding : public QObject
{
    Q_OBJECT

private slots:
    void readsTheEncodingSubtitlesComeIn_data();
    void readsTheEncodingSubtitlesComeIn();

    void decodeGivesBackTheLetters_data();
    void decodeGivesBackTheLetters();
};

void TestSubtitleEncoding::readsTheEncodingSubtitlesComeIn_data()
{
    QTest::addColumn<QByteArray>("sample");
    QTest::addColumn<QString>("expected");

    QTest::newRow("nothing at all") << QByteArray() << QStringLiteral("utf-8");
    QTest::newRow("plain ascii")
        << QByteArrayLiteral("1\r\n00:00:09,349 --> 00:00:13,349\r\nwww.titlovi.com\r\n")
        << QStringLiteral("utf-8");
    QTest::newRow("croatian in utf-8")
        << QStringLiteral("Dok su kretali u borbu za šesti naslov, budućnost").toUtf8()
        << QStringLiteral("utf-8");
    QTest::newRow("serbian cyrillic in utf-8, from the tablet")
        << QByteArray::fromHex("0ad097d0b020d0b2d0b5d19bd0b8d0bdd18320d0b4d0b5d186d0b5")
        << QStringLiteral("utf-8");
    QTest::newRow("utf-8 cut in the middle of a letter")
        << QByteArray::fromHex("62756475c4")
        << QStringLiteral("utf-8");
    QTest::newRow("a utf-8 byte order mark")
        << QByteArray::fromHex("efbbbf310d0a")
        << QStringLiteral("utf-8");
    QTest::newRow("a utf-16 little endian mark")
        << QByteArray::fromHex("fffe31000d000a00")
        << QStringLiteral("utf-16le");
    QTest::newRow("a utf-16 big endian mark")
        << QByteArray::fromHex("feff00310d000a00")
        << QStringLiteral("utf-16be");

    QTest::newRow("the last dance, windows-1250")
        << QByteArray::fromHex(
               "446f6b207375206b726574616c69207520626f726275207a61209a65737469"
               "206e61736c6f762c0d0a0d0a340d0a"
               "62756475e66e6f7374207469 6d6120706f7374616a616c610d0a"
               "6a652073766520 6e65697a766a65736e696a6f0d0a"
               "30303a32353a30302c373439202d2d3e2030303a32353a30332c3936300d0a"
               "84416b6f206d6f9e659a2c209a75")
        << QStringLiteral("cp1250");
    QTest::newRow("serbian cyrillic, windows-1251")
        << QByteArray::fromHex(
               "31340d0a30303a30313a34352c323536202d2d3e2030303a30313a34372c3637330d0a"
               "a3e020e2e59ee8edf320e4e5f6e5209ce5e3eee2e520"
               "e3e5ede5f0e0f6e8bce520ede8bce520e1e8ebee")
        << QStringLiteral("cp1251");
    QTest::newRow("one latin letter outside ascii")
        << QByteArray::fromHex("5a61209a746f206e6f7669206e61736c6f76")
        << QStringLiteral("cp1250");
}

void TestSubtitleEncoding::readsTheEncodingSubtitlesComeIn()
{
    QFETCH(QByteArray, sample);
    QFETCH(QString, expected);

    QCOMPARE(SubtitleEncoding::guess(sample), expected);
}

void TestSubtitleEncoding::decodeGivesBackTheLetters_data()
{
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<QString>("encoding");
    QTest::addColumn<QString>("expected");

    QTest::newRow("the last dance, a line")
        << QByteArray::fromHex("7a61209a65737469206e61736c6f762c")
        << QStringLiteral("cp1250")
        << QStringLiteral("za šesti naslov,");
    QTest::newRow("the last dance, quotes and two letters")
        << QByteArray::fromHex("84416b6f206d6f9e659a2c209a75746972616a2e93")
        << QStringLiteral("cp1250")
        << QStringLiteral("„Ako možeš, šutiraj.“");
    QTest::newRow("every serbian latin letter, windows-1250")
        << QByteArray::fromHex("9a8af0d0e8c8e6c69e8e")
        << QStringLiteral("cp1250")
        << QStringLiteral("šŠđĐčČćĆžŽ");
    QTest::newRow("serbian cyrillic, windows-1251")
        << QByteArray::fromHex("a3e020e2e59ee8edf3")
        << QStringLiteral("cp1251")
        << QStringLiteral("Ја већину");
    QTest::newRow("the letters only serbian cyrillic has")
        << QByteArray::fromHex("80909a8a9c8cbca39f8f9e8e")
        << QStringLiteral("cp1251")
        << QStringLiteral("ЂђљЉњЊјЈџЏћЋ");
    QTest::newRow("utf-16 little endian, mark dropped")
        << QByteArray::fromHex("fffe61010d01")
        << QStringLiteral("utf-16le")
        << QStringLiteral("šč");
    QTest::newRow("utf-8 left as it is")
        << QStringLiteral("šđčćž").toUtf8()
        << QStringLiteral("utf-8")
        << QStringLiteral("šđčćž");
}

void TestSubtitleEncoding::decodeGivesBackTheLetters()
{
    QFETCH(QByteArray, bytes);
    QFETCH(QString, encoding);
    QFETCH(QString, expected);

    QCOMPARE(SubtitleEncoding::decode(bytes, encoding), expected);
}

QTEST_APPLESS_MAIN(TestSubtitleEncoding)

#include "tst_subtitleencoding.moc"
