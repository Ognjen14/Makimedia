#include <QtTest>

#include "Library/SearchIndex.h"

namespace {

MediaRecord titled(qint64 id, const QString &title, int year,
                   const QString &kind = QStringLiteral("movie"))
{
    MediaRecord record;
    record.id = id;
    record.tmdbId = id;
    record.kind = kind;
    record.title = title;
    record.year = year;
    record.fileCount = 1;
    return record;
}

SearchIndex::Credit credited(qint64 mediaId, int kind, const QString &name,
                             const QString &role = QString())
{
    CreditRecord credit;
    credit.kind = kind;
    credit.name = name;
    credit.role = role;
    return {mediaId, credit};
}

QStringList titleNames(const QList<SearchIndex::TitleHit> &hits)
{
    QStringList names;
    for (const SearchIndex::TitleHit &hit : hits) {
        names.append(hit.title.title);
    }
    return names;
}

QStringList personNames(const QList<SearchIndex::PersonHit> &hits)
{
    QStringList names;
    for (const SearchIndex::PersonHit &hit : hits) {
        names.append(hit.name);
    }
    return names;
}

SearchIndex library()
{
    SearchIndex index;
    index.setTitles({
        titled(1, QStringLiteral("Dune"), 2021),
        titled(2, QStringLiteral("Dune: Part Two"), 2024),
        titled(3, QStringLiteral("The Lord of the Rings: The Fellowship of the Ring"), 2001),
        titled(4, QStringLiteral("Spider-Man: No Way Home"), 2021),
        titled(5, QString::fromUtf8("Amélie"), 2001),
        titled(6, QStringLiteral("Oppenheimer"), 2023),
        titled(7, QStringLiteral("Interstellar"), 2014),
        titled(8, QStringLiteral("Ocean's Eleven"), 2001),
        titled(9, QStringLiteral("Dark"), 2017, QStringLiteral("tv")),
    });
    index.setCredits({
        credited(1, CreditRecord::Cast, QStringLiteral("Zendaya"), QStringLiteral("Chani")),
        credited(1, CreditRecord::Director, QStringLiteral("Denis Villeneuve")),
        credited(1, CreditRecord::Writer, QStringLiteral("Jon Spaihts")),
        credited(1, CreditRecord::Cast, QStringLiteral("Tom Hardy")),
        credited(2, CreditRecord::Cast, QStringLiteral("Zendaya"), QStringLiteral("Chani")),
        credited(2, CreditRecord::Director, QStringLiteral("Denis Villeneuve")),
        credited(4, CreditRecord::Cast, QStringLiteral("Zendaya"), QStringLiteral("MJ")),
        credited(4, CreditRecord::Cast, QStringLiteral("Tom Holland")),
        credited(6, CreditRecord::Cast, QStringLiteral("Cillian Murphy"),
                 QStringLiteral("J. Robert Oppenheimer")),
        credited(6, CreditRecord::Director, QStringLiteral("Christopher Nolan")),
        credited(7, CreditRecord::Director, QStringLiteral("Christopher Nolan")),
        credited(7, CreditRecord::Cast, QStringLiteral("Matthew McConaughey"),
                 QStringLiteral("Cooper")),
        credited(8, CreditRecord::Cast, QStringLiteral("Tom Cruise")),
        credited(9, CreditRecord::Creator, QStringLiteral("Baran bo Odar")),
        credited(9, CreditRecord::Cast, QStringLiteral("Tom Hanks")),
        credited(99, CreditRecord::Cast, QStringLiteral("Zendaya"), QStringLiteral("Rue")),
    });
    return index;
}

}

class TestSearchIndex : public QObject
{
    Q_OBJECT

private slots:
    void foldDropsCaseAccentsAndPunctuation();
    void distanceCountsSwapsAsOneSlip();
    void anExactTitleComesBeforeALongerOne();
    void punctuationAndSpacesDoNotMatter();
    void everyWordMatchesFromItsStart();
    void initialsFindALongTitle();
    void oneSlipIsForgivenInLongerWords();
    void nothingMatchesNonsense();
    void oneLetterOnlyMatchesWordStarts();
    void peopleAreFoundByAnyPartOfTheirName();
    void writersAreLeftOut();
    void aDirectorLendsTheirTitles();
    void tooManyPeopleLendNoTitles();
    void titlesOfAPersonComeNewestFirst();
    void titlesOutsideTheLibraryDoNotCount();
};

void TestSearchIndex::foldDropsCaseAccentsAndPunctuation()
{
    QCOMPARE(SearchIndex::fold(QStringLiteral("Ocean's Eleven: Re-Cut")),
             QStringLiteral("oceans eleven re cut"));
    QCOMPARE(SearchIndex::fold(QString::fromUtf8("Amélie")), QStringLiteral("amelie"));
    QCOMPARE(SearchIndex::fold(QStringLiteral("  --  ")), QString());
}

void TestSearchIndex::distanceCountsSwapsAsOneSlip()
{
    QCOMPARE(SearchIndex::distance(QStringLiteral("abc"), QStringLiteral("acb"), 2), 1);
    QCOMPARE(SearchIndex::distance(QStringLiteral("kitten"), QStringLiteral("sitting"), 5), 3);
    QVERIFY(SearchIndex::distance(QStringLiteral("abcdef"), QStringLiteral("uvwxyz"), 1) > 1);
}

void TestSearchIndex::anExactTitleComesBeforeALongerOne()
{
    const QStringList names = titleNames(library().titles(QStringLiteral("dune")));
    QVERIFY(names.size() >= 2);
    QCOMPARE(names.at(0), QStringLiteral("Dune"));
    QCOMPARE(names.at(1), QStringLiteral("Dune: Part Two"));
}

void TestSearchIndex::punctuationAndSpacesDoNotMatter()
{
    const SearchIndex index = library();
    QCOMPARE(titleNames(index.titles(QStringLiteral("spiderman"))).value(0),
             QStringLiteral("Spider-Man: No Way Home"));
    QCOMPARE(titleNames(index.titles(QStringLiteral("oceans"))).value(0),
             QStringLiteral("Ocean's Eleven"));
    QCOMPARE(titleNames(index.titles(QStringLiteral("amelie"))).value(0),
             QString::fromUtf8("Amélie"));
}

void TestSearchIndex::everyWordMatchesFromItsStart()
{
    const QList<SearchIndex::TitleHit> hits =
        library().titles(QStringLiteral("way no"));
    QCOMPARE(titleNames(hits).value(0), QStringLiteral("Spider-Man: No Way Home"));
    QCOMPARE(hits.first().score, SearchIndex::titleScore(SearchIndex::WordPrefix));
}

void TestSearchIndex::initialsFindALongTitle()
{
    const QList<SearchIndex::TitleHit> hits = library().titles(QStringLiteral("lotr"));
    QCOMPARE(titleNames(hits),
             QStringList{QStringLiteral("The Lord of the Rings: The Fellowship of the Ring")});
    QCOMPARE(hits.first().score, SearchIndex::titleScore(SearchIndex::Initials));
}

void TestSearchIndex::oneSlipIsForgivenInLongerWords()
{
    const SearchIndex index = library();
    QCOMPARE(titleNames(index.titles(QStringLiteral("intersteller"))),
             QStringList{QStringLiteral("Interstellar")});
    QCOMPARE(titleNames(index.titles(QStringLiteral("oppenhiemer"))),
             QStringList{QStringLiteral("Oppenheimer")});
}

void TestSearchIndex::nothingMatchesNonsense()
{
    QVERIFY(library().titles(QStringLiteral("xyz")).isEmpty());
    QVERIFY(library().people(QStringLiteral("xyz")).isEmpty());
    QVERIFY(library().titles(QStringLiteral("  ")).isEmpty());
}

void TestSearchIndex::oneLetterOnlyMatchesWordStarts()
{
    const QStringList names = titleNames(library().titles(QStringLiteral("o")));
    QVERIFY(names.contains(QStringLiteral("Oppenheimer")));
    QVERIFY(names.contains(QStringLiteral("Ocean's Eleven")));
    QVERIFY(names.contains(QStringLiteral("The Lord of the Rings: The Fellowship of the Ring")));
    QVERIFY(!names.contains(QStringLiteral("Dune")));
}

void TestSearchIndex::peopleAreFoundByAnyPartOfTheirName()
{
    const SearchIndex index = library();
    QCOMPARE(personNames(index.people(QStringLiteral("nolan"))),
             QStringList{QStringLiteral("Christopher Nolan")});
    QCOMPARE(personNames(index.people(QStringLiteral("chris nol"))),
             QStringList{QStringLiteral("Christopher Nolan")});

    const QList<SearchIndex::PersonHit> nolan = index.people(QStringLiteral("nolan"));
    QVERIFY(nolan.first().directed);
    QVERIFY(!nolan.first().acted);
    QCOMPARE(int(nolan.first().mediaIds.size()), 2);

    QCOMPARE(personNames(index.people(QStringLiteral("zendya"))),
             QStringList{QStringLiteral("Zendaya")});
}

void TestSearchIndex::writersAreLeftOut()
{
    QVERIFY(library().people(QStringLiteral("spaihts")).isEmpty());
}

void TestSearchIndex::aDirectorLendsTheirTitles()
{
    const QList<SearchIndex::TitleHit> hits = library().titles(QStringLiteral("nolan"));
    QCOMPARE(titleNames(hits),
             (QStringList{QStringLiteral("Interstellar"), QStringLiteral("Oppenheimer")}));
    QCOMPARE(hits.first().person, QStringLiteral("Christopher Nolan"));
    QCOMPARE(hits.first().personKind, int(CreditRecord::Director));
}

void TestSearchIndex::tooManyPeopleLendNoTitles()
{
    const SearchIndex index = library();
    QCOMPARE(int(index.people(QStringLiteral("tom")).size()), 4);
    QVERIFY(index.titles(QStringLiteral("tom")).isEmpty());
}

void TestSearchIndex::titlesOfAPersonComeNewestFirst()
{
    const QList<SearchIndex::TitleHit> hits = library().titlesOf(QStringLiteral("zendaya"));
    QCOMPARE(titleNames(hits),
             (QStringList{QStringLiteral("Dune: Part Two"), QStringLiteral("Dune"),
                          QStringLiteral("Spider-Man: No Way Home")}));
    QCOMPARE(hits.first().role, QStringLiteral("Chani"));
}

void TestSearchIndex::titlesOutsideTheLibraryDoNotCount()
{
    const QList<SearchIndex::PersonHit> hits = library().people(QStringLiteral("zendaya"));
    QCOMPARE(int(hits.size()), 1);
    QCOMPARE(int(hits.first().mediaIds.size()), 3);
}

QTEST_GUILESS_MAIN(TestSearchIndex)
#include "tst_searchindex.moc"
