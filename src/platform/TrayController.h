#pragma once

#include <QObject>
#include <QVariant>

class QAudioOutput;
class QMediaPlayer;
class QSystemTrayIcon;
class QWindow;

namespace nuclip {

class Settings;
class RecordingEngine;

class TrayController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool quitting READ quitting NOTIFY quittingChanged)
    Q_PROPERTY(QVariantList sounds READ sounds CONSTANT)

public:
    explicit TrayController(Settings *settings, RecordingEngine *recording, QObject *parent = nullptr);

    bool quitting() const { return m_quitting; }
    QVariantList sounds() const;
    void attach(QWindow *window);
    Q_INVOKABLE void quit();
    Q_INVOKABLE void previewSound();

signals:
    void quittingChanged();
    void showRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildMenu();
    void primeSavedSound();
    void playEvent(const QString &event);
    void startPlayback();
    void onRecordingState();

    Settings *m_settings = nullptr;
    RecordingEngine *m_recording = nullptr;
    QMediaPlayer *m_savedSound = nullptr;
    QAudioOutput *m_savedAudio = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QWindow *m_window = nullptr;
    bool m_quitting = false;
    bool m_primed = false;
    bool m_priming = false;
    bool m_sessionLive = false;
    int m_lastState = 0;
    QString m_lastError;
};

} // namespace nuclip
