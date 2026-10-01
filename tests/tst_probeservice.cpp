#include <QtTest>

#include "Library/ProbeService.h"
#include "Platform/IMediaSource.h"

namespace {

const QString kFile = QStringLiteral("F:/Media/a.mkv");

class NoUrlSource : public IMediaSource
{
public:
    int urlRequests = 0;

    QString mpvUrl(const QString &) override
    {
        ++urlRequests;
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

class TestProbeService : public QObject
{
    Q_OBJECT

private slots:
    void switchingOffForgetsWhatWasOnlyWaiting();
    void aFailedProbeWaitsForTheNextScan();
    void aDisabledServiceTakesNoRequests();
    void anUnresolvedFileIsReportedFromTheEventLoop();
    void unresolvedFilesAreReportedOneAtATime();
    void aFileAlreadyWaitingIsNotQueuedTwice();

private:
    void countProbes(ProbeService &service, int *total, int *failed);
};

void TestProbeService::countProbes(ProbeService &service, int *total, int *failed)
{
    connect(&service, &ProbeService::probed, this,
            [total, failed](const ProbeInfo &info) {
        ++*total;
        if (!info.valid) {
            ++*failed;
        }
    });
}

void TestProbeService::switchingOffForgetsWhatWasOnlyWaiting()
{
    NoUrlSource source;
    ProbeService service;
    service.setMediaSource(&source);
    int total = 0;
    int failed = 0;
    countProbes(service, &total, &failed);

    service.setDeferred(true);
    service.request(kFile, kFile);
    QCOMPARE(service.pending(), 1);

    service.setEnabled(false);
    QCOMPARE(service.pending(), 0);

    service.setEnabled(true);
    service.request(kFile, kFile);
    QCOMPARE(service.pending(), 1);

    service.setDeferred(false);
    QCOMPARE(source.urlRequests, 1);
    QTRY_COMPARE(total, 1);
    QCOMPARE(failed, 1);
    QCOMPARE(service.pending(), 0);
}

void TestProbeService::aFailedProbeWaitsForTheNextScan()
{
    NoUrlSource source;
    ProbeService service;
    service.setMediaSource(&source);
    int total = 0;
    int failed = 0;
    countProbes(service, &total, &failed);

    service.requestAll({ProbeRequest{kFile, kFile}});
    QCOMPARE(source.urlRequests, 1);
    QTRY_COMPARE(failed, 1);

    service.request(kFile, kFile);
    QCOMPARE(source.urlRequests, 1);

    service.requestAll({ProbeRequest{kFile, kFile}});
    QCOMPARE(source.urlRequests, 2);
    QTRY_COMPARE(failed, 2);
}

void TestProbeService::aDisabledServiceTakesNoRequests()
{
    NoUrlSource source;
    ProbeService service;
    service.setMediaSource(&source);

    service.setEnabled(false);
    service.request(kFile, kFile);
    QCOMPARE(service.pending(), 0);
    QCOMPARE(source.urlRequests, 0);

    service.setEnabled(true);
    service.request(kFile, kFile);
    QCOMPARE(source.urlRequests, 1);
}

void TestProbeService::anUnresolvedFileIsReportedFromTheEventLoop()
{
    NoUrlSource source;
    ProbeService service;
    service.setMediaSource(&source);
    int total = 0;
    int failed = 0;
    countProbes(service, &total, &failed);

    service.request(kFile, kFile);
    QCOMPARE(source.urlRequests, 1);
    QCOMPARE(total, 0);
    QCOMPARE(service.pending(), 1);

    QTRY_COMPARE(total, 1);
    QCOMPARE(failed, 1);
    QCOMPARE(service.pending(), 0);
}

void TestProbeService::unresolvedFilesAreReportedOneAtATime()
{
    NoUrlSource source;
    ProbeService service;
    service.setMediaSource(&source);
    int total = 0;
    int failed = 0;
    countProbes(service, &total, &failed);

    service.requestAll({ProbeRequest{QStringLiteral("F:/Media/a.mkv"), QStringLiteral("F:/Media/a.mkv")},
                        ProbeRequest{QStringLiteral("F:/Media/b.mkv"), QStringLiteral("F:/Media/b.mkv")},
                        ProbeRequest{QStringLiteral("F:/Media/c.mkv"), QStringLiteral("F:/Media/c.mkv")}});
    QCOMPARE(source.urlRequests, 1);
    QCOMPARE(total, 0);
    QCOMPARE(service.pending(), 3);

    QTRY_COMPARE(total, 3);
    QCOMPARE(failed, 3);
    QCOMPARE(source.urlRequests, 3);
    QCOMPARE(service.pending(), 0);
}

void TestProbeService::aFileAlreadyWaitingIsNotQueuedTwice()
{
    NoUrlSource source;
    ProbeService service;
    service.setMediaSource(&source);
    int total = 0;
    int failed = 0;
    countProbes(service, &total, &failed);

    service.setDeferred(true);
    service.requestAll({ProbeRequest{kFile, kFile}});
    service.requestAll({ProbeRequest{kFile, kFile}});
    QCOMPARE(service.pending(), 1);

    service.setDeferred(false);
    QTRY_COMPARE(total, 1);
    QCOMPARE(source.urlRequests, 1);
    QCOMPARE(service.pending(), 0);

    service.requestAll({ProbeRequest{kFile, kFile}});
    QTRY_COMPARE(total, 2);
    QCOMPARE(source.urlRequests, 2);
}

QTEST_GUILESS_MAIN(TestProbeService)

#include "tst_probeservice.moc"
