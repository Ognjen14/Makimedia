#include <QtTest>

#include "Library/PreviewPick.h"

namespace {

EpisodeRecord episode(int season, int number, bool watched = false, bool hasFile = true)
{
    EpisodeRecord record;
    record.season = season;
    record.episode = number;
    record.watched = watched;
    if (hasFile) {
        record.fileId = season * 100 + number;
        record.fileHandle = QStringLiteral("file-%1-%2").arg(season).arg(number);
    }
    return record;
}

}

class TestPreviewPick : public QObject
{
    Q_OBJECT

private slots:
    void theStartStaysInTheFirstHalf();
    void anUnknownLengthStartsAtTwoMinutes();
    void aNewShowPreviewsTheFirstSeason();
    void aStartedShowPreviewsTheNextEpisode();
    void episodesWithoutFilesAreSkipped();
    void nothingPlayableGivesNoEpisode();
};

void TestPreviewPick::theStartStaysInTheFirstHalf()
{
    QCOMPARE(PreviewPick::startSeconds(6000.0, 0.0), 900.0);
    QCOMPARE(PreviewPick::startSeconds(6000.0, 1.0), 3000.0);
    const double middle = PreviewPick::startSeconds(6000.0, 0.5);
    QVERIFY(middle > 900.0 && middle < 3000.0);
}

void TestPreviewPick::anUnknownLengthStartsAtTwoMinutes()
{
    QCOMPARE(PreviewPick::startSeconds(0.0, 0.7), 120.0);
}

void TestPreviewPick::aNewShowPreviewsTheFirstSeason()
{
    const QList<EpisodeRecord> episodes = {
        episode(0, 1), episode(1, 1), episode(1, 2), episode(2, 1), episode(2, 2),
    };
    for (const double chance : {0.0, 0.3, 0.6, 0.99, 1.0}) {
        const int at = PreviewPick::episodeIndex(episodes, chance);
        QCOMPARE(episodes.at(at).season, 1);
    }
}

void TestPreviewPick::aStartedShowPreviewsTheNextEpisode()
{
    const QList<EpisodeRecord> episodes = {
        episode(1, 1, true), episode(1, 2, true), episode(1, 3), episode(1, 4),
    };
    QCOMPARE(PreviewPick::episodeIndex(episodes, 0.9), 2);
}

void TestPreviewPick::episodesWithoutFilesAreSkipped()
{
    const QList<EpisodeRecord> episodes = {
        episode(1, 1, false, false), episode(1, 2),
    };
    QCOMPARE(PreviewPick::episodeIndex(episodes, 0.0), 1);
}

void TestPreviewPick::nothingPlayableGivesNoEpisode()
{
    QCOMPARE(PreviewPick::episodeIndex({episode(1, 1, false, false)}, 0.5), -1);
}

QTEST_GUILESS_MAIN(TestPreviewPick)
#include "tst_previewpick.moc"
