#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QTimer;

namespace nuclip {

class Settings : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool launchOnStartup READ launchOnStartup WRITE setLaunchOnStartup NOTIFY launchOnStartupChanged)
    Q_PROPERTY(bool updatePrompt READ updatePrompt WRITE setUpdatePrompt NOTIFY updatePromptChanged)
    Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY notificationsChanged)
    Q_PROPERTY(bool autoStartReplay READ autoStartReplay WRITE setAutoStartReplay NOTIFY autoStartReplayChanged)
    Q_PROPERTY(QString saveDirectory READ saveDirectory WRITE setSaveDirectory NOTIFY saveDirectoryChanged)
    Q_PROPERTY(QString clipsDirectory READ clipsDirectory NOTIFY clipsDirectoryChanged)
    Q_PROPERTY(QString screenshotsDirectory READ screenshotsDirectory NOTIFY clipsDirectoryChanged)

    Q_PROPERTY(int replaySeconds READ replaySeconds WRITE setReplaySeconds NOTIFY replaySecondsChanged)
    Q_PROPERTY(QString captureMode READ captureMode WRITE setCaptureMode NOTIFY captureModeChanged)
    Q_PROPERTY(int monitorIndex READ monitorIndex WRITE setMonitorIndex NOTIFY monitorIndexChanged)
    Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY qualityChanged)
    Q_PROPERTY(QString qualityLabel READ qualityLabel NOTIFY qualityChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY videoChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY videoChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY videoChanged)
    Q_PROPERTY(int videoBitrateKbps READ videoBitrateKbps WRITE setVideoBitrateKbps NOTIFY videoChanged)
    Q_PROPERTY(QString encoder READ encoder WRITE setEncoder NOTIFY encoderChanged)
    Q_PROPERTY(QString codec READ codec WRITE setCodec NOTIFY codecChanged)
    Q_PROPERTY(int audioBitrateKbps READ audioBitrateKbps WRITE setAudioBitrateKbps NOTIFY audioBitrateChanged)
    Q_PROPERTY(bool showCursor READ showCursor WRITE setShowCursor NOTIFY showCursorChanged)

    Q_PROPERTY(bool systemAudio READ systemAudio WRITE setSystemAudio NOTIFY systemAudioChanged)
    Q_PROPERTY(double systemVolume READ systemVolume WRITE setSystemVolume NOTIFY systemVolumeChanged)
    Q_PROPERTY(QString outputDeviceId READ outputDeviceId WRITE setOutputDeviceId NOTIFY outputDeviceIdChanged)
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled NOTIFY micEnabledChanged)
    Q_PROPERTY(double micVolume READ micVolume WRITE setMicVolume NOTIFY micVolumeChanged)
    Q_PROPERTY(QString micDeviceId READ micDeviceId WRITE setMicDeviceId NOTIFY micDeviceIdChanged)

    Q_PROPERTY(QString hotkeyClip READ hotkeyClip WRITE setHotkeyClip NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyClip15 READ hotkeyClip15 WRITE setHotkeyClip15 NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyClip30 READ hotkeyClip30 WRITE setHotkeyClip30 NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyClip60 READ hotkeyClip60 WRITE setHotkeyClip60 NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyClip120 READ hotkeyClip120 WRITE setHotkeyClip120 NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyScreenshot READ hotkeyScreenshot WRITE setHotkeyScreenshot NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyToggle READ hotkeyToggle WRITE setHotkeyToggle NOTIFY hotkeysChanged)
    Q_PROPERTY(QString hotkeyPause READ hotkeyPause WRITE setHotkeyPause NOTIFY hotkeysChanged)

    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(QString customAccent READ customAccent WRITE setCustomAccent NOTIFY accentChanged)

    Q_PROPERTY(bool organizeByGame READ organizeByGame WRITE setOrganizeByGame NOTIFY organizeByGameChanged)
    Q_PROPERTY(int storageLimitGb READ storageLimitGb WRITE setStorageLimitGb NOTIFY storageLimitChanged)
    Q_PROPERTY(bool autoCleanup READ autoCleanup WRITE setAutoCleanup NOTIFY storageLimitChanged)

    Q_PROPERTY(QString exportFormat READ exportFormat WRITE setExportFormat NOTIFY exportChanged)
    Q_PROPERTY(QString exportQuality READ exportQuality WRITE setExportQuality NOTIFY exportChanged)
    Q_PROPERTY(int exportHeight READ exportHeight WRITE setExportHeight NOTIFY exportChanged)
    Q_PROPERTY(bool discordCompress READ discordCompress WRITE setDiscordCompress NOTIFY exportChanged)
    Q_PROPERTY(int discordTargetMb READ discordTargetMb WRITE setDiscordTargetMb NOTIFY exportChanged)

    Q_PROPERTY(bool overlayEnabled READ overlayEnabled WRITE setOverlayEnabled NOTIFY overlayChanged)
    Q_PROPERTY(bool overlayClip READ overlayClip WRITE setOverlayClip NOTIFY overlayChanged)
    Q_PROPERTY(bool overlayScreenshot READ overlayScreenshot WRITE setOverlayScreenshot NOTIFY overlayChanged)
    Q_PROPERTY(bool overlayRecordStart READ overlayRecordStart WRITE setOverlayRecordStart NOTIFY overlayChanged)
    Q_PROPERTY(bool overlayRecordStop READ overlayRecordStop WRITE setOverlayRecordStop NOTIFY overlayChanged)
    Q_PROPERTY(bool overlayError READ overlayError WRITE setOverlayError NOTIFY overlayChanged)
    Q_PROPERTY(QString overlayPosition READ overlayPosition WRITE setOverlayPosition NOTIFY overlayChanged)
    Q_PROPERTY(int overlayDurationMs READ overlayDurationMs WRITE setOverlayDurationMs NOTIFY overlayChanged)
    Q_PROPERTY(QString overlaySize READ overlaySize WRITE setOverlaySize NOTIFY overlayChanged)
    Q_PROPERTY(QString overlayAnimation READ overlayAnimation WRITE setOverlayAnimation NOTIFY overlayChanged)
    Q_PROPERTY(bool overlayUseAccent READ overlayUseAccent WRITE setOverlayUseAccent NOTIFY overlayChanged)
    Q_PROPERTY(double overlayOpacity READ overlayOpacity WRITE setOverlayOpacity NOTIFY overlayChanged)

    Q_PROPERTY(bool sfxEnabled READ sfxEnabled WRITE setSfxEnabled NOTIFY sfxChanged)
    Q_PROPERTY(double sfxVolume READ sfxVolume WRITE setSfxVolume NOTIFY sfxChanged)
    Q_PROPERTY(QString sfxSound READ sfxSound WRITE setSfxSound NOTIFY sfxChanged)
    Q_PROPERTY(bool sfxClip READ sfxClip WRITE setSfxClip NOTIFY sfxChanged)
    Q_PROPERTY(bool sfxScreenshot READ sfxScreenshot WRITE setSfxScreenshot NOTIFY sfxChanged)
    Q_PROPERTY(bool sfxRecordStart READ sfxRecordStart WRITE setSfxRecordStart NOTIFY sfxChanged)
    Q_PROPERTY(bool sfxRecordStop READ sfxRecordStop WRITE setSfxRecordStop NOTIFY sfxChanged)
    Q_PROPERTY(bool sfxError READ sfxError WRITE setSfxError NOTIFY sfxChanged)

    Q_PROPERTY(bool discordEnabled READ discordEnabled WRITE setDiscordEnabled NOTIFY discordChanged)
    Q_PROPERTY(bool discordAutoConnect READ discordAutoConnect WRITE setDiscordAutoConnect NOTIFY discordChanged)
    Q_PROPERTY(bool discordShowGame READ discordShowGame WRITE setDiscordShowGame NOTIFY discordChanged)
    Q_PROPERTY(bool discordShowRecording READ discordShowRecording WRITE setDiscordShowRecording NOTIFY discordChanged)
    Q_PROPERTY(bool discordShowBuffer READ discordShowBuffer WRITE setDiscordShowBuffer NOTIFY discordChanged)
    Q_PROPERTY(bool discordShowEditing READ discordShowEditing WRITE setDiscordShowEditing NOTIFY discordChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    void load();
    void setPersistEnabled(bool enabled);

    bool launchOnStartup() const { return m_launchOnStartup; }
    bool updatePrompt() const { return m_updatePrompt; }
    bool minimizeToTray() const { return m_minimizeToTray; }
    bool notifications() const { return m_notifications; }
    bool autoStartReplay() const { return m_autoStartReplay; }
    QString saveDirectory() const { return m_saveDirectory; }
    QString clipsDirectory() const;
    QString screenshotsDirectory() const;

    int replaySeconds() const { return m_replaySeconds; }
    QString captureMode() const { return m_captureMode; }
    int monitorIndex() const { return m_monitorIndex; }
    QString quality() const { return m_quality; }
    QString qualityLabel() const;
    int width() const { return m_width; }
    int height() const { return m_height; }
    int fps() const { return m_fps; }
    int videoBitrateKbps() const { return m_videoBitrateKbps; }
    QString encoder() const { return m_encoder; }
    QString codec() const { return m_codec; }
    int audioBitrateKbps() const { return m_audioBitrateKbps; }
    bool showCursor() const { return m_showCursor; }

    bool systemAudio() const { return m_systemAudio; }
    double systemVolume() const { return m_systemVolume; }
    QString outputDeviceId() const { return m_outputDeviceId; }
    bool micEnabled() const { return m_micEnabled; }
    double micVolume() const { return m_micVolume; }
    QString micDeviceId() const { return m_micDeviceId; }

    QString hotkeyClip() const { return m_hotkeyClip; }
    QString hotkeyClip15() const { return m_hotkeyClip15; }
    QString hotkeyClip30() const { return m_hotkeyClip30; }
    QString hotkeyClip60() const { return m_hotkeyClip60; }
    QString hotkeyClip120() const { return m_hotkeyClip120; }
    QString hotkeyScreenshot() const { return m_hotkeyScreenshot; }
    QString hotkeyToggle() const { return m_hotkeyToggle; }
    QString hotkeyPause() const { return m_hotkeyPause; }

    QString themeMode() const { return m_themeMode; }
    QString accent() const { return m_accent; }
    QString customAccent() const { return m_customAccent; }

    bool organizeByGame() const { return m_organizeByGame; }
    int storageLimitGb() const { return m_storageLimitGb; }
    bool autoCleanup() const { return m_autoCleanup; }

    QString exportFormat() const { return m_exportFormat; }
    QString exportQuality() const { return m_exportQuality; }
    int exportHeight() const { return m_exportHeight; }
    bool discordCompress() const { return m_discordCompress; }
    int discordTargetMb() const { return m_discordTargetMb; }

    bool overlayEnabled() const { return m_overlayEnabled; }
    bool overlayClip() const { return m_overlayClip; }
    bool overlayScreenshot() const { return m_overlayScreenshot; }
    bool overlayRecordStart() const { return m_overlayRecordStart; }
    bool overlayRecordStop() const { return m_overlayRecordStop; }
    bool overlayError() const { return m_overlayError; }
    QString overlayPosition() const { return m_overlayPosition; }
    int overlayDurationMs() const { return m_overlayDurationMs; }
    QString overlaySize() const { return m_overlaySize; }
    QString overlayAnimation() const { return m_overlayAnimation; }
    bool overlayUseAccent() const { return m_overlayUseAccent; }
    double overlayOpacity() const { return m_overlayOpacity; }

    bool sfxEnabled() const { return m_sfxEnabled; }
    double sfxVolume() const { return m_sfxVolume; }
    QString sfxSound() const { return m_sfxSound; }
    bool sfxClip() const { return m_sfxClip; }
    bool sfxScreenshot() const { return m_sfxScreenshot; }
    bool sfxRecordStart() const { return m_sfxRecordStart; }
    bool sfxRecordStop() const { return m_sfxRecordStop; }
    bool sfxError() const { return m_sfxError; }

    bool discordEnabled() const { return m_discordEnabled; }
    bool discordAutoConnect() const { return m_discordAutoConnect; }
    bool discordShowGame() const { return m_discordShowGame; }
    bool discordShowRecording() const { return m_discordShowRecording; }
    bool discordShowBuffer() const { return m_discordShowBuffer; }
    bool discordShowEditing() const { return m_discordShowEditing; }

    Q_INVOKABLE void applyQualityPreset(const QString &preset);
    Q_INVOKABLE int recommendedVideoBitrate(int width, int height, int fps, const QString &codec,
                                            const QString &quality) const;
    Q_INVOKABLE void applyDerivedBitrate(int width, int height);
    Q_INVOKABLE void editCustomBitrate(int kbps);
    Q_INVOKABLE int audioBitrateFor(const QString &quality) const;
    Q_INVOKABLE int exportVideoBitrateKbps() const;
    QString appDisplayName(const QString &stableId) const;
    void setAppDisplayName(const QString &stableId, const QString &name);

public slots:
    void setLaunchOnStartup(bool value);
    void setUpdatePrompt(bool value);
    void setMinimizeToTray(bool value);
    void setNotifications(bool value);
    void setAutoStartReplay(bool value);
    void setSaveDirectory(const QString &value);
    void setReplaySeconds(int value);
    void setCaptureMode(const QString &value);
    void setMonitorIndex(int value);
    void setQuality(const QString &value);
    void setWidth(int value);
    void setHeight(int value);
    void setFps(int value);
    void setVideoBitrateKbps(int value);
    void setEncoder(const QString &value);
    void setCodec(const QString &value);
    void setAudioBitrateKbps(int value);
    void setShowCursor(bool value);
    void setSystemAudio(bool value);
    void setSystemVolume(double value);
    void setOutputDeviceId(const QString &value);
    void setMicEnabled(bool value);
    void setMicVolume(double value);
    void setMicDeviceId(const QString &value);
    void setHotkeyClip(const QString &value);
    void setHotkeyClip15(const QString &value);
    void setHotkeyClip30(const QString &value);
    void setHotkeyClip60(const QString &value);
    void setHotkeyClip120(const QString &value);
    void setHotkeyScreenshot(const QString &value);
    void setHotkeyToggle(const QString &value);
    void setHotkeyPause(const QString &value);
    void setThemeMode(const QString &value);
    void setAccent(const QString &value);
    void setCustomAccent(const QString &value);
    void setOrganizeByGame(bool value);
    void setStorageLimitGb(int value);
    void setAutoCleanup(bool value);
    void setExportFormat(const QString &value);
    void setExportQuality(const QString &value);
    void setExportHeight(int value);
    void setDiscordCompress(bool value);
    void setDiscordTargetMb(int value);
    void setOverlayEnabled(bool value);
    void setOverlayClip(bool value);
    void setOverlayScreenshot(bool value);
    void setOverlayRecordStart(bool value);
    void setOverlayRecordStop(bool value);
    void setOverlayError(bool value);
    void setOverlayPosition(const QString &value);
    void setOverlayDurationMs(int value);
    void setOverlaySize(const QString &value);
    void setOverlayAnimation(const QString &value);
    void setOverlayUseAccent(bool value);
    void setOverlayOpacity(double value);
    void setSfxEnabled(bool value);
    void setSfxVolume(double value);
    void setSfxSound(const QString &value);
    void setSfxClip(bool value);
    void setSfxScreenshot(bool value);
    void setSfxRecordStart(bool value);
    void setSfxRecordStop(bool value);
    void setSfxError(bool value);
    void setDiscordEnabled(bool value);
    void setDiscordAutoConnect(bool value);
    void setDiscordShowGame(bool value);
    void setDiscordShowRecording(bool value);
    void setDiscordShowBuffer(bool value);
    void setDiscordShowEditing(bool value);

signals:
    void launchOnStartupChanged();
    void updatePromptChanged();
    void minimizeToTrayChanged();
    void notificationsChanged();
    void autoStartReplayChanged();
    void saveDirectoryChanged();
    void clipsDirectoryChanged();
    void replaySecondsChanged();
    void captureModeChanged();
    void monitorIndexChanged();
    void qualityChanged();
    void videoChanged();
    void encoderChanged();
    void codecChanged();
    void audioBitrateChanged();
    void showCursorChanged();
    void systemAudioChanged();
    void systemVolumeChanged();
    void outputDeviceIdChanged();
    void micEnabledChanged();
    void micVolumeChanged();
    void micDeviceIdChanged();
    void hotkeysChanged();
    void themeModeChanged();
    void accentChanged();
    void organizeByGameChanged();
    void storageLimitChanged();
    void exportChanged();
    void appNamesChanged();
    void overlayChanged();
    void sfxChanged();
    void discordChanged();

private:
    void scheduleSave();
    void saveNow();
    void markCustom();

    bool m_persist = true;
    bool m_loading = false;
    bool m_applyingPreset = false;

    bool m_launchOnStartup = false;
    bool m_updatePrompt = true;
    bool m_minimizeToTray = false;
    bool m_notifications = true;
    bool m_autoStartReplay = true;
    QString m_saveDirectory;

    int m_replaySeconds = 30;
    QString m_captureMode = QStringLiteral("auto");
    int m_monitorIndex = 0;
    QString m_quality = QStringLiteral("high");
    int m_width = 0;
    int m_height = 0;
    int m_fps = 60;
    int m_videoBitrateKbps = 15000;
    QString m_encoder = QStringLiteral("auto");
    QString m_codec = QStringLiteral("h264");
    int m_audioBitrateKbps = 192;
    bool m_showCursor = true;

    bool m_systemAudio = true;
    double m_systemVolume = 1.0;
    QString m_outputDeviceId;
    bool m_micEnabled = false;
    double m_micVolume = 1.0;
    QString m_micDeviceId;

    QString m_hotkeyClip = QStringLiteral("Ctrl+Shift+F9");
    QString m_hotkeyClip15;
    QString m_hotkeyClip30;
    QString m_hotkeyClip60;
    QString m_hotkeyClip120;
    QString m_hotkeyScreenshot = QStringLiteral("Ctrl+Shift+F10");
    QString m_hotkeyToggle = QStringLiteral("Ctrl+Shift+F8");
    QString m_hotkeyPause;

    QString m_themeMode = QStringLiteral("dark");
    QString m_accent = QStringLiteral("green");
    QString m_customAccent = QStringLiteral("#3DDC97");

    bool m_organizeByGame = true;
    int m_storageLimitGb = 0;
    bool m_autoCleanup = false;

    QString m_exportFormat = QStringLiteral("mp4");
    QString m_exportQuality = QStringLiteral("high");
    int m_exportHeight = 0;
    bool m_discordCompress = false;
    int m_discordTargetMb = 10;

    bool m_overlayEnabled = true;
    bool m_overlayClip = true;
    bool m_overlayScreenshot = true;
    bool m_overlayRecordStart = true;
    bool m_overlayRecordStop = true;
    bool m_overlayError = true;
    QString m_overlayPosition = QStringLiteral("topRight");
    int m_overlayDurationMs = 3000;
    QString m_overlaySize = QStringLiteral("normal");
    QString m_overlayAnimation = QStringLiteral("slide");
    bool m_overlayUseAccent = false;
    double m_overlayOpacity = 0.95;
    bool m_sfxEnabled = true;
    double m_sfxVolume = 0.7;
    QString m_sfxSound = QStringLiteral("nuclip");
    bool m_sfxClip = true;
    bool m_sfxScreenshot = true;
    bool m_sfxRecordStart = false;
    bool m_sfxRecordStop = false;
    bool m_sfxError = false;

    bool m_discordEnabled = false;
    bool m_discordAutoConnect = true;
    bool m_discordShowGame = true;
    bool m_discordShowRecording = true;
    bool m_discordShowBuffer = true;
    bool m_discordShowEditing = true;
    QHash<QString, QString> m_appNames;

    QTimer *m_saveTimer = nullptr;
};

} // namespace nuclip
