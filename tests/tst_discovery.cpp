#include <QtTest>

#include <QHostAddress>
#include <QSignalSpy>

#include "Streaming/Discovery.h"
#include "Streaming/FoundServerModel.h"

class TestDiscovery : public QObject
{
    Q_OBJECT

private slots:
    void anAskIsRecognised();
    void anAnswerCarriesTheServer();
    void anAnswerInAnotherVersionIsIgnored();
    void somethingElseOnThePortIsIgnored();
    void aResponderAndABrowserFindEachOther();
    void aServerFoundTwiceIsListedOnce();
};

void TestDiscovery::anAskIsRecognised()
{
    QVERIFY(Discovery::isAsk(Discovery::askMessage()));
    QVERIFY(Discovery::isAsk(R"({"mm":"discover","v":1})"));
    QVERIFY(!Discovery::isAsk(R"({"mm":"discover","v":2})"));
    QVERIFY(!Discovery::isAsk(R"({"mm":"here","v":1})"));
}

void TestDiscovery::anAnswerCarriesTheServer()
{
    Discovery::Here here;
    here.serverId = QStringLiteral("0b3c-server");
    here.name = QStringLiteral("DESKTOP-ABC");
    here.port = 47811;

    const std::optional<Discovery::Here> read = Discovery::parseHere(Discovery::hereMessage(here));
    QVERIFY(read.has_value());
    QCOMPARE(read->serverId, here.serverId);
    QCOMPARE(read->name, here.name);
    QCOMPARE(read->port, here.port);
}

void TestDiscovery::anAnswerInAnotherVersionIsIgnored()
{
    QVERIFY(!Discovery::parseHere(
        R"({"mm":"here","v":2,"id":"x","name":"PC","port":47811})").has_value());
}

void TestDiscovery::somethingElseOnThePortIsIgnored()
{
    QVERIFY(!Discovery::parseHere("not json at all").has_value());
    QVERIFY(!Discovery::parseHere(R"({"mm":"here","v":1,"name":"PC","port":47811})").has_value());
    QVERIFY(!Discovery::parseHere(R"({"mm":"here","v":1,"id":"x","port":0})").has_value());
    QVERIFY(!Discovery::parseHere(R"({"mm":"here","v":1,"id":"x","port":70000})").has_value());
    QVERIFY(!Discovery::parseHere(QByteArray(2000, '{')).has_value());
}

void TestDiscovery::aResponderAndABrowserFindEachOther()
{
    const quint16 udpPort = 47890;

    Discovery::Here here;
    here.serverId = QStringLiteral("server-loopback");
    here.name = QStringLiteral("DESKTOP-LOOP");
    here.port = 47811;

    DiscoveryResponder responder;
    QVERIFY(responder.start(here, udpPort));

    DiscoveryBrowser browser;
    QSignalSpy found(&browser, &DiscoveryBrowser::found);
    browser.setExtraTargets({ QHostAddress(QHostAddress::LocalHost) });
    QVERIFY(browser.start(udpPort, 200));

    QVERIFY(found.wait(5000));
    const QList<QVariant> first = found.first();
    QCOMPARE(first.at(0).toString(), here.serverId);
    QCOMPARE(first.at(1).toString(), here.name);
    QVERIFY(!first.at(2).toString().isEmpty());
    QCOMPARE(first.at(3).value<quint16>(), here.port);

    browser.stop();
    responder.stop();
    QVERIFY(!browser.isRunning());
    QVERIFY(!responder.isRunning());
}

void TestDiscovery::aServerFoundTwiceIsListedOnce()
{
    FoundServerModel model;
    FoundServer server;
    server.serverId = QStringLiteral("a");
    server.name = QStringLiteral("DESKTOP-A");
    server.host = QStringLiteral("192.168.1.5");
    server.port = 47811;

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    QVERIFY(model.upsert(server));
    QVERIFY(!model.upsert(server));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(changed.count(), 0);

    server.host = QStringLiteral("192.168.1.9");
    QVERIFY(!model.upsert(server));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), FoundServerModel::HostRole).toString(),
             QStringLiteral("192.168.1.9"));

    model.setSelected(QStringLiteral("a"));
    QVERIFY(model.data(model.index(0, 0), FoundServerModel::SelectedRole).toBool());
    QCOMPARE(reset.count(), 0);
}

QTEST_GUILESS_MAIN(TestDiscovery)
#include "tst_discovery.moc"
