#pragma once

#include "Platform/IScreenLock.h"

#include <QColor>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#ifdef Q_OS_WIN
class WindowsTrayIcon;
#endif

class SystemBridge : public QObject, public IScreenLock
{
    Q_OBJECT

    Q_PROPERTY(bool usesSystemBackNavigation READ usesSystemBackNavigation CONSTANT FINAL)
    Q_PROPERTY(bool hasTouchInput READ hasTouchInput CONSTANT FINAL)
    Q_PROPERTY(bool usesWindowGeometry READ usesWindowGeometry CONSTANT FINAL)
    Q_PROPERTY(bool isTelevision READ isTelevision CONSTANT FINAL)
    Q_PROPERTY(bool canSimulateTelevision READ canSimulateTelevision CONSTANT FINAL)
    Q_PROPERTY(bool pausesPlaybackInBackground READ pausesPlaybackInBackground CONSTANT FINAL)
    Q_PROPERTY(bool canSetBrightness READ canSetBrightness CONSTANT FINAL)
    Q_PROPERTY(bool usesManagedStorageRoots READ usesManagedStorageRoots CONSTANT FINAL)
    Q_PROPERTY(bool usesSystemFilePicker READ usesSystemFilePicker CONSTANT FINAL)
    Q_PROPERTY(bool canForceHardwareDecoding READ canForceHardwareDecoding CONSTANT FINAL)
    Q_PROPERTY(bool supportsMiniPlayer READ supportsMiniPlayer CONSTANT FINAL)
    Q_PROPERTY(bool inMiniPlayer READ inMiniPlayer NOTIFY inMiniPlayerChanged FINAL)
    Q_PROPERTY(bool canShowInFileManager READ canShowInFileManager CONSTANT FINAL)
    Q_PROPERTY(bool supportsTrayIcon READ supportsTrayIcon CONSTANT FINAL)
    Q_PROPERTY(bool inTray READ inTray NOTIFY inTrayChanged FINAL)
    Q_PROPERTY(bool videoPermissionGranted READ hasVideoPermission NOTIFY videoPermissionChanged FINAL)
    Q_PROPERTY(bool videoPermissionBlocked READ videoPermissionBlocked NOTIFY videoPermissionChanged FINAL)
    Q_PROPERTY(bool videoAccessPartial READ videoAccessPartial NOTIFY videoPermissionChanged FINAL)
    Q_PROPERTY(bool videoPartialAccessAccepted READ videoPartialAccessAccepted NOTIFY videoPermissionChanged FINAL)
    Q_PROPERTY(bool videoAccessSkipped READ videoAccessSkipped NOTIFY videoPermissionChanged FINAL)
    Q_PROPERTY(QStringList readingDrives READ readingDrives NOTIFY readingDrivesChanged FINAL)
    Q_PROPERTY(qreal keyboardHeight READ keyboardHeight NOTIFY keyboardHeightChanged FINAL)

public:
    explicit SystemBridge(QObject *parent = nullptr);
    ~SystemBridge() override;

    bool usesSystemBackNavigation() const;
    bool hasTouchInput() const;
    bool usesWindowGeometry() const;
    bool isTelevision() const;
    static bool runningOnTelevision();
    bool canSimulateTelevision() const;
    bool pausesPlaybackInBackground() const;
    bool canRegisterFileTypes() const;
    bool fileTypesRegistered() const;

    bool usesManagedStorageRoots() const;
    bool supportsMiniPlayer() const;
    bool inMiniPlayer() const;

    Q_INVOKABLE void enterMiniPlayer(QObject *window,
                                     int videoWidth,
                                     int videoHeight);
    Q_INVOKABLE void leaveMiniPlayer(QObject *window);

    bool supportsTrayIcon() const;
    bool inTray() const;

    Q_INVOKABLE void hideToTray(QObject *window);
    Q_INVOKABLE void restoreFromTray(QObject *window);
    Q_INVOKABLE void setTrayStatus(const QString &tooltip, bool streaming);
    Q_INVOKABLE void showTrayNotice(const QString &title, const QString &text);

    Q_INVOKABLE bool startWindowMove(QObject *window);
    bool usesSystemFilePicker() const;
    bool canForceHardwareDecoding() const;

    Q_INVOKABLE void copyToClipboard(const QString &text);

    bool canShowInFileManager() const;
    Q_INVOKABLE void showInFileManager(const QString &path);

    Q_INVOKABLE void pickSubtitleFile();
    Q_INVOKABLE void saveLogFile();
    Q_INVOKABLE void requestTextInput(const QString &title, const QString &text);

    Q_INVOKABLE QString readBundledText(const QString &path) const;

    Q_INVOKABLE bool needsVideoPermission() const;
    Q_INVOKABLE bool hasVideoPermission() const;
    bool videoAccessPartial() const;
    bool videoPartialAccessAccepted() const;
    Q_INVOKABLE void acceptPartialVideoAccess();

    bool videoAccessSkipped() const;
    Q_INVOKABLE void continueWithoutVideos();
    Q_INVOKABLE void requestVideoPermission();

    bool videoPermissionBlocked() const;
    Q_INVOKABLE void openAppSettings();

    void setFileTypesRegistered(bool registered);

    bool canSetBrightness() const;

    Q_INVOKABLE double systemBrightness() const;
    Q_INVOKABLE void setScreenBrightness(double value);
    Q_INVOKABLE void releaseScreenBrightness();

    Q_INVOKABLE void setImmersiveMode(bool immersive);

    Q_INVOKABLE void setLightStatusBar(bool light);

    Q_INVOKABLE void setKeepScreenOn(bool keepOn) override;
    bool keepScreenOn() const override;

    void setKeepSystemAwake(bool awake) override;
    bool keepSystemAwake() const override;

    Q_INVOKABLE void minimizeApp();

    QStringList readingDrives() const;

    qreal keyboardHeight() const;

signals:
    void backPressed();
    void fileTypesRegisteredChanged();
    void videoPermissionChanged();
    void subtitleFilePicked(const QString &url);
    void subtitleFilePickCancelled();
    void subtitleFolderAccessResult(bool granted);
    void logFileSaved(int outcome);
    void textInputFinished(const QString &text, bool accepted);
    void inMiniPlayerChanged();
    void inTrayChanged();
    void trayOpenRequested();
    void trayQuitRequested();
    void trayStopStreamingRequested();
    void storageIndexChanged(const QString &volumeName);
    void readingDrivesChanged();
    void keyboardHeightChanged();

private slots:
    void onBackInternal();
    void onVideoPermissionResult(bool granted);
    void onSubtitlePicked(const QString &url);
    void onSubtitleFolderResult(bool granted);
    void onLogSaved(int outcome);
    void onTextEntered(const QString &text, bool accepted);
    void onStorageIndexChanged(const QString &volumeName);
    void onStorageReading(const QString &volumeName, bool reading);
    void onKeyboardHeight(int pixels);

private:
    int videoAccessState() const;
    void applyExecutionState();

#ifdef Q_OS_WIN
    bool ensureTray();

    WindowsTrayIcon *m_tray = nullptr;
    bool m_inTray = false;
#endif

    QSettings m_settings;
    bool m_keepScreenOn = false;
    bool m_keepSystemAwake = false;
    bool m_hasTouchInput = false;
    int m_lastVideoAccess = -1;
    QStringList m_readingVolumes;
    qreal m_keyboardHeight = 0;
};
