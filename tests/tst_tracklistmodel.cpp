#include <QtTest>

#include <QAbstractItemModel>
#include <QSignalSpy>

#include "Player/TrackListModel.h"

namespace {

QVariantMap track(qint64 id, const QString &lang, bool external = false,
                  bool selected = false)
{
    QVariantMap item;
    item.insert(QStringLiteral("id"), id);
    item.insert(QStringLiteral("title"), QString());
    item.insert(QStringLiteral("lang"), lang);
    item.insert(QStringLiteral("codec"), QStringLiteral("subrip"));
    item.insert(QStringLiteral("selected"), selected);
    item.insert(QStringLiteral("external"), external);
    return item;
}

QVariantList embeddedPair()
{
    return {track(1, QStringLiteral("eng")), track(2, QStringLiteral("srp"))};
}

}

class TestTrackListModel : public QObject
{
    Q_OBJECT

private slots:
    void tracksReadBackThroughTheirRoles();
    void anAddedSubtitleIsOneInsert();
    void thirtyTwoAddedOneAtATimeNeverReset();
    void choosingATrackChangesOnlyTheRowsInvolved();
    void theSameListAgainChangesNothing();
    void aTrackIsFoundById();
    void onlyWhatIsInsideTheFileIsEmbedded();
};

void TestTrackListModel::tracksReadBackThroughTheirRoles()
{
    TrackListModel model;
    model.setTracks(embeddedPair());

    QCOMPARE(model.rowCount(), 2);
    const QModelIndex second = model.index(1, 0);
    QCOMPARE(model.data(second, TrackListModel::IdRole).toLongLong(), Q_INT64_C(2));
    QCOMPARE(model.data(second, TrackListModel::LangRole).toString(),
             QStringLiteral("srp"));
    QCOMPARE(model.data(second, TrackListModel::ExternalRole).toBool(), false);
}

void TestTrackListModel::anAddedSubtitleIsOneInsert()
{
    TrackListModel model;
    model.setTracks(embeddedPair());

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy counted(&model, &TrackListModel::countChanged);
    QSignalSpy revised(&model, &TrackListModel::revisionChanged);

    QVariantList withOne = embeddedPair();
    withOne.append(track(3, QStringLiteral("hrv"), true));
    model.setTracks(withOne);

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(counted.count(), 1);
    QCOMPARE(revised.count(), 1);
}

void TestTrackListModel::thirtyTwoAddedOneAtATimeNeverReset()
{
    TrackListModel model;
    QVariantList tracks = embeddedPair();
    model.setTracks(tracks);

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    for (int i = 0; i < 32; ++i) {
        tracks.append(track(10 + i, QStringLiteral("eng"), true));
        model.setTracks(tracks);
    }

    QCOMPARE(model.rowCount(), 34);
    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 32);
}

void TestTrackListModel::choosingATrackChangesOnlyTheRowsInvolved()
{
    TrackListModel model;
    model.setTracks({track(1, QStringLiteral("eng"), false, true),
                     track(2, QStringLiteral("srp")),
                     track(3, QStringLiteral("hrv"))});

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setTracks({track(1, QStringLiteral("eng")),
                     track(2, QStringLiteral("srp")),
                     track(3, QStringLiteral("hrv"), false, true)});

    QCOMPARE(reset.count(), 0);
    QCOMPARE(changed.count(), 2);
    QVERIFY(!model.data(model.index(0, 0), TrackListModel::SelectedRole).toBool());
    QVERIFY(model.data(model.index(2, 0), TrackListModel::SelectedRole).toBool());
}

void TestTrackListModel::theSameListAgainChangesNothing()
{
    TrackListModel model;
    model.setTracks(embeddedPair());

    QSignalSpy revised(&model, &TrackListModel::revisionChanged);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setTracks(embeddedPair());

    QCOMPARE(revised.count(), 0);
    QCOMPARE(changed.count(), 0);
}

void TestTrackListModel::aTrackIsFoundById()
{
    TrackListModel model;
    model.setTracks(embeddedPair());

    QCOMPARE(model.byId(2).value(QStringLiteral("lang")).toString(),
             QStringLiteral("srp"));
    QVERIFY(model.byId(9).isEmpty());
}

void TestTrackListModel::onlyWhatIsInsideTheFileIsEmbedded()
{
    TrackListModel model;
    QVariantList tracks = embeddedPair();
    tracks.append(track(3, QStringLiteral("hrv"), true));
    model.setTracks(tracks);

    QCOMPARE(model.embeddedCount(), 2);
}

QTEST_GUILESS_MAIN(TestTrackListModel)

#include "tst_tracklistmodel.moc"
