#include <QtTest>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QStringList>

#include "Library/RemovedFileModel.h"

namespace {

RemovedFileRecord removedFile(const QString &name)
{
    RemovedFileRecord record;
    record.handle = QStringLiteral("F:/Media/") + name;
    record.folderId = 1;
    record.displayName = name;
    record.folder = QStringLiteral("F:/Media");
    return record;
}

QList<RemovedFileRecord> removedFiles(const QStringList &names)
{
    QList<RemovedFileRecord> records;
    for (const QString &name : names) {
        records.append(removedFile(name));
    }
    return records;
}

QStringList namesIn(const RemovedFileModel &model)
{
    QStringList names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names.append(model.data(model.index(row, 0),
                                RemovedFileModel::DisplayNameRole).toString());
    }
    return names;
}

}

class TestRemovedFileModel : public QObject
{
    Q_OBJECT

private slots:
    void newlyRemovedFilesArriveOnTopAsOneInsert();
    void aFileRemovedTwiceIsListedOnce();
    void puttingBackScatteredFilesRemovesOnlyThem();
    void aHandleLookupStillWorksAfterRowsMove();
};

void TestRemovedFileModel::newlyRemovedFilesArriveOnTopAsOneInsert()
{
    RemovedFileModel model;
    model.setFiles(removedFiles({QStringLiteral("old.mkv")}));

    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    model.prependFiles(removedFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv")}));

    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(inserted.first().at(1).toInt(), 0);
    QCOMPARE(inserted.first().at(2).toInt(), 1);
    QCOMPARE(namesIn(model),
             QStringList({QStringLiteral("a.mkv"), QStringLiteral("b.mkv"),
                          QStringLiteral("old.mkv")}));
}

void TestRemovedFileModel::aFileRemovedTwiceIsListedOnce()
{
    RemovedFileModel model;
    model.setFiles(removedFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv")}));

    model.prependFiles(removedFiles({QStringLiteral("b.mkv")}));

    QCOMPARE(namesIn(model),
             QStringList({QStringLiteral("b.mkv"), QStringLiteral("a.mkv")}));
}

void TestRemovedFileModel::puttingBackScatteredFilesRemovesOnlyThem()
{
    RemovedFileModel model;
    model.setFiles(removedFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv"),
                                 QStringLiteral("c.mkv"), QStringLiteral("d.mkv"),
                                 QStringLiteral("e.mkv"), QStringLiteral("f.mkv")}));

    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    model.removeHandles({QStringLiteral("F:/Media/b.mkv"),
                         QStringLiteral("F:/Media/c.mkv"),
                         QStringLiteral("F:/Media/f.mkv"),
                         QStringLiteral("F:/Media/nothing.mkv")});

    QCOMPARE(reset.count(), 0);
    QCOMPARE(removed.count(), 2);
    QCOMPARE(namesIn(model),
             QStringList({QStringLiteral("a.mkv"), QStringLiteral("d.mkv"),
                          QStringLiteral("e.mkv")}));
}

void TestRemovedFileModel::aHandleLookupStillWorksAfterRowsMove()
{
    RemovedFileModel model;
    model.setFiles(removedFiles({QStringLiteral("a.mkv"), QStringLiteral("b.mkv"),
                                 QStringLiteral("c.mkv")}));

    model.removeHandles({QStringLiteral("F:/Media/a.mkv")});
    model.prependFiles(removedFiles({QStringLiteral("z.mkv")}));
    model.removeHandles({QStringLiteral("F:/Media/c.mkv")});

    QCOMPARE(namesIn(model),
             QStringList({QStringLiteral("z.mkv"), QStringLiteral("b.mkv")}));
    QCOMPARE(model.handles(),
             QStringList({QStringLiteral("F:/Media/z.mkv"),
                          QStringLiteral("F:/Media/b.mkv")}));
}

QTEST_GUILESS_MAIN(TestRemovedFileModel)
#include "tst_removedfilemodel.moc"
