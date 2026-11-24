#pragma once

#include <QObject>
#include <QVariantList>

namespace nuclip {

class Settings;
class DeviceCatalog;
class ClipRepository;

struct ExportRequest {
    QString source;
    QString game;
    double trimIn = 0;
    double trimOut = 0;
    int sourceHeight = 0;
    QVariantList tracks;
};

class Exporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString eta READ eta NOTIFY changed)
    Q_PROPERTY(QString outputPath READ outputPath NOTIFY changed)
    Q_PROPERTY(QString outputSize READ outputSize NOTIFY changed)
    Q_PROPERTY(QString discordPath READ discordPath NOTIFY changed)
    Q_PROPERTY(QString discordSize READ discordSize NOTIFY changed)

public:
    explicit Exporter(Settings *settings, DeviceCatalog *devices, ClipRepository *library, QObject *parent = nullptr);

    bool running() const { return m_running; }
    double progress() const { return m_progress; }
    QString status() const { return m_status; }
    QString error() const { return m_error; }
    QString eta() const { return m_eta; }
    QString outputPath() const { return m_outputPath; }
    QString outputSize() const { return m_outputSize; }
    QString discordPath() const { return m_discordPath; }
    QString discordSize() const { return m_discordSize; }

    void start(const ExportRequest &request);
    Q_INVOKABLE void cancel();

signals:
    void changed();

private:
    void beginQuality();
    void beginDiscord();
    void finishQuality(int exitCode);
    void finishDiscordAttempt(int exitCode);
    QStringList qualityArguments(const QString &output, int videoBitrateKbps, bool discord) const;
    void publish();
    static QString uniqueFile(const QString &directory, const QString &base, const QString &extension);

    Settings *m_settings = nullptr;
    DeviceCatalog *m_devices = nullptr;
    ClipRepository *m_library = nullptr;
    class FfmpegJob *m_job = nullptr;
    ExportRequest m_request;
    bool m_running = false;
    bool m_cancel = false;
    bool m_discordPhase = false;
    int m_discordTries = 0;
    int m_discordBitrate = 0;
    qint64 m_bestDiscordBytes = 0;
    QString m_bestDiscordPath;
    double m_progress = 0;
    QString m_status;
    QString m_error;
    QString m_eta;
    QString m_outputPath;
    QString m_outputSize;
    QString m_discordPath;
    QString m_discordSize;
    QString m_encoder;
};

} // namespace nuclip
