#include <QtTest>

#include <QScopedPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

#include "AppSettings.h"

namespace {

const QString kOrganisation = QStringLiteral("TopicDevTests");
const QString kApplication = QStringLiteral("MakimediaUnderTest");

QSettings theStore()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     kOrganisation, kApplication);
}

AppSettings freshSettings()
{
    return AppSettings(QSettings::IniFormat, kOrganisation, kApplication);
}

}

class TestAppSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void everyDefaultIsWhatTheAppExpects();
    void readingWritesNothing();

    void aWrittenValueComesBack();
    void aWrittenValueSurvivesANewInstance();

    void theVolumeIsClampedOnTheWayIn();
    void theVolumeIsClampedOnTheWayOut();

    void accentChosenFollowsWhatWasPicked();

    void aSetterAnnouncesItselfOnceAndOnlyOnChange();

private:
    QScopedPointer<QTemporaryDir> m_dir;
};

void TestAppSettings::initTestCase()
{
    m_dir.reset(new QTemporaryDir);
    QVERIFY2(m_dir->isValid(), qPrintable(m_dir->errorString()));

    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       m_dir->path());

    QSettings store = theStore();
    QVERIFY2(store.fileName().startsWith(m_dir->path()),
             qPrintable(QStringLiteral("refusing to run: settings would go to ")
                        + store.fileName()));

    {
        AppSettings settings = freshSettings();
        settings.setAccentIndex(7);
    }
    QVERIFY2(theStore().value(QStringLiteral("accentIndex")).toInt() == 7,
             "AppSettings did not write where the test is looking");
}

void TestAppSettings::init()
{
    QSettings store = theStore();
    store.clear();
    store.sync();

    QVERIFY(theStore().allKeys().isEmpty());
}

void TestAppSettings::everyDefaultIsWhatTheAppExpects()
{
    AppSettings settings = freshSettings();

    QCOMPARE(settings.accentIndex(), 0);
    QCOMPARE(settings.fontSizeScale(), 1.0);

    QCOMPARE(settings.skipIntervalSeconds(), 10);
    QCOMPARE(settings.playerVolume(), 100);
    QCOMPARE(settings.holdToSpeedMultiplier(), 1.5);
    QCOMPARE(settings.keepScreenOn(), true);
    QCOMPARE(settings.forceHardwareDecoding(), false);
    QCOMPARE(settings.closeToTray(), true);
    QCOMPARE(settings.trayNoticeSeen(), false);

    QCOMPARE(settings.subtitleScalePercent(), 100);
    QCOMPARE(settings.subtitleEdgeStyle(), QStringLiteral("outline"));
    QCOMPARE(settings.subtitlePosition(), QStringLiteral("bottom"));
    QCOMPARE(settings.subtitleColor(), QStringLiteral("#FFFFFFFF"));
    QCOMPARE(settings.subtitleBold(), false);
    QCOMPARE(settings.subtitleLanguage(), QStringLiteral("en"));
    QCOMPARE(settings.audioLanguage(), QStringLiteral("en"));
    QCOMPARE(settings.subtitlesOnByDefault(), true);
    QCOMPARE(settings.rememberTrackPerShow(), true);

    QCOMPARE(settings.playerControlScheme(), QStringLiteral("auto"));

    QCOMPARE(settings.windowX(), -1);
    QCOMPARE(settings.windowY(), -1);
    QCOMPARE(settings.windowWidth(), 1280);
    QCOMPARE(settings.windowHeight(), 800);
    QCOMPARE(settings.windowMaximized(), false);
    QCOMPARE(settings.treatAsTelevision(), false);

    QCOMPARE(settings.accentChosen(), false);
}

void TestAppSettings::readingWritesNothing()
{
    {
        AppSettings settings = freshSettings();
        settings.accentIndex();
        settings.accentChosen();
        settings.playerVolume();
        settings.subtitleLanguage();
        settings.windowWidth();
    }

    QVERIFY(theStore().allKeys().isEmpty());
}

void TestAppSettings::aWrittenValueComesBack()
{
    AppSettings settings = freshSettings();

    settings.setAccentIndex(3);
    settings.setFontSizeScale(1.25);
    settings.setSkipIntervalSeconds(30);
    settings.setSubtitleLanguage(QStringLiteral("hr"));
    settings.setWindowMaximized(true);
    settings.setWindowWidth(1920);
    settings.setTreatAsTelevision(true);

    QCOMPARE(settings.accentIndex(), 3);
    QCOMPARE(settings.accentChosen(), true);
    QCOMPARE(settings.fontSizeScale(), 1.25);
    QCOMPARE(settings.skipIntervalSeconds(), 30);
    QCOMPARE(settings.subtitleLanguage(), QStringLiteral("hr"));
    QCOMPARE(settings.windowMaximized(), true);
    QCOMPARE(settings.windowWidth(), 1920);
    QCOMPARE(settings.treatAsTelevision(), true);
}

void TestAppSettings::aWrittenValueSurvivesANewInstance()
{
    {
        AppSettings first = freshSettings();
        first.setAccentIndex(5);
        first.setSkipIntervalSeconds(30);
        first.setAudioLanguage(QStringLiteral("hr"));
        first.setWindowHeight(1000);
        first.setTreatAsTelevision(true);
    }

    AppSettings second = freshSettings();

    QCOMPARE(second.accentIndex(), 5);
    QCOMPARE(second.skipIntervalSeconds(), 30);
    QCOMPARE(second.audioLanguage(), QStringLiteral("hr"));
    QCOMPARE(second.windowHeight(), 1000);
    QCOMPARE(second.treatAsTelevision(), true);
}

void TestAppSettings::theVolumeIsClampedOnTheWayIn()
{
    AppSettings settings = freshSettings();

    settings.setPlayerVolume(200);
    QCOMPARE(settings.playerVolume(), 130);

    settings.setPlayerVolume(-5);
    QCOMPARE(settings.playerVolume(), 0);

    settings.setPlayerVolume(130);
    QCOMPARE(settings.playerVolume(), 130);

    settings.setPlayerVolume(65);
    QCOMPARE(settings.playerVolume(), 65);
}

void TestAppSettings::theVolumeIsClampedOnTheWayOut()
{
    {
        QSettings store = theStore();
        store.setValue(QStringLiteral("playerVolume"), 999);
        store.sync();
    }

    QCOMPARE(freshSettings().playerVolume(), 130);

    {
        QSettings store = theStore();
        store.setValue(QStringLiteral("playerVolume"), -50);
        store.sync();
    }

    QCOMPARE(freshSettings().playerVolume(), 0);
}

void TestAppSettings::accentChosenFollowsWhatWasPicked()
{
    QVERIFY(!freshSettings().accentChosen());

    {
        AppSettings settings = freshSettings();
        settings.setAccentIndex(11);
        QVERIFY(settings.accentChosen());
    }

    QVERIFY(freshSettings().accentChosen());

    {
        QSettings store = theStore();
        store.clear();
        store.sync();
    }

    QVERIFY(!freshSettings().accentChosen());
}

void TestAppSettings::aSetterAnnouncesItselfOnceAndOnlyOnChange()
{
    AppSettings settings = freshSettings();

    QSignalSpy accent(&settings, &AppSettings::accentIndexChanged);
    QSignalSpy volume(&settings, &AppSettings::playerVolumeChanged);
    QSignalSpy language(&settings, &AppSettings::subtitleLanguageChanged);

    QVERIFY(accent.isValid());
    QVERIFY(volume.isValid());
    QVERIFY(language.isValid());

    settings.setAccentIndex(3);
    QCOMPARE(accent.count(), 1);
    settings.setAccentIndex(3);
    QCOMPARE(accent.count(), 1);
    settings.setAccentIndex(4);
    QCOMPARE(accent.count(), 2);

    settings.setPlayerVolume(50);
    QCOMPARE(volume.count(), 1);
    settings.setPlayerVolume(50);
    QCOMPARE(volume.count(), 1);

    settings.setPlayerVolume(500);
    QCOMPARE(volume.count(), 2);
    QCOMPARE(settings.playerVolume(), 130);
    settings.setPlayerVolume(400);
    QCOMPARE(volume.count(), 2);

    settings.setSubtitleLanguage(QStringLiteral("hr"));
    QCOMPARE(language.count(), 1);
    settings.setSubtitleLanguage(QStringLiteral("hr"));
    QCOMPARE(language.count(), 1);
}

QTEST_GUILESS_MAIN(TestAppSettings)

#include "tst_appsettings.moc"
