#include <QtTest>

#include <QList>

#include <algorithm>

#include "Library/RowPlacement.h"

namespace {

bool ascending(int a, int b)
{
    return a < b;
}

QList<int> applied(QList<int> rows, int current, int value, bool keep)
{
    const RowPlacement::Step step =
        RowPlacement::plan(rows, current, value, keep, &ascending);

    switch (step.kind) {
    case RowPlacement::Kind::Nothing:
        break;
    case RowPlacement::Kind::Update:
        rows[step.from] = value;
        break;
    case RowPlacement::Kind::Insert:
        rows.insert(step.to, value);
        break;
    case RowPlacement::Kind::Remove:
        rows.removeAt(step.from);
        break;
    case RowPlacement::Kind::Move:
        rows.move(step.from, step.to);
        rows[step.to] = value;
        break;
    }
    return rows;
}

}

class TestRowPlacement : public QObject
{
    Q_OBJECT

private slots:
    void aNewRowGoesWhereTheOrderPutsIt();
    void aNewRowCanStartOrEndTheList();
    void aRowThatStaysInPlaceIsOnlyUpdated();
    void aRowThatIsNoLongerKeptIsRemoved();
    void aRowNotListedAndNotKeptIsLeftAlone();
    void aRowMovesToWhereItNowBelongs();
    void everyPlacementLeavesTheListSorted();
};

void TestRowPlacement::aNewRowGoesWhereTheOrderPutsIt()
{
    const QList<int> rows = {10, 20, 30};

    const RowPlacement::Step step = RowPlacement::plan(rows, -1, 25, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Insert);
    QCOMPARE(step.to, 2);
    QCOMPARE(applied(rows, -1, 25, true), (QList<int>{10, 20, 25, 30}));
}

void TestRowPlacement::aNewRowCanStartOrEndTheList()
{
    const QList<int> rows = {10, 20, 30};

    QCOMPARE(RowPlacement::plan(rows, -1, 5, true, &ascending).to, 0);
    QCOMPARE(RowPlacement::plan(rows, -1, 40, true, &ascending).to, 3);
    QCOMPARE(RowPlacement::plan(QList<int>(), -1, 7, true, &ascending).to, 0);
}

void TestRowPlacement::aRowThatStaysInPlaceIsOnlyUpdated()
{
    const QList<int> rows = {10, 20, 30};

    RowPlacement::Step step = RowPlacement::plan(rows, 1, 22, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Update);
    QCOMPARE(step.from, 1);

    step = RowPlacement::plan(rows, 1, 20, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Update);

    step = RowPlacement::plan(rows, 0, 1, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Update);

    step = RowPlacement::plan(rows, 2, 99, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Update);
}

void TestRowPlacement::aRowThatIsNoLongerKeptIsRemoved()
{
    const QList<int> rows = {10, 20, 30};

    const RowPlacement::Step step = RowPlacement::plan(rows, 1, 20, false, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Remove);
    QCOMPARE(step.from, 1);
    QCOMPARE(applied(rows, 1, 20, false), (QList<int>{10, 30}));
}

void TestRowPlacement::aRowNotListedAndNotKeptIsLeftAlone()
{
    const QList<int> rows = {10, 20, 30};

    const RowPlacement::Step step = RowPlacement::plan(rows, -1, 15, false, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Nothing);
    QCOMPARE(applied(rows, -1, 15, false), rows);
}

void TestRowPlacement::aRowMovesToWhereItNowBelongs()
{
    const QList<int> rows = {10, 20, 30, 40};

    RowPlacement::Step step = RowPlacement::plan(rows, 0, 35, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Move);
    QCOMPARE(step.from, 0);
    QCOMPARE(step.to, 2);
    QCOMPARE(applied(rows, 0, 35, true), (QList<int>{20, 30, 35, 40}));

    step = RowPlacement::plan(rows, 3, 15, true, &ascending);
    QCOMPARE(step.kind, RowPlacement::Kind::Move);
    QCOMPARE(step.from, 3);
    QCOMPARE(step.to, 1);
    QCOMPARE(applied(rows, 3, 15, true), (QList<int>{10, 15, 20, 30}));
}

void TestRowPlacement::everyPlacementLeavesTheListSorted()
{
    const QList<int> rows = {10, 20, 30, 40, 50};

    for (int current = -1; current < rows.size(); ++current) {
        for (int value = 5; value <= 55; value += 5) {
            const qsizetype existing = rows.indexOf(value);
            if (existing >= 0 && existing != current) {
                continue;
            }

            for (const bool keep : {true, false}) {
                const QList<int> result = applied(rows, current, value, keep);
                QVERIFY(std::is_sorted(result.begin(), result.end()));

                const qsizetype expected = rows.size()
                    + (current < 0 && keep ? 1 : 0)
                    - (current >= 0 && !keep ? 1 : 0);
                QCOMPARE(result.size(), expected);
                QCOMPARE(result.contains(value), keep);
            }
        }
    }
}

QTEST_GUILESS_MAIN(TestRowPlacement)

#include "tst_rowplacement.moc"
