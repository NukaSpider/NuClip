#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVariantList>

namespace nuclip {

class Settings;

struct ClipRecord {
    QString id;
    QString path;
    QString name;
    QString game;
    QDateTime created;
    double duration = 0;
    double fps = 0;
    int width = 0;
    int height = 0;
    qint64 bytes = 0;
    QString thumbnail;
    double trimIn = 0;
    double trimOut = 0;
    QVariantList tracks;
    QVariantMap source;
};

struct ClipInfo {
    bool found = false;
    QString id;
    QString game;
    double duration = 0;
    double fps = 60;
    int width = 0;
    int height = 0;
    double trimIn = 0;
    double trimOut = 0;
    QVariantList tracks;
};

class ClipRepository : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        NameRole,
        GameRole,
        DateRole,
        DateMsRole,
        DurationRole,
        DurationTextRole,
        WidthRole,
        HeightRole,
        ResolutionRole,
        FpsRole,
        SizeRole,
        SizeTextRole,
        ThumbnailRole,
        ThumbnailUrlRole
    };
    Q_ENUM(Role)

    explicit ClipRepository(Settings *settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_clips.size(); }
    QStringList games() const { return m_games; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void rescan();
    Q_INVOKABLE void requestThumbnail(const QString &id);
    Q_INVOKABLE bool renameClip(const QString &id, const QString &newBaseName);
    Q_INVOKABLE bool deleteClip(const QString &id);
    Q_INVOKABLE void reveal(const QString &id);
    ClipInfo infoForPath(const QString &path) const;
    void ingest(const QString &path, const QString &game, double durationHint, const QVariantMap &source = {});
    void storeEdits(const QString &path, double trimIn, double trimOut, const QVariantList &tracks);

signals:
    void countChanged();
    void gamesChanged();
    void lastErrorChanged();
    void metadataUpdated(const QString &path);

private:
    void applyScan(const QVector<ClipRecord> &clips);
    void rebuildGames();
    void queueProbe(const QString &path);
    void queueThumb(const QString &id);
    void pumpJobs();
    void finishJob(int exitCode);
    void writeSidecar(const ClipRecord &clip) const;
    void enforceLimit();
    int indexOfId(const QString &id) const;
    int indexOfPath(const QString &path) const;
    void setError(const QString &error);

    Settings *m_settings = nullptr;
    QVector<ClipRecord> m_clips;
    QStringList m_games;
    QString m_lastError;
    int m_generation = 0;
    struct Job {
        enum Kind { Probe, Thumb } kind;
        QString path;
        QString id;
        QString output;
    };
    QList<Job> m_jobs;
    bool m_jobActive = false;
    class FfmpegJob *m_ffmpeg = nullptr;
    Job m_current;
};

} // namespace nuclip
