#include <QtTest>

#include "Player/VideoTraits.h"

class TestVideoTraits : public QObject
{
    Q_OBJECT

private slots:
    void isHdr_data();
    void isHdr();
};

void TestVideoTraits::isHdr_data()
{
    QTest::addColumn<QString>("primaries");
    QTest::addColumn<QString>("gamma");
    QTest::addColumn<bool>("expected");

    QTest::newRow("hdr10 is pq on bt.2020")
        << QStringLiteral("bt.2020") << QStringLiteral("pq") << true;
    QTest::newRow("broadcast hdr is hlg on bt.2020")
        << QStringLiteral("bt.2020") << QStringLiteral("hlg") << true;

    QTest::newRow("wide gamut on an sdr curve is not hdr")
        << QStringLiteral("bt.2020") << QStringLiteral("bt.1886") << false;
    QTest::newRow("wide gamut on srgb is not hdr")
        << QStringLiteral("bt.2020") << QStringLiteral("srgb") << false;

    QTest::newRow("pq on bt.709 is not hdr")
        << QStringLiteral("bt.709") << QStringLiteral("pq") << false;
    QTest::newRow("hlg on bt.709 is not hdr")
        << QStringLiteral("bt.709") << QStringLiteral("hlg") << false;

    QTest::newRow("ordinary sdr")
        << QStringLiteral("bt.709") << QStringLiteral("bt.1886") << false;

    QTest::newRow("nothing known yet")
        << QString() << QString() << false;
    QTest::newRow("only the curve arrived")
        << QString() << QStringLiteral("pq") << false;
    QTest::newRow("only the gamut arrived")
        << QStringLiteral("bt.2020") << QString() << false;

    QTest::newRow("the spelling is mpv's, lower case")
        << QStringLiteral("BT.2020") << QStringLiteral("PQ") << false;

    QTest::newRow("the arguments are not interchangeable")
        << QStringLiteral("pq") << QStringLiteral("bt.2020") << false;
}

void TestVideoTraits::isHdr()
{
    QFETCH(QString, primaries);
    QFETCH(QString, gamma);
    QFETCH(bool, expected);

    QCOMPARE(VideoTraits::isHdr(primaries, gamma), expected);
}

QTEST_APPLESS_MAIN(TestVideoTraits)

#include "tst_videotraits.moc"
