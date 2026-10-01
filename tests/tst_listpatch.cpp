#include <QtTest>

#include <QList>
#include <QString>
#include <QStringList>

#include "Library/ListPatch.h"

namespace {

QStringList keysOf(const QString &letters)
{
    QStringList keys;
    for (const QChar letter : letters) {
        keys.append(QString(letter));
    }
    return keys;
}

QStringList applied(const QStringList &before, const QStringList &after,
                    const QList<ListPatch::Step> &steps)
{
    QStringList rows = before;

    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Remove:
            rows.remove(step.from, step.count);
            break;
        case ListPatch::Kind::Insert:
            for (int i = 0; i < step.count; ++i) {
                rows.insert(step.to + i, after.at(step.to + i));
            }
            break;
        case ListPatch::Kind::Move:
            rows.move(step.from, step.to);
            break;
        case ListPatch::Kind::Update:
            break;
        }
    }
    return rows;
}

int countOf(const QList<ListPatch::Step> &steps, ListPatch::Kind kind)
{
    int found = 0;
    for (const ListPatch::Step &step : steps) {
        if (step.kind == kind) {
            ++found;
        }
    }
    return found;
}

}

class TestListPatch : public QObject
{
    Q_OBJECT

private slots:
    void anUnchangedListIsAllUpdates();
    void newRowsAtTheEndAreOneInsert();
    void newRowsAtTheTopAreOneInsert();
    void goneRowsAreRemovedInRuns();
    void scatteredRemovalsKeepTheirRows();
    void aRowThatMovedIsOneMove();
    void aMoveAlwaysComesFromBelowItsTarget();
    void emptyListsBothWays();
    void aReorderedListIsNotWorthPatching();
    void aChangedRowIsWorthPatching();
    void everyPlanArrivesAtTheNewList();
    void everyPlanArrivesAtTheNewList_data();
};

void TestListPatch::anUnchangedListIsAllUpdates()
{
    const QStringList rows = keysOf(QStringLiteral("abcd"));
    const QList<ListPatch::Step> steps = ListPatch::plan(rows, rows);

    QCOMPARE(steps.size(), 4);
    QCOMPARE(countOf(steps, ListPatch::Kind::Update), 4);
    QCOMPARE(ListPatch::structuralRows(steps), 0);
    QVERIFY(ListPatch::worthPatching(steps, 4, 4));
}

void TestListPatch::newRowsAtTheEndAreOneInsert()
{
    const QStringList before = keysOf(QStringLiteral("abc"));
    const QStringList after = keysOf(QStringLiteral("abcde"));
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    QCOMPARE(countOf(steps, ListPatch::Kind::Insert), 1);
    QCOMPARE(countOf(steps, ListPatch::Kind::Move), 0);
    QCOMPARE(countOf(steps, ListPatch::Kind::Remove), 0);

    const ListPatch::Step insert = steps.last();
    QCOMPARE(insert.kind, ListPatch::Kind::Insert);
    QCOMPARE(insert.to, 3);
    QCOMPARE(insert.count, 2);
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::newRowsAtTheTopAreOneInsert()
{
    const QStringList before = keysOf(QStringLiteral("cd"));
    const QStringList after = keysOf(QStringLiteral("abcd"));
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    QCOMPARE(steps.first().kind, ListPatch::Kind::Insert);
    QCOMPARE(steps.first().to, 0);
    QCOMPARE(steps.first().count, 2);
    QCOMPARE(countOf(steps, ListPatch::Kind::Move), 0);
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::goneRowsAreRemovedInRuns()
{
    const QStringList before = keysOf(QStringLiteral("abcde"));
    const QStringList after = keysOf(QStringLiteral("ae"));
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    QCOMPARE(countOf(steps, ListPatch::Kind::Remove), 1);
    QCOMPARE(steps.first().kind, ListPatch::Kind::Remove);
    QCOMPARE(steps.first().from, 1);
    QCOMPARE(steps.first().count, 3);
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::scatteredRemovalsKeepTheirRows()
{
    const QStringList before = keysOf(QStringLiteral("abcde"));
    const QStringList after = keysOf(QStringLiteral("ace"));
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    QCOMPARE(countOf(steps, ListPatch::Kind::Remove), 2);
    QCOMPARE(steps.at(0).from, 3);
    QCOMPARE(steps.at(0).count, 1);
    QCOMPARE(steps.at(1).from, 1);
    QCOMPARE(steps.at(1).count, 1);
    QCOMPARE(countOf(steps, ListPatch::Kind::Move), 0);
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::aRowThatMovedIsOneMove()
{
    const QStringList before = keysOf(QStringLiteral("abcd"));
    const QStringList after = keysOf(QStringLiteral("dabc"));
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    QCOMPARE(countOf(steps, ListPatch::Kind::Move), 1);
    QCOMPARE(steps.first().kind, ListPatch::Kind::Move);
    QCOMPARE(steps.first().from, 3);
    QCOMPARE(steps.first().to, 0);
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::aMoveAlwaysComesFromBelowItsTarget()
{
    const QStringList before = keysOf(QStringLiteral("abcdef"));
    const QStringList after = keysOf(QStringLiteral("fbadce"));
    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);

    for (const ListPatch::Step &step : steps) {
        if (step.kind == ListPatch::Kind::Move) {
            QVERIFY(step.from > step.to);
        }
    }
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::emptyListsBothWays()
{
    const QStringList rows = keysOf(QStringLiteral("abc"));

    const QList<ListPatch::Step> emptied = ListPatch::plan(rows, QStringList());
    QCOMPARE(emptied.size(), 1);
    QCOMPARE(emptied.first().kind, ListPatch::Kind::Remove);
    QCOMPARE(emptied.first().from, 0);
    QCOMPARE(emptied.first().count, 3);
    QCOMPARE(applied(rows, QStringList(), emptied), QStringList());

    const QList<ListPatch::Step> filled = ListPatch::plan(QStringList(), rows);
    QCOMPARE(filled.size(), 1);
    QCOMPARE(filled.first().kind, ListPatch::Kind::Insert);
    QCOMPARE(filled.first().count, 3);
    QCOMPARE(applied(QStringList(), rows, filled), rows);

    QVERIFY(ListPatch::plan(QStringList(), QStringList()).isEmpty());
}

void TestListPatch::aReorderedListIsNotWorthPatching()
{
    QStringList before;
    QStringList after;
    for (int i = 0; i < 100; ++i) {
        before.append(QString::number(i));
        after.append(QString::number(99 - i));
    }

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    QVERIFY(ListPatch::structuralRows(steps) > 50);
    QVERIFY(!ListPatch::worthPatching(steps, before.size(), after.size()));
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::aChangedRowIsWorthPatching()
{
    QStringList before;
    for (int i = 0; i < 100; ++i) {
        before.append(QString::number(i));
    }

    QStringList after = before;
    after.removeAt(40);
    after.append(QStringLiteral("new"));

    const QList<ListPatch::Step> steps = ListPatch::plan(before, after);
    QCOMPARE(ListPatch::structuralRows(steps), 2);
    QVERIFY(ListPatch::worthPatching(steps, before.size(), after.size()));
    QCOMPARE(applied(before, after, steps), after);
}

void TestListPatch::everyPlanArrivesAtTheNewList_data()
{
    QTest::addColumn<QString>("before");
    QTest::addColumn<QString>("after");

    QTest::newRow("same") << "abcdef" << "abcdef";
    QTest::newRow("appended") << "abc" << "abcdef";
    QTest::newRow("prepended") << "def" << "abcdef";
    QTest::newRow("emptied") << "abcdef" << "";
    QTest::newRow("filled") << "" << "abcdef";
    QTest::newRow("nothing in common") << "abc" << "xyz";
    QTest::newRow("reversed") << "abcdef" << "fedcba";
    QTest::newRow("shuffled") << "abcdef" << "cfbdae";
    QTest::newRow("one moved to the front") << "abcdef" << "fabcde";
    QTest::newRow("one moved to the back") << "abcdef" << "bcdefa";
    QTest::newRow("swapped neighbours") << "abcdef" << "bacdfe";
    QTest::newRow("removed and reordered") << "abcdef" << "fdb";
    QTest::newRow("added and reordered") << "bdf" << "abcdef";
    QTest::newRow("replaced the middle") << "abcdef" << "abxyef";
    QTest::newRow("one row") << "a" << "b";
}

void TestListPatch::everyPlanArrivesAtTheNewList()
{
    QFETCH(QString, before);
    QFETCH(QString, after);

    const QStringList from = keysOf(before);
    const QStringList to = keysOf(after);

    const QList<ListPatch::Step> steps = ListPatch::plan(from, to);
    QCOMPARE(applied(from, to, steps), to);

    for (const ListPatch::Step &step : steps) {
        if (step.kind == ListPatch::Kind::Move) {
            QVERIFY(step.from > step.to);
        }
        if (step.kind != ListPatch::Kind::Update) {
            QVERIFY(step.count >= 1);
        }
    }

    int placed = 0;
    for (const ListPatch::Step &step : steps) {
        switch (step.kind) {
        case ListPatch::Kind::Insert:
            placed += step.count;
            break;
        case ListPatch::Kind::Move:
        case ListPatch::Kind::Update:
            ++placed;
            break;
        case ListPatch::Kind::Remove:
            break;
        }
    }
    QCOMPARE(qsizetype(placed), to.size());
}

QTEST_GUILESS_MAIN(TestListPatch)

#include "tst_listpatch.moc"
