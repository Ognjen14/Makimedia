#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

class AppSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int accentIndex READ accentIndex WRITE setAccentIndex NOTIFY accentIndexChanged FINAL)
    Q_PROPERTY(double fontSizeScale READ fontSizeScale WRITE setFontSizeScale NOTIFY fontSizeScaleChanged FINAL)

    Q_PROPERTY(int skipIntervalSeconds READ skipIntervalSeconds WRITE setSkipIntervalSeconds NOTIFY skipIntervalSecondsChanged FINAL)
    Q_PROPERTY(int playerVolume READ playerVolume WRITE setPlayerVolume NOTIFY playerVolumeChanged FINAL)
    Q_PROPERTY(double holdToSpeedMultiplier READ holdToSpeedMultiplier WRITE setHoldToSpeedMultiplier NOTIFY holdToSpeedMultiplierChanged FINAL)
    Q_PROPERTY(bool keepScreenOn READ keepScreenOn WRITE setKeepScreenOn NOTIFY keepScreenOnChanged FINAL)
    Q_PROPERTY(bool autoPlayNextEpisode READ autoPlayNextEpisode WRITE setAutoPlayNextEpisode NOTIFY autoPlayNextEpisodeChanged FINAL)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY closeToTrayChanged FINAL)
    Q_PROPERTY(bool trayNoticeSeen READ trayNoticeSeen WRITE setTrayNoticeSeen NOTIFY trayNoticeSeenChanged FINAL)
    Q_PROPERTY(bool forceHardwareDecoding READ forceHardwareDecoding WRITE setForceHardwareDecoding NOTIFY forceHardwareDecodingChanged FINAL)
    Q_PROPERTY(int subtitleScalePercent READ subtitleScalePercent WRITE setSubtitleScalePercent NOTIFY subtitleScalePercentChanged FINAL)
    Q_PROPERTY(QString subtitleEdgeStyle READ subtitleEdgeStyle WRITE setSubtitleEdgeStyle NOTIFY subtitleEdgeStyleChanged FINAL)
    Q_PROPERTY(QString subtitlePosition READ subtitlePosition WRITE setSubtitlePosition NOTIFY subtitlePositionChanged FINAL)
    Q_PROPERTY(QString subtitleColor READ subtitleColor WRITE setSubtitleColor NOTIFY subtitleColorChanged FINAL)
    Q_PROPERTY(bool subtitleBold READ subtitleBold WRITE setSubtitleBold NOTIFY subtitleBoldChanged FINAL)
    Q_PROPERTY(QString subtitleLanguage READ subtitleLanguage WRITE setSubtitleLanguage NOTIFY subtitleLanguageChanged FINAL)
    Q_PROPERTY(QString audioLanguage READ audioLanguage WRITE setAudioLanguage NOTIFY audioLanguageChanged FINAL)
    Q_PROPERTY(QString subtitleAccount READ subtitleAccount WRITE setSubtitleAccount NOTIFY subtitleAccountChanged FINAL)
    Q_PROPERTY(QString subtitlePassword READ subtitlePassword WRITE setSubtitlePassword NOTIFY subtitleAccountChanged FINAL)
    Q_PROPERTY(QStringList subtitleSearchLanguages READ subtitleSearchLanguages WRITE setSubtitleSearchLanguages NOTIFY subtitleSearchLanguagesChanged FINAL)
    Q_PROPERTY(bool subtitlesOnByDefault READ subtitlesOnByDefault WRITE setSubtitlesOnByDefault NOTIFY subtitlesOnByDefaultChanged FINAL)
    Q_PROPERTY(QString playerControlScheme READ playerControlScheme WRITE setPlayerControlScheme NOTIFY playerControlSchemeChanged FINAL)
    Q_PROPERTY(bool rememberTrackPerShow READ rememberTrackPerShow WRITE setRememberTrackPerShow NOTIFY rememberTrackPerShowChanged FINAL)
    Q_PROPERTY(int windowX READ windowX WRITE setWindowX NOTIFY windowXChanged FINAL)
    Q_PROPERTY(int windowY READ windowY WRITE setWindowY NOTIFY windowYChanged FINAL)
    Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowWidthChanged FINAL)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged FINAL)
    Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized NOTIFY windowMaximizedChanged FINAL)
    Q_PROPERTY(bool treatAsTelevision READ treatAsTelevision WRITE setTreatAsTelevision NOTIFY treatAsTelevisionChanged FINAL)
    Q_PROPERTY(QStringList recentSearches READ recentSearches NOTIFY recentSearchesChanged FINAL)

public:
    explicit AppSettings(QObject *parent = nullptr);
    AppSettings(QSettings::Format format,
                const QString &organisation,
                const QString &application,
                QObject *parent = nullptr);

    int accentIndex() const;
    void setAccentIndex(int index);

    double fontSizeScale() const;
    void setFontSizeScale(double scale);

    int skipIntervalSeconds() const;
    int playerVolume() const;
    void setPlayerVolume(int volume);
    void setSkipIntervalSeconds(int seconds);

    double holdToSpeedMultiplier() const;
    void setHoldToSpeedMultiplier(double multiplier);



    bool keepScreenOn() const;
    void setKeepScreenOn(bool value);

    bool autoPlayNextEpisode() const;
    void setAutoPlayNextEpisode(bool value);

    bool closeToTray() const;
    void setCloseToTray(bool value);

    bool trayNoticeSeen() const;
    void setTrayNoticeSeen(bool value);

    bool forceHardwareDecoding() const;
    void setForceHardwareDecoding(bool value);

    int subtitleScalePercent() const;
    void setSubtitleScalePercent(int value);

    QString subtitleEdgeStyle() const;
    void setSubtitleEdgeStyle(const QString &value);

    QString subtitlePosition() const;
    void setSubtitlePosition(const QString &value);

    QString subtitleColor() const;
    void setSubtitleColor(const QString &value);

    bool subtitleBold() const;
    void setSubtitleBold(bool value);

    QString subtitleLanguage() const;
    void setSubtitleLanguage(const QString &value);

    QString audioLanguage() const;
    void setAudioLanguage(const QString &value);

    QString subtitleAccount() const;
    void setSubtitleAccount(const QString &value);
    QString subtitlePassword() const;
    void setSubtitlePassword(const QString &value);
    QStringList subtitleSearchLanguages() const;
    void setSubtitleSearchLanguages(const QStringList &value);

    bool subtitlesOnByDefault() const;
    void setSubtitlesOnByDefault(bool value);

    QString playerControlScheme() const;
    void setPlayerControlScheme(const QString &value);



    bool rememberTrackPerShow() const;
    void setRememberTrackPerShow(bool value);

    int windowX() const;
    void setWindowX(int value);

    int windowY() const;
    void setWindowY(int value);

    int windowWidth() const;
    void setWindowWidth(int value);

    int windowHeight() const;
    void setWindowHeight(int value);

    bool windowMaximized() const;
    void setWindowMaximized(bool value);

    bool treatAsTelevision() const;
    void setTreatAsTelevision(bool value);

    QStringList recentSearches() const;
    Q_INVOKABLE void noteSearch(const QString &query);
    Q_INVOKABLE void forgetSearches();
    static bool treatAsTelevisionAtLaunch();

    QString streamingServerId();
    QString streamingServerName() const;
    QString streamingDeviceId();

    bool accentChosen() const;

signals:
    void accentIndexChanged();
    void fontSizeScaleChanged();
    void skipIntervalSecondsChanged();
    void playerVolumeChanged();
    void holdToSpeedMultiplierChanged();
    void keepScreenOnChanged();
    void autoPlayNextEpisodeChanged();
    void closeToTrayChanged();
    void trayNoticeSeenChanged();
    void forceHardwareDecodingChanged();
    void subtitleScalePercentChanged();
    void subtitleEdgeStyleChanged();
    void subtitlePositionChanged();
    void subtitleColorChanged();
    void subtitleBoldChanged();
    void subtitleLanguageChanged();
    void audioLanguageChanged();
    void subtitleAccountChanged();
    void subtitleSearchLanguagesChanged();
    void subtitlesOnByDefaultChanged();
    void playerControlSchemeChanged();
    void rememberTrackPerShowChanged();
    void windowXChanged();
    void windowYChanged();
    void windowWidthChanged();
    void windowHeightChanged();
    void windowMaximizedChanged();
    void treatAsTelevisionChanged();
    void recentSearchesChanged();

private:
    QSettings m_settings;
};
