#pragma once

#include "audio/WasapiInput.h"

#include <QObject>
#include <QStringList>
#include <memory>

class QTimer;

namespace nuclip {

class Settings;
class DeviceCatalog;
class GameDetector;
class ClipRepository;
class FfmpegJob;

class RecordingEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY stateChanged)
    Q_PROPERTY(bool booting READ booting NOTIFY stateChanged)
    Q_PROPERTY(QString mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(double elapsed READ elapsed NOTIFY elapsedChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY warningChanged)
    Q_PROPERTY(QString details READ details NOTIFY detailsChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(QString activeEncoder READ activeEncoder NOTIFY stateChanged)
    Q_PROPERTY(QString activeMonitor READ activeMonitor NOTIFY stateChanged)
    Q_PROPERTY(qreal systemLevel READ systemLevel NOTIFY levelsChanged)
    Q_PROPERTY(qreal micLevel READ micLevel NOTIFY levelsChanged)
    Q_PROPERTY(int bufferSeconds READ bufferSeconds NOTIFY bufferChanged)

public:
    enum State { Idle = 0, Starting, Recording, Paused, Stopping, Error };
    Q_ENUM(State)

    explicit RecordingEngine(Settings *settings, DeviceCatalog *devices, GameDetector *detector,
                             ClipRepository *library, QObject *parent = nullptr);
    ~RecordingEngine() override;

    int state() const { return int(m_state); }
    QString stateText() const;
    bool booting() const { return m_booting; }
    QString mode() const;
    double elapsed() const { return m_elapsed; }
    QString error() const { return m_error; }
    QString warning() const { return m_warning; }
    QString details() const;
    bool saving() const { return m_saving; }
    QString activeEncoder() const { return m_activeEncoder; }
    QString activeMonitor() const { return m_activeMonitor; }
    qreal systemLevel() const { return m_systemLevel; }
    qreal micLevel() const { return m_micLevel; }
    int bufferSeconds() const;

    Q_INVOKABLE void startReplay();
    Q_INVOKABLE void startSession();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void saveClip(int seconds);
    Q_INVOKABLE void screenshot();
    Q_INVOKABLE void shutdown();
    void startup();

signals:
    void stateChanged();
    void elapsedChanged();
    void errorChanged();
    void warningChanged();
    void detailsChanged();
    void savingChanged();
    void levelsChanged();
    void bufferChanged();
    void clipSaved(const QString &path, const QString &game, double seconds);
    void screenshotSaved(const QString &path);
    void captureAccepted();
    void clipAccepted(const QString &game, double seconds);
    void screenshotAccepted();

private:
    enum class Mode { None, Replay, Session };
    enum class Intent { None, Stop, Pause, Recycle };

    void beginStart(Mode mode, bool resume);
    void launch(const QString &encoderId);
    void onCaptureFinished(int code);
    void requestStopProcess();
    void stopAudio();
    void cleanupSegments();
    void watchSegments();
    void recycleCapture();
    void maybeAutoStart();
    void removeSegments();
    QStringList listSegments() const;
    int nextSegmentIndex() const;
    int chooseMonitor() const;
    QString gameName() const;
    QString makeClipPath(const QString &game) const;
    void saveClipInternal(int seconds, bool acknowledge);
    void startConcat(const QStringList &files, const QString &output, const QString &game, double durationHint);
    void onConcatFinished(int code);
    void setState(State state);
    void setError(const QString &error);
    void setWarning(const QString &warning);
    void tick();

    Settings *m_settings = nullptr;
    DeviceCatalog *m_devices = nullptr;
    GameDetector *m_detector = nullptr;
    ClipRepository *m_library = nullptr;
    FfmpegJob *m_capture = nullptr;
    FfmpegJob *m_concat = nullptr;
    FfmpegJob *m_shot = nullptr;
    std::unique_ptr<WasapiInput> m_system;
    std::unique_ptr<WasapiInput> m_mic;
    QTimer *m_tickTimer = nullptr;

    State m_state = Idle;
    Mode m_mode = Mode::None;
    Intent m_intent = Intent::None;
    int m_token = 0;
    bool m_retried = false;
    bool m_busyRetried = false;
    bool m_keepSegments = false;
    bool m_saving = false;
    bool m_booting = true;
    bool m_sessionSave = false;
    int m_pendingSeconds = 0;
    QString m_error;
    QString m_warning;
    QString m_activeEncoder;
    QString m_activeMonitor;
    QString m_codecOverride;
    QString m_bufferDir;
    QString m_concatOutput;
    QString m_concatGame;
    double m_concatDuration = 0;
    QString m_shotPath;
    double m_elapsed = 0;
    double m_elapsedAccum = 0;
    qint64 m_clockStart = 0;
    qreal m_systemLevel = 0;
    qreal m_micLevel = 0;
    int m_cleanupTicks = 0;
    int m_pipeGeneration = 0;
    int m_segmentWatch = -1;
    int m_recycleStreak = 0;
    qint64 m_segmentWatchAt = 0;
    qint64 m_recycleAfter = 0;
};

} // namespace nuclip
