#include <QtTest>

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QStringList>

#include "Library/UnmatchedShowModel.h"

namespace {

ShowGrouping::Show showOf(const QString &title, const QStringList &files,
                          const QList<int> &seasons = {1})
{
    ShowGrouping::Show show;
    show.title = title;
    show.fileHandles = files;
    show.folderHandles = QStringList{QStringLiteral("F:/Media/") + title};
    show.seasons = seasons;
    return show;
}

QStringList episodesOf(const QString &title, int count)
{
    QStringList handles;
    for (int i = 1; i <= count; ++i) {
        handles.append(QStringLiteral("F:/Media/%1/e%2.mkv").arg(title).arg(i));
    }
    return handles;
}

QString titleAt(const UnmatchedShowModel &model, int row)
{
    return model.data(model.index(row, 0), UnmatchedShowModel::TitleRole).toString();
}

int filesAt(const UnmatchedShowModel &model, int row)
{
    return model.data(model.index(row, 0), UnmatchedShowModel::FileCountRole).toInt();
}

}

class TestUnmatchedShowModel : public QObject
{
    Q_OBJECT

private slots:
    void groupsReadBackThroughTheirRoles();
    void theSameGroupsAgainChangeNothing();
    void aNewGroupIsInsertedWithoutAReset();
    void aMatchedFileLeavesItsGroup();
    void aGroupWithNothingLeftGoesAway();
    void handlesFromOutsideTheListAreIgnored();
    void oneCallCanEmptyOneGroupAndThinAnother();
    void theFilesAndTheirFolderAreReadBack();
    void aFolderWithNoNameOfItsOwnFallsBackToItsHandle();
};

void TestUnmatchedShowModel::theFilesAndTheirFolderAreReadBack()
{
    ShowGrouping::Show show =
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 2));
    show.folderHandles = QStringList{QStringLiteral("content://tree/primary/Vikings")};
    show.fileNames = QStringList{QStringLiteral("e1.mkv"), QStringLiteral("e2.mkv")};

    UnmatchedShowModel model;
    model.setShows({show},
                   {{QStringLiteral("content://tree/primary/Vikings"),
                     QStringLiteral("Internal storage/TV/Vikings")}});

    const QModelIndex first = model.index(0, 0);
    QCOMPARE(model.data(first, UnmatchedShowModel::FileNamesRole).toStringList(),
             QStringList({QStringLiteral("e1.mkv"), QStringLiteral("e2.mkv")}));
    QCOMPARE(model.data(first, UnmatchedShowModel::FolderPathRole).toString(),
             QStringLiteral("Internal storage/TV/Vikings"));
}

void TestUnmatchedShowModel::aFolderWithNoNameOfItsOwnFallsBackToItsHandle()
{
    UnmatchedShowModel model;
    model.setShows({
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 2))
    });

    QCOMPARE(model.data(model.index(0, 0),
                        UnmatchedShowModel::FolderPathRole).toString(),
             QStringLiteral("F:/Media/Vikings"));
}

void TestUnmatchedShowModel::groupsReadBackThroughTheirRoles()
{
    UnmatchedShowModel model;
    model.setShows({
        showOf(QStringLiteral("Tvrdjava"), episodesOf(QStringLiteral("Tvrdjava"), 2),
               {1, 2}),
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 3))
    });

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(titleAt(model, 0), QStringLiteral("Tvrdjava"));
    QCOMPARE(filesAt(model, 0), 2);
    QCOMPARE(filesAt(model, 1), 3);

    const QModelIndex first = model.index(0, 0);
    QCOMPARE(model.data(first, UnmatchedShowModel::SeasonsRole).toList(),
             (QVariantList{1, 2}));
    QCOMPARE(model.data(first, UnmatchedShowModel::FileHandlesRole).toStringList(),
             episodesOf(QStringLiteral("Tvrdjava"), 2));
    QCOMPARE(model.data(first, UnmatchedShowModel::FolderHandlesRole).toStringList(),
             QStringList{QStringLiteral("F:/Media/Tvrdjava")});
}

void TestUnmatchedShowModel::theSameGroupsAgainChangeNothing()
{
    UnmatchedShowModel model;
    const QList<ShowGrouping::Show> shows = {
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 3))
    };
    model.setShows(shows);

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    model.setShows(shows);

    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(inserted.count(), 0);
}

void TestUnmatchedShowModel::aNewGroupIsInsertedWithoutAReset()
{
    UnmatchedShowModel model;
    model.setShows({
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 3))
    });

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    model.setShows({
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 3)),
        showOf(QStringLiteral("Tvrdjava"), episodesOf(QStringLiteral("Tvrdjava"), 2))
    });

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 1);
}

void TestUnmatchedShowModel::aMatchedFileLeavesItsGroup()
{
    UnmatchedShowModel model;
    const QStringList files = episodesOf(QStringLiteral("Vikings"), 3);
    model.setShows({showOf(QStringLiteral("Vikings"), files)});

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy counted(&model, &UnmatchedShowModel::countChanged);

    model.removeFiles({files.at(1)});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(filesAt(model, 0), 2);
    QCOMPARE(model.data(model.index(0, 0),
                        UnmatchedShowModel::FileHandlesRole).toStringList(),
             (QStringList{files.at(0), files.at(2)}));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(counted.count(), 0);
}

void TestUnmatchedShowModel::aGroupWithNothingLeftGoesAway()
{
    UnmatchedShowModel model;
    const QStringList files = episodesOf(QStringLiteral("Vikings"), 2);
    model.setShows({
        showOf(QStringLiteral("Vikings"), files),
        showOf(QStringLiteral("Tvrdjava"), episodesOf(QStringLiteral("Tvrdjava"), 1))
    });

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy counted(&model, &UnmatchedShowModel::countChanged);

    model.removeFiles(files);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(titleAt(model, 0), QStringLiteral("Tvrdjava"));
    QCOMPARE(removed.count(), 1);
    QCOMPARE(reset.count(), 0);
    QCOMPARE(counted.count(), 1);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.removeFiles(files);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(changed.count(), 0);
}

void TestUnmatchedShowModel::handlesFromOutsideTheListAreIgnored()
{
    UnmatchedShowModel model;
    model.setShows({
        showOf(QStringLiteral("Vikings"), episodesOf(QStringLiteral("Vikings"), 2))
    });

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

    model.removeFiles({QStringLiteral("F:/Media/A film.mkv")});
    model.removeFiles(QStringList());

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(filesAt(model, 0), 2);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(removed.count(), 0);
}

void TestUnmatchedShowModel::oneCallCanEmptyOneGroupAndThinAnother()
{
    UnmatchedShowModel model;
    const QStringList vikings = episodesOf(QStringLiteral("Vikings"), 3);
    const QStringList fortress = episodesOf(QStringLiteral("Tvrdjava"), 2);
    model.setShows({
        showOf(QStringLiteral("Vikings"), vikings),
        showOf(QStringLiteral("Tvrdjava"), fortress)
    });

    model.removeFiles({vikings.at(0), fortress.at(0), fortress.at(1)});

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(titleAt(model, 0), QStringLiteral("Vikings"));
    QCOMPARE(filesAt(model, 0), 2);

    model.removeFiles({vikings.at(1), vikings.at(2)});
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(TestUnmatchedShowModel)

#include "tst_unmatchedshowmodel.moc"
