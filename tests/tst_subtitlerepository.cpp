#include <QtTest>

#include <QScopedPointer>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Data/Database.h"
#include "Data/SubtitleRepository.h"

namespace {

const QString kFilm = QStringLiteral("F:/Media/film.mkv");
const QString kEnglish = QStringLiteral("F:/Media/film.en.srt");
const QString kForced = QStringLiteral("F:/Media/film.en.forced.srt");
const QString kOutside = QStringLiteral("D:/Downloads/clip.mkv");

QStringList handlesOf(const QList<ExternalSubtitle> &subtitles)
{
    QStringList handles;
    for (const ExternalSubtitle &subtitle : subtitles) {
        handles.append(subtitle.subHandle);
    }
    return handles;
}

}

class TestSubtitleRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void attachingTheSameFileTwiceUpdatesInsteadOfDuplicating();
    void subtitlesComeBackInTheOrderTheyWereAdded();
    void detachRemovesOneAndDetachAllTheRest();
    void aFileLeavingTheLibraryTakesItsSubtitles();
    void aSubtitleWithNoLanguageIsStillSaved();

private:
    bool addLibraryFile();

    QScopedPointer<QTemporaryDir> m_dir;
    QScopedPointer<Database> m_database;
    QScopedPointer<SubtitleRepository> m_subtitles;
};

void TestSubtitleRepository::init()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    m_database.reset(new Database);
    QVERIFY(m_database->open(m_dir->path() + QStringLiteral("/library.sqlite")));
    QVERIFY(addLibraryFile());

    m_subtitles.reset(new SubtitleRepository(*m_database));
}

void TestSubtitleRepository::cleanup()
{
    m_subtitles.reset();
    m_database.reset();
    m_dir.reset();
}

bool TestSubtitleRepository::addLibraryFile()
{
    QSqlQuery folder(m_database->handle());
    if (!folder.exec(QStringLiteral(
            "INSERT INTO folders (id, handle, display_name, added)"
            " VALUES (1, 'F:/Media', 'Media', 1)"))) {
        return false;
    }

    QSqlQuery file(m_database->handle());
    file.prepare(QStringLiteral(
        "INSERT INTO files (folder_id, handle, display_name, added)"
        " VALUES (1, :handle, 'film.mkv', 1)"));
    file.bindValue(QStringLiteral(":handle"), kFilm);
    return file.exec();
}

void TestSubtitleRepository::attachingTheSameFileTwiceUpdatesInsteadOfDuplicating()
{
    QVERIFY(m_subtitles->attach(kFilm, kEnglish, QStringLiteral("English"),
                                QStringLiteral("en")));
    QVERIFY(m_subtitles->attach(kFilm, kEnglish, QStringLiteral("English (SDH)"),
                                QStringLiteral("eng")));

    const QList<ExternalSubtitle> subtitles = m_subtitles->forFile(kFilm);
    QCOMPARE(subtitles.size(), 1);
    QCOMPARE(subtitles.first().fileHandle, kFilm);
    QCOMPARE(subtitles.first().subHandle, kEnglish);
    QCOMPARE(subtitles.first().displayName, QStringLiteral("English (SDH)"));
    QCOMPARE(subtitles.first().language, QStringLiteral("eng"));
}

void TestSubtitleRepository::subtitlesComeBackInTheOrderTheyWereAdded()
{
    QVERIFY(m_subtitles->attach(kFilm, kForced, QStringLiteral("English"),
                                QStringLiteral("en")));
    QVERIFY(m_subtitles->attach(kFilm, kEnglish, QStringLiteral("English"),
                                QStringLiteral("en")));

    QCOMPARE(handlesOf(m_subtitles->forFile(kFilm)), QStringList({kForced, kEnglish}));

    QVERIFY(m_subtitles->attach(kFilm, kForced, QStringLiteral("English (forced)"),
                                QStringLiteral("en")));
    QCOMPARE(handlesOf(m_subtitles->forFile(kFilm)), QStringList({kForced, kEnglish}));

    QVERIFY(m_subtitles->forFile(QStringLiteral("F:/Media/nothing.mkv")).isEmpty());
}

void TestSubtitleRepository::detachRemovesOneAndDetachAllTheRest()
{
    QVERIFY(m_subtitles->attach(kFilm, kEnglish, QStringLiteral("English"),
                                QStringLiteral("en")));
    QVERIFY(m_subtitles->attach(kFilm, kForced, QStringLiteral("English"),
                                QStringLiteral("en")));

    QVERIFY(m_subtitles->detach(kFilm, kEnglish));
    QCOMPARE(handlesOf(m_subtitles->forFile(kFilm)), QStringList({kForced}));

    QVERIFY(m_subtitles->detachAll(kFilm));
    QVERIFY(m_subtitles->forFile(kFilm).isEmpty());

    QVERIFY(m_subtitles->detachAll(kFilm));
    QVERIFY(m_subtitles->detachAll(QStringLiteral("F:/Media/nothing.mkv")));
    QVERIFY(m_subtitles->detach(kFilm, kEnglish));
}

void TestSubtitleRepository::aFileLeavingTheLibraryTakesItsSubtitles()
{
    QVERIFY(m_subtitles->attach(kFilm, kEnglish, QStringLiteral("English"),
                                QStringLiteral("en")));
    QVERIFY(m_subtitles->attach(kOutside, QStringLiteral("D:/Downloads/clip.srt"),
                                QStringLiteral("English"), QStringLiteral("en")));

    QSqlQuery remove(m_database->handle());
    QVERIFY(remove.exec(QStringLiteral("DELETE FROM folders WHERE id = 1")));

    QVERIFY(m_subtitles->forFile(kFilm).isEmpty());
    QCOMPARE(m_subtitles->forFile(kOutside).size(), 1);

    QVERIFY(addLibraryFile());
    QVERIFY(m_subtitles->forFile(kFilm).isEmpty());
}

void TestSubtitleRepository::aSubtitleWithNoLanguageIsStillSaved()
{
    const QString noLanguage = QStringLiteral("F:/Media/film.2021.HI.srt");

    QVERIFY(m_subtitles->attach(kFilm, noLanguage, QStringLiteral("film.2021.HI"),
                                QString()));
    QVERIFY(m_subtitles->attach(kFilm, kEnglish, QString(), QString()));

    const QList<ExternalSubtitle> subtitles = m_subtitles->forFile(kFilm);
    QCOMPARE(subtitles.size(), 2);
    QCOMPARE(subtitles.at(0).subHandle, noLanguage);
    QCOMPARE(subtitles.at(0).displayName, QStringLiteral("film.2021.HI"));
    QVERIFY(subtitles.at(0).language.isEmpty());
    QVERIFY(subtitles.at(1).displayName.isEmpty());
}

QTEST_GUILESS_MAIN(TestSubtitleRepository)

#include "tst_subtitlerepository.moc"
