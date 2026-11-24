#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QUrl>
#include <QVariantList>

class QTimer;

namespace nuclip {

class Settings;
class ClipRepository;
class Exporter;
struct ExportRequest;

class TrackModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { IndexRole = Qt::UserRole + 1, CodecRole, ChannelsRole, SampleRateRole, EnabledRole, VolumeRole, LabelRole };
    explicit TrackModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setTracks(const QVariantList &tracks);
    QVariantList tracks() const { return m_tracks; }
    bool setEnabled(int streamIndex, bool enabled);
    bool setVolume(int streamIndex, double volume);

private:
    int rowForStream(int streamIndex) const;
    QVariantList m_tracks;
};

class EditorSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY sourceChanged)
    Q_PROPERTY(QUrl mediaUrl READ mediaUrl NOTIFY sourceChanged)
    Q_PROPERTY(QString name READ name NOTIFY sourceChanged)
    Q_PROPERTY(QString game READ game NOTIFY sourceChanged)
    Q_PROPERTY(double duration READ duration NOTIFY sourceChanged)
    Q_PROPERTY(double fps READ fps NOTIFY sourceChanged)
    Q_PROPERTY(int width READ width NOTIFY sourceChanged)
    Q_PROPERTY(int height READ height NOTIFY sourceChanged)
    Q_PROPERTY(QString resolution READ resolution NOTIFY sourceChanged)
    Q_PROPERTY(double trimIn READ trimIn WRITE setTrimIn NOTIFY trimChanged)
    Q_PROPERTY(double trimOut READ trimOut WRITE setTrimOut NOTIFY trimChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY sourceChanged)
    Q_PROPERTY(TrackModel *tracks READ tracks CONSTANT)
    Q_PROPERTY(int waveformRevision READ waveformRevision NOTIFY waveformChanged)

public:
    explicit EditorSession(Settings *settings, ClipRepository *library, Exporter *exporter, QObject *parent = nullptr);

    QString path() const { return m_path; }
    QUrl mediaUrl() const;
    QString name() const { return m_name; }
    QString game() const { return m_game; }
    double duration() const { return m_duration; }
    double fps() const { return m_fps; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    QString resolution() const;
    double trimIn() const { return m_trimIn; }
    double trimOut() const { return m_trimOut; }
    bool ready() const { return !m_path.isEmpty(); }
    TrackModel *tracks() const { return m_tracks; }

    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void setTrackEnabled(int streamIndex, bool enabled);
    Q_INVOKABLE void setTrackVolume(int streamIndex, double volume);
    Q_INVOKABLE void exportCurrent();
    Q_INVOKABLE QString waveformFor(int audioIndex) const;
    int waveformRevision() const { return m_waveRevision; }
    void setTrimIn(double value);
    void setTrimOut(double value);

signals:
    void sourceChanged();
    void trimChanged();
    void waveformChanged();

private:
    void applyInfo();
    void scheduleSave();
    void requestWaveforms();
    void pumpWaveform();

    Settings *m_settings = nullptr;
    ClipRepository *m_library = nullptr;
    Exporter *m_exporter = nullptr;
    TrackModel *m_tracks = nullptr;
    QTimer *m_saveTimer = nullptr;
    QString m_path;
    QString m_name;
    QString m_game;
    double m_duration = 0;
    double m_fps = 60;
    int m_width = 0;
    int m_height = 0;
    double m_trimIn = 0;
    double m_trimOut = 0;
    bool m_loading = false;
    class FfmpegJob *m_wave = nullptr;
    QList<int> m_waveQueue;
    QHash<int, QString> m_waveFiles;
    int m_waveStream = -1;
    int m_waveToken = 0;
    int m_waveExpect = 0;
    int m_waveRevision = 0;
    QString m_waveOutput;
};

} // namespace nuclip
