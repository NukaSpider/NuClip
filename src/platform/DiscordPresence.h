#pragma once

#include <QObject>
#include <QVariantList>

class QLocalSocket;
class QTimer;

namespace nuclip {

class Settings;
class GameDetector;
class RecordingEngine;
class EditorSession;
class Exporter;

// Native Discord Rich Presence over the desktop client's local IPC pipe.
// This is the unauthenticated RPC path used by Discord's Social SDK on Windows.
// No browser, no Electron, and no application secret.
class DiscordPresence : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    Q_PROPERTY(QString previewDetails READ previewDetails NOTIFY previewChanged)
    Q_PROPERTY(QString previewState READ previewState NOTIFY previewChanged)
    Q_PROPERTY(QString largeAsset READ largeAsset NOTIFY previewChanged)
    Q_PROPERTY(QString smallAsset READ smallAsset NOTIFY previewChanged)
    Q_PROPERTY(QVariantList assets READ assets CONSTANT)

public:
    DiscordPresence(Settings *settings, GameDetector *detector, RecordingEngine *recording,
                    EditorSession *editor, Exporter *exporter, QObject *parent = nullptr);

    QString status() const { return m_status; }
    bool connected() const { return m_ready; }
    QString previewDetails() const { return m_details; }
    QString previewState() const { return m_state; }
    QString largeAsset() const { return m_largeAsset; }
    QString smallAsset() const { return m_smallAsset; }
    QVariantList assets() const;

    void startup();
    void shutdown();
    Q_INVOKABLE void connectNow();
    Q_INVOKABLE void disconnectNow();
    Q_INVOKABLE void setSurface(int page);

signals:
    void statusChanged();
    void previewChanged();

private:
    struct Activity {
        QString details;
        QString state;
        QString largeImage;
        QString smallImage;
        QString kind;
        bool timed = false;
    };

    Activity compose() const;
    QString applicationId() const;
    bool wantsLink() const;
    void scheduleApply();
    void apply();
    void beginConnect();
    void tryPipe(int index);
    void sendHandshake();
    void sendActivity(const Activity &activity, bool clear);
    void sendFrame(quint32 opcode, const QByteArray &payload);
    void readFrames();
    void onFrame(quint32 opcode, const QByteArray &payload);
    void setStatus(const QString &status);
    void scheduleRetry();
    void failLink(const QString &status, bool retry);

    Settings *m_settings = nullptr;
    GameDetector *m_detector = nullptr;
    RecordingEngine *m_recording = nullptr;
    EditorSession *m_editor = nullptr;
    Exporter *m_exporter = nullptr;
    QLocalSocket *m_socket = nullptr;
    QTimer *m_apply = nullptr;
    QTimer *m_retry = nullptr;
    QTimer *m_probe = nullptr;
    QByteArray m_buffer;
    QByteArray m_lastPayload;
    QString m_status = QStringLiteral("Off");
    QString m_details;
    QString m_state;
    QString m_largeAsset = QStringLiteral("nuclip_logo");
    QString m_smallAsset = QStringLiteral("nuclip_icon");
    QString m_kind;
    qint64 m_startedAt = 0;
    qint64 m_lastSendMs = 0;
    int m_pipe = 0;
    int m_surface = 0;
    bool m_ready = false;
    bool m_stopped = false;
    bool m_holdOff = true;
    bool m_wasEnabled = false;
    bool m_wasAutomatic = true;
    bool m_loggedAbsent = false;
    bool m_closing = false;
    bool m_rejected = false;
};

QString discordPresenceSelfCheck();

} // namespace nuclip
