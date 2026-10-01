#include <QtTest>

#include "TextFold.h"

class TestTextFold : public QObject
{
    Q_OBJECT

private slots:
    void foldsCaseAndAccents_data();
    void foldsCaseAndAccents();

    void ordersAnAccentedTitleAmongItsLetter();
    void keepsTwoSpellingsApartWhenTheyFoldTheSame();
};

void TestTextFold::foldsCaseAndAccents_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << QStringLiteral("The Wire") << QStringLiteral("the wire");
    QTest::newRow("caron") << QStringLiteral("Žene") << QStringLiteral("zene");
    QTest::newRow("acute and caron")
        << QStringLiteral("Čuvari Ćelije Šume") << QStringLiteral("cuvari celije sume");
    QTest::newRow("stroke d") << QStringLiteral("ĐORĐE") << QStringLiteral("djordje");
    QTest::newRow("dj spelling") << QStringLiteral("Djordje") << QStringLiteral("djordje");
    QTest::newRow("sharp s") << QStringLiteral("Straße") << QStringLiteral("strasse");
    QTest::newRow("slashed o") << QStringLiteral("Ønske") << QStringLiteral("onske");
    QTest::newRow("stroke l") << QStringLiteral("Łódź") << QStringLiteral("lodz");
    QTest::newRow("ligature") << QStringLiteral("Æon Œuvre") << QStringLiteral("aeon oeuvre");
    QTest::newRow("punctuation kept")
        << QStringLiteral("Žene.S01E01.mkv") << QStringLiteral("zene.s01e01.mkv");
    QTest::newRow("empty") << QString() << QString();
}

void TestTextFold::foldsCaseAndAccents()
{
    QFETCH(QString, text);
    QFETCH(QString, expected);

    QCOMPARE(TextFold::key(text), expected);
}

void TestTextFold::ordersAnAccentedTitleAmongItsLetter()
{
    QVERIFY(TextFold::compare(QStringLiteral("Crna"), QStringLiteral("Čuvari")) < 0);
    QVERIFY(TextFold::compare(QStringLiteral("Čuvari"), QStringLiteral("Dan")) < 0);
    QVERIFY(TextFold::compare(QStringLiteral("Žene"), QStringLiteral("zodiac")) < 0);
    QVERIFY(TextFold::compare(QStringLiteral("apple"), QStringLiteral("Banana")) < 0);
}

void TestTextFold::keepsTwoSpellingsApartWhenTheyFoldTheSame()
{
    const int one = TextFold::compare(QStringLiteral("Zene"), QStringLiteral("Žene"));
    const int other = TextFold::compare(QStringLiteral("Žene"), QStringLiteral("Zene"));

    QVERIFY(one != 0);
    QCOMPARE(one < 0, other > 0);
    QCOMPARE(TextFold::compare(QStringLiteral("Žene"), QStringLiteral("Žene")), 0);
}

QTEST_APPLESS_MAIN(TestTextFold)

#include "tst_textfold.moc"
