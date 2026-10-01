#include <QtTest>

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <QUuid>

#include <atomic>
#include <memory>

#include "SingleInstance.h"

namespace {

QString uniqueKey()
{
    return QStringLiteral("makimedia-test-%1")
        .arg(QUuid::createUuid().toString(QUuid::Id128));
}

QString aLongNonAsciiPath()
{
    return QStringLiteral("D:/Filmovi/")
        + QStringLiteral("Čuvari Đorđevih žena, šest ćelija ").repeated(12)
        + QStringLiteral("Žene.S01E01.mkv");
}

struct HandOverResult
{
    bool wasPrimary = true;
    bool handedOver = false;
};

HandOverResult handOverFromAnotherThread(const QString &key, const QString &path)
{
    std::atomic_bool wasPrimary{true};
    std::atomic_bool handedOver{false};

    std::unique_ptr<QThread> thread(QThread::create([&]() {
        SingleInstance secondary(key);
        wasPrimary = secondary.isPrimary();
        handedOver = !wasPrimary && secondary.handOver(path);
    }));
    thread->start();

    QElapsedTimer waited;
    waited.start();
    while (!thread->wait(10) && waited.elapsed() < 10000) {
        QCoreApplication::processEvents();
    }
    thread->wait();
    QCoreApplication::processEvents();

    HandOverResult result;
    result.wasPrimary = wasPrimary;
    result.handedOver = handedOver;
    return result;
}

}

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void theFirstInstanceIsThePrimary();
    void aSecondInstanceHandsOverTheWholePath();
    void anEmptyHandOverStillReachesThePrimary();
    void handOversInARowEachArriveWhole();
    void aPrimaryCannotHandOver();
};

void TestSingleInstance::theFirstInstanceIsThePrimary()
{
    const QString key = uniqueKey();
    SingleInstance primary(key);
    QVERIFY(primary.isPrimary());

    SingleInstance secondary(key);
    QVERIFY(!secondary.isPrimary());
}

void TestSingleInstance::aSecondInstanceHandsOverTheWholePath()
{
    const QString key = uniqueKey();
    const QString path = aLongNonAsciiPath();
    QVERIFY(path.size() > 400);

    SingleInstance primary(key);
    QSignalSpy opened(&primary, &SingleInstance::openRequested);

    const HandOverResult result = handOverFromAnotherThread(key, path);
    QVERIFY(!result.wasPrimary);
    QVERIFY(result.handedOver);

    QTRY_COMPARE_WITH_TIMEOUT(opened.count(), 1, 3000);
    QCOMPARE(opened.first().first().toString(), path);
}

void TestSingleInstance::anEmptyHandOverStillReachesThePrimary()
{
    const QString key = uniqueKey();
    SingleInstance primary(key);
    QSignalSpy opened(&primary, &SingleInstance::openRequested);

    QVERIFY(handOverFromAnotherThread(key, QString()).handedOver);

    QTRY_COMPARE_WITH_TIMEOUT(opened.count(), 1, 3000);
    QVERIFY(opened.first().first().toString().isEmpty());
}

void TestSingleInstance::handOversInARowEachArriveWhole()
{
    const QString key = uniqueKey();
    SingleInstance primary(key);
    QSignalSpy opened(&primary, &SingleInstance::openRequested);

    const QString first = QStringLiteral("F:/Video Player Test/Žene.S01E01.mkv");
    const QString second = aLongNonAsciiPath();

    QVERIFY(handOverFromAnotherThread(key, first).handedOver);
    QTRY_COMPARE_WITH_TIMEOUT(opened.count(), 1, 3000);

    QVERIFY(handOverFromAnotherThread(key, second).handedOver);
    QTRY_COMPARE_WITH_TIMEOUT(opened.count(), 2, 3000);

    QCOMPARE(opened.at(0).first().toString(), first);
    QCOMPARE(opened.at(1).first().toString(), second);
}

void TestSingleInstance::aPrimaryCannotHandOver()
{
    SingleInstance primary(uniqueKey());
    QVERIFY(primary.isPrimary());
    QVERIFY(!primary.handOver(QStringLiteral("F:/a.mkv")));
}

QTEST_GUILESS_MAIN(TestSingleInstance)

#include "tst_singleinstance.moc"
