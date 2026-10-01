#include <QtTest>

#include <QStringList>

#include "Platform/MediaFileInfo.h"

class TestPackagedFormats : public QObject
{
    Q_OBJECT

private slots:
    void theInstallerOffersEveryFormatTheScannerReads();
};

void TestPackagedFormats::theInstallerOffersEveryFormatTheScannerReads()
{
    const QStringList packaged =
        QString::fromLatin1(MM_PACKAGED_VIDEO_SUFFIXES)
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList scanned = MediaFormats::videoSuffixes();

    QStringList sortedPackaged = packaged;
    sortedPackaged.sort();
    scanned.sort();

    QCOMPARE(sortedPackaged, scanned);
}

QTEST_GUILESS_MAIN(TestPackagedFormats)

#include "tst_packagedformats.moc"
