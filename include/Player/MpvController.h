#pragma once

#include "Player/TrackListModel.h"

#include <QObject>
#include <QSize>
#include <QString>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <functional>
#include <memory>

struct mpv_handle;

class IAudioFocus;
class QThread;
class ITransportControls;

class MpvController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged FINAL)
    Q_PROPERTY(bool fileLoaded READ fileLoaded NOTIFY fileLoadedChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged FINAL)
    Q_PROPERTY(bool buffering READ buffering NOTIFY bufferingChanged FINAL)
    Q_PROPERTY(bool endReached READ endReached NOTIFY endReachedChanged FINAL)
    Q_PROPERTY(bool allowUnsupportedPicture READ allowUnsupportedPicture
               WRITE setAllowUnsupportedPicture
               NOTIFY allowUnsupportedPictureChanged FINAL)

    Q_PROPERTY(double position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged FINAL)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged FINAL)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged FINAL)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged FINAL)

    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged FINAL)
    Q_PROPERTY(QString subtitleText READ subtitleText NOTIFY subtitleTextChanged FINAL)
    Q_PROPERTY(QString hwdecActive READ hwdecActive NOTIFY hwdecActiveChanged FINAL)
    Q_PROPERTY(QString videoCodec READ videoCodec NOTIFY videoCodecChanged FINAL)
    Q_PROPERTY(QString audioCodec READ audioCodec NOTIFY audioCodecChanged FINAL)
    Q_PROPERTY(QString videoResolution READ videoResolution NOTIFY videoResolutionChanged FINAL)
    Q_PROPERTY(QString fileFormat READ fileFormat NOTIFY fileFormatChanged FINAL)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoResolutionChanged FINAL)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoResolutionChanged FINAL)
    Q_PROPERTY(bool hdr READ hdr NOTIFY hdrChanged FINAL)

    Q_PROPERTY(TrackListModel *audioTracks READ audioTracks CONSTANT FINAL)
    Q_PROPERTY(TrackListModel *subtitleTracks READ subtitleTracks CONSTANT FINAL)
    Q_PROPERTY(qint64 audioTrackId READ audioTrackId WRITE setAudioTrackId NOTIFY audioTrackIdChanged FINAL)
    Q_PROPERTY(qint64 subtitleTrackId READ subtitleTrackId WRITE setSubtitleTrackId NOTIFY subtitleTrackIdChanged FINAL)

    Q_PROPERTY(double subDelay READ subDelay WRITE setSubDelay NOTIFY subDelayChanged FINAL)
    Q_PROPERTY(double audioDelay READ audioDelay WRITE setAudioDelay NOTIFY audioDelayChanged FINAL)
    Q_PROPERTY(QString aspectOverride READ aspectOverride WRITE setAspectOverride NOTIFY aspectOverrideChanged FINAL)
    Q_PROPERTY(double panscan READ panscan WRITE setPanscan NOTIFY panscanChanged FINAL)
    Q_PROPERTY(double videoZoom READ videoZoom WRITE setVideoZoom NOTIFY videoZoomChanged FINAL)
    Q_PROPERTY(double videoPanX READ videoPanX WRITE setVideoPanX NOTIFY videoPanChanged FINAL)
    Q_PROPERTY(double videoPanY READ videoPanY WRITE setVideoPanY NOTIFY videoPanChanged FINAL)

public:
    explicit MpvController(QObject *parent = nullptr);
    ~MpvController() override;

    mpv_handle *handle() const;

    void setAudioFocus(IAudioFocus *focus);
    void setTransportControls(ITransportControls *controls);

    bool ready() const;
    bool fileLoaded() const;
    bool loading() const;

    bool paused() const;
    void setPaused(bool paused);

    bool buffering() const;
    bool endReached() const;

    bool allowUnsupportedPicture() const;
    void setAllowUnsupportedPicture(bool allow);

    double position() const;
    double duration() const;

    double speed() const;
    void setSpeed(double speed);

    int volume() const;
    void setVolume(int volume);

    bool muted() const;
    void setMuted(bool muted);

    QString mediaTitle() const;
    QString subtitleText() const;
    QString hwdecActive() const;
    QString videoCodec() const;
    QString audioCodec() const;
    QString videoResolution() const;
    QString fileFormat() const;
    int videoWidth() const;
    int videoHeight() const;
    bool hdr() const;

    TrackListModel *audioTracks() const;
    TrackListModel *subtitleTracks() const;

    qint64 audioTrackId() const;
    void setAudioTrackId(qint64 id);

    qint64 subtitleTrackId() const;
    void setSubtitleTrackId(qint64 id);

    double subDelay() const;
    void setSubDelay(double seconds);

    double audioDelay() const;
    void setAudioDelay(double seconds);

    QString aspectOverride() const;
    void setAspectOverride(const QString &aspect);

    double panscan() const;
    void setPanscan(double value);

    double videoZoom() const;
    void setVideoZoom(double value);

    double videoPanX() const;
    void setVideoPanX(double value);

    double videoPanY() const;
    void setVideoPanY(double value);

    Q_INVOKABLE void resetVideoZoom();

    Q_INVOKABLE void setSurfaceAspect(double aspect, bool fill);

    Q_INVOKABLE void setRenderContextActive(bool active);
    Q_INVOKABLE bool renderContextActive() const;

    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void setSubtitlesForNextOpen(const QString &path,
                                             const QVariantList &subtitles);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void pauseForBackground();
    Q_INVOKABLE void seekRelative(double seconds);
    Q_INVOKABLE void seekAbsolute(double seconds);
    Q_INVOKABLE void addSubtitleFile(const QString &path,
                                     const QString &title = QString());
    Q_INVOKABLE void applySubtitleStyle(const QVariantMap &options);
    Q_INVOKABLE void setForceHardwareDecoding(bool force);
    Q_INVOKABLE void cycleAudioTrack();
    Q_INVOKABLE void cycleSubtitleTrack();

signals:
    void readyChanged();
    void fileLoadedChanged();
    void loadingChanged();
    void pausedChanged();
    void bufferingChanged();
    void endReachedChanged();
    void allowUnsupportedPictureChanged();
    void positionChanged();
    void durationChanged();
    void speedChanged();
    void volumeChanged();
    void mutedChanged();
    void mediaTitleChanged();
    void subtitleTextChanged();
    void hwdecActiveChanged();
    void videoCodecChanged();
    void audioCodecChanged();
    void videoResolutionChanged();
    void hdrChanged();
    void fileFormatChanged();
    void audioTrackIdChanged();
    void subtitleTrackIdChanged();
    void subDelayChanged();
    void audioDelayChanged();
    void aspectOverrideChanged();
    void panscanChanged();
    void videoZoomChanged();
    void videoPanChanged();
    void playbackFailed(const QString &reason);

private slots:
    void drainEvents();
    void logPlaybackHealth();
    void giveUpOnOpening();
    void flushMpvRepeats();

    void onFocusLost();
    void onFocusLostTransient();
    void onFocusRegained();
    void onShouldDuck(bool duck);
    void onBecomingNoisy();

private:
    static constexpr quint64 kPreloadedHookId = 1;

    void observeProperties();
    void addPreloadedSubtitles();
    void pickPreloadedSubtitle();
    void applyCacheProfile(const QString &path);
    void setPropertyFallback(const QString &name,
                             const QString &legacyName,
                             const QVariant &value);
    void acquireAudioFocus();
    void releaseAudioFocus();
    void publishTransportMetadata();
    void publishTransportState();
    void handlePropertyChange(void *eventProperty);
    void clearProperty(const QString &name);
    void command(const QVariantList &args);
    void setPropertyVariant(const QString &name, const QVariant &value);
    void post(std::function<void(mpv_handle *)> work);
    void startRequestThread();
    bool stopRequestThread();
    bool attachVideoSurface();
    void detachVideoSurface();

    void rebuildTrackLists(const QVariant &trackList);
    void updateVideoResolution();
    void updateHdr();
    void setLoading(bool loading);

    mpv_handle *m_mpv = nullptr;
    QThread *m_requestThread = nullptr;
    QObject *m_requestWorker = nullptr;

    QTimer m_healthTimer;
    struct DropCounts
    {
        qint64 vo = 0;
        qint64 decoder = 0;
    };
    std::shared_ptr<DropCounts> m_dropCounts = std::make_shared<DropCounts>();

    bool m_ready = false;
    bool m_fileLoaded = false;
    bool m_videoChainFailed = false;
    QHash<QString, int> m_mpvSuppressed;
    QElapsedTimer m_mpvWindow;
    bool m_decoderComplained = false;
    QTimer m_startTimer;
    bool m_allowUnsupportedPicture = false;
    bool m_pictureJudged = false;
    bool m_pickPreloadedSubtitle = false;
    bool m_networkCacheProfile = false;
    QString m_firstPreloadedSubtitle;
    bool m_loading = false;
    bool m_openInFlight = false;
    std::atomic<bool> m_paused{true};
    bool m_buffering = false;
    bool m_endReached = false;
    double m_position = 0.0;
    double m_duration = 0.0;
    double m_speed = 1.0;
    std::atomic<int> m_volume{100};
    bool m_muted = false;
    QString m_mediaTitle;
    QString m_subtitleText;
    QString m_hwdecActive;
    QString m_videoCodec;
    QString m_audioCodec;
    QString m_videoResolution;
    QString m_fileFormat;
    qint64 m_videoWidth = 0;
    qint64 m_videoHeight = 0;

    qint64 m_displayWidth = 0;
    qint64 m_displayHeight = 0;

    QSize m_surfaceSize;
    QString m_videoPrimaries;
    QString m_videoGamma;
    bool m_hdr = false;
    TrackListModel *m_audioTracks = nullptr;
    TrackListModel *m_subtitleTracks = nullptr;
    qint64 m_audioTrackId = 0;
    qint64 m_subtitleTrackId = 0;
    double m_videoZoom = 0.0;
    double m_videoPanX = 0.0;
    double m_videoPanY = 0.0;
    double m_subDelay = 0.0;
    double m_audioDelay = 0.0;
    bool m_renderContextActive = false;
    bool m_videoSurfaceAttached = false;
    QString m_pendingOpenPath;
    QString m_subtitlesPath;
    QVariantList m_pendingSubtitles;
    QVariantList m_subtitlesForLoad;
    QString m_aspectOverride = QStringLiteral("-1");
    double m_panscan = 0.0;
    IAudioFocus *m_audioFocus = nullptr;
    ITransportControls *m_transportControls = nullptr;
    std::atomic<bool> m_holdsAudioFocus{false};
    std::atomic<bool> m_pausedByFocusLoss{false};
    bool m_subtitlesOnByDefault = true;
    std::atomic<bool> m_ducked{false};
    std::atomic<int> m_volumeBeforeDuck{100};
};
