#include <QtTest>

#include "Streaming/ConnectedDeviceModel.h"
#include "Streaming/SessionTable.h"
#include "Streaming/StreamProtocol.h"

#include <QSignalSpy>

class TestSessionTable : public QObject
{
    Q_OBJECT

private slots:
    void eachDeviceGetsItsOwnToken();
    void theSameDeviceGetsTheSameTokenBack();
    void aSilentDeviceLeavesTheListButKeepsItsToken();
    void aHeartbeatSaysWhatIsPlaying();
    void aClosedSessionIsForgotten();
    void theDeviceListChangesRowByRow();
};

void TestSessionTable::eachDeviceGetsItsOwnToken()
{
    SessionTable table;
    bool changed = false;
    const QString tablet = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                      QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                      0, &changed);
    QVERIFY(changed);
    const QString phone = table.open(QStringLiteral("b"), QStringLiteral("Phone"),
                                     QStringLiteral("phone"), QStringLiteral("192.168.0.3"),
                                     10, &changed);
    QVERIFY(changed);
    QVERIFY(tablet != phone);
    QCOMPARE(tablet.size(), qsizetype(32));
    QCOMPARE(table.active().size(), qsizetype(2));
    QCOMPARE(table.active().at(0).name, QStringLiteral("Tab"));
    QCOMPARE(table.active().at(1).name, QStringLiteral("Phone"));

    QVERIFY(table.touch(tablet, 20, &changed));
    QVERIFY(!changed);
    QVERIFY(!table.touch(QStringLiteral("made-up"), 20, &changed));
    QVERIFY(!table.touch(QString(), 20, &changed));
}

void TestSessionTable::theSameDeviceGetsTheSameTokenBack()
{
    SessionTable table;
    bool changed = false;
    const QString first = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                     0, &changed);
    const QString again = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                     100, &changed);
    QCOMPARE(again, first);
    QVERIFY(!changed);
    QCOMPARE(table.size(), 1);

    const QString moved = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.7"),
                                     200, &changed);
    QCOMPARE(moved, first);
    QVERIFY(changed);
    QCOMPARE(table.active().at(0).peer, QStringLiteral("192.168.0.7"));
}

void TestSessionTable::aSilentDeviceLeavesTheListButKeepsItsToken()
{
    SessionTable table;
    bool changed = false;
    const QString token = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                     0, &changed);
    Playing playing;
    playing.title = QStringLiteral("Heat");
    playing.position = 60;
    QVERIFY(table.heartbeat(token, playing, 5000, &changed));

    QVERIFY(table.expire(5000 + StreamProtocol::DeviceSilenceMs, StreamProtocol::DeviceSilenceMs)
                .isEmpty());
    const QStringList dropped = table.expire(5001 + StreamProtocol::DeviceSilenceMs,
                                             StreamProtocol::DeviceSilenceMs);
    QCOMPARE(dropped, QStringList{ QStringLiteral("Tab") });
    QVERIFY(table.active().isEmpty());
    QVERIFY(table.expire(90000, StreamProtocol::DeviceSilenceMs).isEmpty());

    QVERIFY(table.touch(token, 91000, &changed));
    QVERIFY(changed);
    QCOMPARE(table.active().size(), qsizetype(1));
    QVERIFY(!table.active().at(0).watching());
}

void TestSessionTable::aHeartbeatSaysWhatIsPlaying()
{
    SessionTable table;
    bool changed = false;
    const QString token = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                     0, &changed);

    Playing playing;
    playing.title = QStringLiteral("Heat");
    playing.position = 60;
    playing.duration = 10200;
    playing.paused = true;
    QVERIFY(table.heartbeat(token, playing, 5000, &changed));
    QVERIFY(changed);
    StreamDevice device = table.deviceFor(token);
    QVERIFY(device.watching());
    QCOMPARE(device.title, QStringLiteral("Heat"));
    QCOMPARE(device.position, 60.0);
    QVERIFY(device.paused);

    QVERIFY(table.heartbeat(token, playing, 10000, &changed));
    QVERIFY(!changed);

    QVERIFY(table.heartbeat(token, Playing(), 15000, &changed));
    QVERIFY(changed);
    device = table.deviceFor(token);
    QVERIFY(!device.watching());
    QCOMPARE(device.position, 0.0);
    QVERIFY(!device.paused);

    QVERIFY(!table.heartbeat(QStringLiteral("made-up"), playing, 15000, &changed));
}

void TestSessionTable::aClosedSessionIsForgotten()
{
    SessionTable table;
    bool changed = false;
    const QString token = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                     0, &changed);
    QString name;
    QVERIFY(table.close(token, &name));
    QCOMPARE(name, QStringLiteral("Tab"));
    QVERIFY(!table.touch(token, 10, &changed));
    QVERIFY(!table.close(token, &name));
    QVERIFY(table.active().isEmpty());

    const QString fresh = table.open(QStringLiteral("a"), QStringLiteral("Tab"),
                                     QStringLiteral("tablet"), QStringLiteral("192.168.0.2"),
                                     20, &changed);
    QVERIFY(fresh != token);
}

void TestSessionTable::theDeviceListChangesRowByRow()
{
    ConnectedDeviceModel model;
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    StreamDevice tablet;
    tablet.deviceId = QStringLiteral("a");
    tablet.name = QStringLiteral("Tab");
    StreamDevice phone;
    phone.deviceId = QStringLiteral("b");
    phone.name = QStringLiteral("Phone");

    model.apply({ tablet, phone });
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(inserted.count(), 2);

    tablet.title = QStringLiteral("Heat");
    model.apply({ tablet, phone });
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).toModelIndex().row(), 0);
    QCOMPARE(model.watchingCount(), 1);

    model.apply({ tablet, phone });
    QCOMPARE(changed.count(), 1);

    model.apply({ phone });
    QCOMPARE(removed.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), ConnectedDeviceModel::NameRole).toString(),
             QStringLiteral("Phone"));
    QCOMPARE(model.watchingCount(), 0);

    model.apply({});
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(reset.count(), 0);
}

QTEST_GUILESS_MAIN(TestSessionTable)
#include "tst_sessiontable.moc"
