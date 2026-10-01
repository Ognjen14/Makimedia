#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "Library/SubtitleFinder.h"

namespace {

bool touch(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write("1\n00:00:01,000 --> 00:00:02,000\nx\n");
    return true;
}

QStringList namesOf(const QStringList &paths, const QString &root)
{
    QStringList names;
    for (const QString &path : paths) {
        names.append(QDir(root).relativeFilePath(path));
    }
    names.sort();
    return names;
}

bool isVideo(const QString &name)
{
    const QString suffix = QFileInfo(name).suffix().toLower();
    return suffix == QLatin1String("mkv") || suffix == QLatin1String("mp4");
}

}

class TestSubtitleFinder : public QObject
{
    Q_OBJECT

private slots:
    void theOnlyFilmInAFolderTakesItsSubsFolder();
    void twoFilmsInAFolderShareNothingByAccident();
    void aMissingFilmHasNoSubtitles();
};

void TestSubtitleFinder::theOnlyFilmInAFolderTakesItsSubsFolder()
{
    QTemporaryDir dir;
    const QString root = dir.path();
    for (const QString &name : { QStringLiteral("Heat.mkv"), QStringLiteral("Heat.srt"),
                                 QStringLiteral("Heat.en.srt"), QStringLiteral("Heatwave.srt"),
                                 QStringLiteral("notes.txt"), QStringLiteral("Subs/English.srt"),
                                 QStringLiteral("Subs/Heat/2_English.srt"),
                                 QStringLiteral("Other/Heat.srt") }) {
        QVERIFY(touch(root + QLatin1Char('/') + name));
    }

    const QStringList found = SubtitleFinder::onDisk(root + QStringLiteral("/Heat.mkv"), isVideo);
    QCOMPARE(namesOf(found, root),
             QStringList({ QStringLiteral("Heat.en.srt"), QStringLiteral("Heat.srt"),
                           QStringLiteral("Subs/English.srt"),
                           QStringLiteral("Subs/Heat/2_English.srt") }));
}

void TestSubtitleFinder::twoFilmsInAFolderShareNothingByAccident()
{
    QTemporaryDir dir;
    const QString root = dir.path();
    for (const QString &name : { QStringLiteral("Alien.mkv"), QStringLiteral("Aliens.mkv"),
                                 QStringLiteral("Alien.srt"), QStringLiteral("Aliens.srt"),
                                 QStringLiteral("Subs/English.srt") }) {
        QVERIFY(touch(root + QLatin1Char('/') + name));
    }

    const QStringList found = SubtitleFinder::onDisk(root + QStringLiteral("/Alien.mkv"), isVideo);
    QCOMPARE(namesOf(found, root), QStringList({ QStringLiteral("Alien.srt") }));
}

void TestSubtitleFinder::aMissingFilmHasNoSubtitles()
{
    QTemporaryDir dir;
    QVERIFY(touch(dir.path() + QStringLiteral("/Gone.srt")));
    QVERIFY(SubtitleFinder::onDisk(dir.path() + QStringLiteral("/Gone.mkv"), isVideo).isEmpty());
}

QTEST_GUILESS_MAIN(TestSubtitleFinder)
#include "tst_subtitlefinder.moc"
