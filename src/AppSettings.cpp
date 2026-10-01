#include "AppSettings.h"

#include "MmLog.h"

#include <QUuid>

AppSettings::AppSettings(QObject *parent)
    : AppSettings(QSettings::NativeFormat,
                  QStringLiteral("TopicDev"),
                  QStringLiteral("Makimedia"),
                  parent)
{
}

AppSettings::AppSettings(QSettings::Format format,
                         const QString &organisation,
                         const QString &application,
                         QObject *parent)
    : QObject(parent)
    , m_settings(format, QSettings::UserScope, organisation, application)
{
}

bool AppSettings::accentChosen() const
{
    return m_settings.contains(QStringLiteral("accentIndex"));
}

int AppSettings::accentIndex() const
{
    return m_settings.value(QStringLiteral("accentIndex"), 11).toInt();
}

void AppSettings::setAccentIndex(int index)
{
    if (accentIndex() == index) {
        return;
    }
    m_settings.setValue(QStringLiteral("accentIndex"), index);
    emit accentIndexChanged();
}

double AppSettings::fontSizeScale() const
{
    return m_settings.value(QStringLiteral("fontSize"), 1.0).toDouble();
}

void AppSettings::setFontSizeScale(double scale)
{
    if (qFuzzyCompare(fontSizeScale(), scale)) {
        return;
    }
    m_settings.setValue(QStringLiteral("fontSize"), scale);
    emit fontSizeScaleChanged();
}

int AppSettings::skipIntervalSeconds() const
{
    return m_settings.value(QStringLiteral("skipIntervalSeconds"), 10).toInt();
}

void AppSettings::setSkipIntervalSeconds(int seconds)
{
    if (skipIntervalSeconds() == seconds) {
        return;
    }
    m_settings.setValue(QStringLiteral("skipIntervalSeconds"), seconds);
    emit skipIntervalSecondsChanged();
}

int AppSettings::playerVolume() const
{
    return qBound(0, m_settings.value(QStringLiteral("playerVolume"), 100).toInt(), 130);
}

void AppSettings::setPlayerVolume(int volume)
{
    const int clamped = qBound(0, volume, 130);
    if (playerVolume() == clamped) {
        return;
    }
    m_settings.setValue(QStringLiteral("playerVolume"), clamped);
    emit playerVolumeChanged();
}

double AppSettings::holdToSpeedMultiplier() const
{
    return m_settings.value(QStringLiteral("holdToSpeedMultiplier"), 1.5).toDouble();
}

void AppSettings::setHoldToSpeedMultiplier(double multiplier)
{
    if (qFuzzyCompare(holdToSpeedMultiplier(), multiplier)) {
        return;
    }
    m_settings.setValue(QStringLiteral("holdToSpeedMultiplier"), multiplier);
    emit holdToSpeedMultiplierChanged();
}

bool AppSettings::keepScreenOn() const
{
    return m_settings.value(QStringLiteral("keepScreenOn"), true).toBool();
}

void AppSettings::setKeepScreenOn(bool value)
{
    if (keepScreenOn() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("keepScreenOn"), value);
    emit keepScreenOnChanged();
}

bool AppSettings::autoPlayNextEpisode() const
{
    return m_settings.value(QStringLiteral("autoPlayNextEpisode"), true).toBool();
}

void AppSettings::setAutoPlayNextEpisode(bool value)
{
    if (autoPlayNextEpisode() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("autoPlayNextEpisode"), value);
    MM_LOG_I() << "auto-play next episode ->" << value;
    emit autoPlayNextEpisodeChanged();
}

bool AppSettings::closeToTray() const
{
    return m_settings.value(QStringLiteral("closeToTray"), true).toBool();
}

void AppSettings::setCloseToTray(bool value)
{
    if (closeToTray() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("closeToTray"), value);
    emit closeToTrayChanged();
}

bool AppSettings::trayNoticeSeen() const
{
    return m_settings.value(QStringLiteral("trayNoticeSeen"), false).toBool();
}

void AppSettings::setTrayNoticeSeen(bool value)
{
    if (trayNoticeSeen() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("trayNoticeSeen"), value);
    emit trayNoticeSeenChanged();
}

bool AppSettings::forceHardwareDecoding() const
{
    return m_settings.value(QStringLiteral("forceHardwareDecoding"), false).toBool();
}

void AppSettings::setForceHardwareDecoding(bool value)
{
    if (forceHardwareDecoding() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("forceHardwareDecoding"), value);
    emit forceHardwareDecodingChanged();
}

int AppSettings::subtitleScalePercent() const
{
    return m_settings.value(QStringLiteral("subtitleScalePercent"), 100).toInt();
}

void AppSettings::setSubtitleScalePercent(int value)
{
    if (subtitleScalePercent() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleScalePercent"), value);
    emit subtitleScalePercentChanged();
}

QString AppSettings::subtitleEdgeStyle() const
{
    return m_settings.value(QStringLiteral("subtitleEdgeStyle"), QStringLiteral("outline")).toString();
}

void AppSettings::setSubtitleEdgeStyle(const QString &value)
{
    if (subtitleEdgeStyle() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleEdgeStyle"), value);
    emit subtitleEdgeStyleChanged();
}

QString AppSettings::subtitlePosition() const
{
    return m_settings.value(QStringLiteral("subtitlePosition"), QStringLiteral("bottom")).toString();
}

void AppSettings::setSubtitlePosition(const QString &value)
{
    if (subtitlePosition() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitlePosition"), value);
    emit subtitlePositionChanged();
}

QString AppSettings::subtitleColor() const
{
    return m_settings.value(QStringLiteral("subtitleColor"),
                            QStringLiteral("#FFFFFFFF")).toString();
}

void AppSettings::setSubtitleColor(const QString &value)
{
    if (subtitleColor() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleColor"), value);
    emit subtitleColorChanged();
}

bool AppSettings::subtitleBold() const
{
    return m_settings.value(QStringLiteral("subtitleBold"), false).toBool();
}

void AppSettings::setSubtitleBold(bool value)
{
    if (subtitleBold() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleBold"), value);
    emit subtitleBoldChanged();
}

QString AppSettings::subtitleLanguage() const
{
    return m_settings.value(QStringLiteral("subtitleLanguage"),
                            QStringLiteral("en")).toString();
}

void AppSettings::setSubtitleLanguage(const QString &value)
{
    if (subtitleLanguage() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleLanguage"), value);
    emit subtitleLanguageChanged();
}

QString AppSettings::subtitleAccount() const
{
    return m_settings.value(QStringLiteral("subtitleAccount")).toString();
}

void AppSettings::setSubtitleAccount(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (subtitleAccount() == trimmed) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleAccount"), trimmed);
    MM_LOG_I() << "subtitle account set to"
               << (trimmed.isEmpty() ? QStringLiteral("nothing") : trimmed);
    emit subtitleAccountChanged();
}

QString AppSettings::subtitlePassword() const
{
    return m_settings.value(QStringLiteral("subtitlePassword")).toString();
}

void AppSettings::setSubtitlePassword(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (subtitlePassword() == trimmed) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitlePassword"), trimmed);
    MM_LOG_I() << "subtitle password" << (trimmed.isEmpty() ? "cleared" : "set")
               << "-" << trimmed.size() << "characters";
    emit subtitleAccountChanged();
}

QStringList AppSettings::subtitleSearchLanguages() const
{
    const QStringList stored =
        m_settings.value(QStringLiteral("subtitleSearchLanguages")).toStringList();
    return stored.isEmpty() ? QStringList{ QStringLiteral("en") } : stored;
}

void AppSettings::setSubtitleSearchLanguages(const QStringList &value)
{
    if (subtitleSearchLanguages() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitleSearchLanguages"), value);
    MM_LOG_I() << "subtitles are searched for in" << value;
    emit subtitleSearchLanguagesChanged();
}

QString AppSettings::audioLanguage() const
{
    return m_settings.value(QStringLiteral("audioLanguage"), QStringLiteral("en")).toString();
}

void AppSettings::setAudioLanguage(const QString &value)
{
    if (audioLanguage() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("audioLanguage"), value);
    emit audioLanguageChanged();
}

bool AppSettings::subtitlesOnByDefault() const
{
    return m_settings.value(QStringLiteral("subtitlesOnByDefault"), true).toBool();
}

void AppSettings::setSubtitlesOnByDefault(bool value)
{
    if (subtitlesOnByDefault() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("subtitlesOnByDefault"), value);
    emit subtitlesOnByDefaultChanged();
}

QString AppSettings::playerControlScheme() const
{
    return m_settings.value(QStringLiteral("playerControlScheme"),
                            QStringLiteral("auto")).toString();
}

void AppSettings::setPlayerControlScheme(const QString &value)
{
    if (playerControlScheme() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("playerControlScheme"), value);
    MM_LOG_I() << "player control scheme set to" << value;
    emit playerControlSchemeChanged();
}

bool AppSettings::rememberTrackPerShow() const
{
    return m_settings.value(QStringLiteral("rememberTrackPerShow"), true).toBool();
}

void AppSettings::setRememberTrackPerShow(bool value)
{
    if (rememberTrackPerShow() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("rememberTrackPerShow"), value);
    emit rememberTrackPerShowChanged();
}

int AppSettings::windowX() const
{
    return m_settings.value(QStringLiteral("windowX"), -1).toInt();
}

void AppSettings::setWindowX(int value)
{
    if (windowX() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("windowX"), value);
    emit windowXChanged();
}

int AppSettings::windowY() const
{
    return m_settings.value(QStringLiteral("windowY"), -1).toInt();
}

void AppSettings::setWindowY(int value)
{
    if (windowY() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("windowY"), value);
    emit windowYChanged();
}

int AppSettings::windowWidth() const
{
    return m_settings.value(QStringLiteral("windowWidth"), 1280).toInt();
}

void AppSettings::setWindowWidth(int value)
{
    if (windowWidth() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("windowWidth"), value);
    emit windowWidthChanged();
}

int AppSettings::windowHeight() const
{
    return m_settings.value(QStringLiteral("windowHeight"), 800).toInt();
}

void AppSettings::setWindowHeight(int value)
{
    if (windowHeight() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("windowHeight"), value);
    emit windowHeightChanged();
}

bool AppSettings::windowMaximized() const
{
    return m_settings.value(QStringLiteral("windowMaximized"), false).toBool();
}

void AppSettings::setWindowMaximized(bool value)
{
    if (windowMaximized() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("windowMaximized"), value);
    emit windowMaximizedChanged();
}

bool AppSettings::treatAsTelevision() const
{
    return m_settings.value(QStringLiteral("treatAsTelevision"), false).toBool();
}

void AppSettings::setTreatAsTelevision(bool value)
{
    if (treatAsTelevision() == value) {
        return;
    }
    m_settings.setValue(QStringLiteral("treatAsTelevision"), value);
    MM_LOG_I() << "treat this device as a television set to" << value
               << "- takes effect at the next launch";
    emit treatAsTelevisionChanged();
}

QStringList AppSettings::recentSearches() const
{
    return m_settings.value(QStringLiteral("recentSearches")).toStringList();
}

void AppSettings::noteSearch(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.length() < 2) {
        return;
    }

    QStringList recent = recentSearches();
    for (int i = recent.size() - 1; i >= 0; --i) {
        if (recent.at(i).compare(trimmed, Qt::CaseInsensitive) == 0) {
            recent.removeAt(i);
        }
    }
    recent.prepend(trimmed);
    while (recent.size() > 6) {
        recent.removeLast();
    }

    m_settings.setValue(QStringLiteral("recentSearches"), recent);
    emit recentSearchesChanged();
}

void AppSettings::forgetSearches()
{
    if (recentSearches().isEmpty()) {
        return;
    }
    m_settings.remove(QStringLiteral("recentSearches"));
    emit recentSearchesChanged();
}

QString AppSettings::streamingServerId()
{
    QString id = m_settings.value(QStringLiteral("streaming/serverId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue(QStringLiteral("streaming/serverId"), id);
        MM_LOG_I() << "this PC is streaming server" << id << "from now on";
    }
    return id;
}

QString AppSettings::streamingDeviceId()
{
    QString id = m_settings.value(QStringLiteral("streaming/deviceId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue(QStringLiteral("streaming/deviceId"), id);
        MM_LOG_I() << "this device is streaming client" << id << "from now on";
    }
    return id;
}

QString AppSettings::streamingServerName() const
{
    const QString name = m_settings.value(QStringLiteral("streaming/serverName")).toString();
    if (!name.isEmpty()) {
        return name;
    }
    return tr("Makimedia server");
}

bool AppSettings::treatAsTelevisionAtLaunch()
{
    return AppSettings().treatAsTelevision();
}
