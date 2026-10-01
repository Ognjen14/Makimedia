#include <QtTest>

#include <QSignalSpy>
#include <QStandardPaths>

#include "Library/ThumbnailService.h"
#include "Platform/IMediaSource.h"

namespace {

const QString kFile = QStringLiteral("F:/Media/a.mkv");
const QString kOther = QStringLiteral("F:/Media/b.mkv");

class NoUrlSource : public IMediaSource
{
public:
    QStringList asked;

    QString mpvUrl(const QString &handle) override
    {
        asked.append(handle);
        return QString();
    }
    bool exists(const QString &) const override { return true; }
    MediaFileInfo info(const QString &) const override { return MediaFileInfo(); }
    QString displayPath(const QString &handle) const override { return handle; }
    QStringList siblingSubtitles(const QString &) const override { return {}; }
    QList<MediaFileInfo> listChildren(const QString &) const override { return {}; }
    QString parentOf(const QString &) const override { return QString(); }
    bool isRootAvailable(const QString &) const override { return true; }
};

}

class TestThumbnailService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void aFileWithNoUrlIsNotAskedAboutAgainStraightAway();
    void aStillWantedFileWithNoUrlIsTriedAgainAfterTheNextHold();
    void switchingBackOnTakesWhatIsStillWanted();
    void aDisabledServiceTakesNoRequests();
    void onlyWhatIsStillWantedIsGenerated();
    void aFileDroppedFromTheListsCanBeWantedAgain();
    void aFileMatchedDuringTheHoldIsNotGenerated();

    void wantingTheSameFileTwiceAsksOnce();
    void anUnwantedHeldFileIsNotGenerated();
    void aFileWantedAgainIsAskedAboutOnce();
    void unwantingWhatWasNeverWantedDoesNothing();
};

void TestThumbnailService::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestThumbnailService::aFileWithNoUrlIsNotAskedAboutAgainStraightAway()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);
    QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);

    service.setWanted({kFile});
    QCOMPARE(source.asked.size(), 1);

    service.setWanted({kFile});
    service.setWanted({kFile});
    QCOMPARE(source.asked.size(), 1);
    QCOMPARE(ready.count(), 0);
}

void TestThumbnailService::aStillWantedFileWithNoUrlIsTriedAgainAfterTheNextHold()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setWanted({kFile});
    QCOMPARE(source.asked.size(), 1);

    service.setDeferred(true);
    service.setDeferred(false);
    QCOMPARE(source.asked.size(), 2);
}

void TestThumbnailService::switchingBackOnTakesWhatIsStillWanted()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.setWanted({kFile});
    QCOMPARE(source.asked.size(), 0);

    service.setEnabled(false);
    service.setDeferred(false);
    QCOMPARE(source.asked.size(), 0);

    service.setEnabled(true);
    QCOMPARE(source.asked, QStringList{kFile});
}

void TestThumbnailService::aDisabledServiceTakesNoRequests()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setEnabled(false);
    service.setWanted({kFile});
    QCOMPARE(source.asked.size(), 0);

    service.setEnabled(true);
    QCOMPARE(source.asked.size(), 1);
}

void TestThumbnailService::onlyWhatIsStillWantedIsGenerated()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.setWanted({kFile, kOther});
    service.setWanted({kOther});
    service.setDeferred(false);

    QCOMPARE(source.asked, QStringList{kOther});
}

void TestThumbnailService::aFileDroppedFromTheListsCanBeWantedAgain()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.setWanted({kFile});
    service.setWanted({});
    service.setWanted({kFile});
    service.setDeferred(false);

    QCOMPARE(source.asked, QStringList{kFile});
}

void TestThumbnailService::aFileMatchedDuringTheHoldIsNotGenerated()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.setWanted({kFile});
    service.discard(kFile);
    service.setDeferred(false);
    QCOMPARE(source.asked.size(), 0);

    service.setWanted({kFile});
    QCOMPARE(source.asked, QStringList{kFile});
}

void TestThumbnailService::wantingTheSameFileTwiceAsksOnce()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.want({kFile});
    service.want({kFile});
    QCOMPARE(source.asked, QStringList{kFile});
}

void TestThumbnailService::anUnwantedHeldFileIsNotGenerated()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.want({kFile, kOther});
    service.unwant({kFile});
    service.setDeferred(false);

    QCOMPARE(source.asked, QStringList{kOther});
}

void TestThumbnailService::aFileWantedAgainIsAskedAboutOnce()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.want({kFile});
    service.unwant({kFile});
    service.want({kFile});
    service.setDeferred(false);

    QCOMPARE(source.asked, QStringList{kFile});
}

void TestThumbnailService::unwantingWhatWasNeverWantedDoesNothing()
{
    NoUrlSource source;
    ThumbnailService service;
    service.setMediaSource(&source);

    service.setDeferred(true);
    service.want({kFile});
    service.unwant({kOther});
    service.unwant(QStringList());
    service.setDeferred(false);

    QCOMPARE(source.asked, QStringList{kFile});
}

QTEST_GUILESS_MAIN(TestThumbnailService)

#include "tst_thumbnailservice.moc"
