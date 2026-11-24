#include "library/ClipRepository.h"

#include "core/Format.h"
#include "platform/AppCatalog.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "platform/WinUtil.h"
#include "recording/CaptureCommand.h"
#include "recording/FfmpegJob.h"
#include "recording/FfmpegLocator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent>

#include <algorithm>

namespace nuclip {
namespace {

QString pathKey(const QString &path)
{
    return QFileInfo(path).absoluteFilePath().toLower();
}

QString folderGame(const QString &root, const QFileInfo &info)
{
    QDir dir = info.dir();
    if (dir.dirName().compare(QLatin1String("Exports"), Qt::CaseInsensitive) == 0)
        dir.cdUp();
    if (QDir::cleanPath(dir.absolutePath()).compare(QDir(root).absolutePath(), Qt::CaseInsensitive) == 0)
        return QStringLiteral("Desktop");
    return dir.dirName();
}

void applyMeta(ClipRecord &clip, const QJsonObject &object)
{
    if (!object.value(QStringLiteral("id")).toString().isEmpty())
        clip.id = object.value(QStringLiteral("id")).toString();
    if (!object.value(QStringLiteral("game")).toString().isEmpty())
        clip.game = object.value(QStringLiteral("game")).toString();
    clip.source = object.value(QStringLiteral("source")).toObject().toVariantMap();
    const QString created = object.value(QStringLiteral("created")).toString();
    if (!created.isEmpty())
        clip.created = QDateTime::fromString(created, Qt::ISODate);
    clip.duration = object.value(QStringLiteral("duration")).toDouble();
    clip.fps = object.value(QStringLiteral("fps")).toDouble();
    clip.width = object.value(QStringLiteral("width")).toInt();
    clip.height = object.value(QStringLiteral("height")).toInt();
    clip.thumbnail = object.value(QStringLiteral("thumbnail")).toString();
    clip.trimIn = object.value(QStringLiteral("trimIn")).toDouble();
    clip.trimOut = object.value(QStringLiteral("trimOut")).toDouble();
    const QJsonArray tracks = object.value(QStringLiteral("tracks")).toArray();
    for (const QJsonValue &value : tracks)
        clip.tracks.append(value.toObject().toVariantMap());
    if (!clip.thumbnail.isEmpty() && !QFileInfo::exists(clip.thumbnail))
        clip.thumbnail.clear();
}

QHash<QString, QJsonObject> loadMetaByPath()
{
    QHash<QString, QJsonObject> map;
    const QFileInfoList files = QDir(Paths::clipMetaDir()).entryInfoList({QStringLiteral("*.json")}, QDir::Files);
    for (const QFileInfo &info : files) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        const QString path = object.value(QStringLiteral("path")).toString();
        if (!path.isEmpty())
            map.insert(pathKey(path), object);
    }
    return map;
}

QVector<ClipRecord> scanTree(const QString &root)
{
    const QHash<QString, QJsonObject> saved = loadMetaByPath();
    QVector<ClipRecord> clips;
    QList<QString> pending{root};
    while (!pending.isEmpty()) {
        const QString directory = pending.takeFirst();
        const QFileInfoList entries = QDir(directory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &info : entries) {
            if (info.isDir()) {
                pending.append(info.absoluteFilePath());
                continue;
            }
            const QString fileName = info.fileName().toLower();
            if (fileName.endsWith(QLatin1String(".part.mp4")) || fileName.endsWith(QLatin1String(".best.mp4")))
                continue;
            const QString ext = info.suffix().toLower();
            if (ext != QLatin1String("mp4") && ext != QLatin1String("mkv") && ext != QLatin1String("webm"))
                continue;
            ClipRecord clip;
            clip.path = info.absoluteFilePath();
            clip.name = info.completeBaseName();
            clip.bytes = info.size();
            clip.created = info.birthTime().isValid() ? info.birthTime() : info.lastModified();
            clip.game = folderGame(root, info);
            clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const auto stored = saved.constFind(pathKey(clip.path));
            if (stored != saved.cend()) {
                applyMeta(clip, stored.value());
            } else {
                QFile sidecar(clip.path + QStringLiteral(".json"));
                if (sidecar.open(QIODevice::ReadOnly))
                    applyMeta(clip, QJsonDocument::fromJson(sidecar.readAll()).object());
            }
            clips.append(clip);
        }
    }
    return clips;
}

QString resolutionText(const ClipRecord &clip)
{
    if (clip.width <= 0 || clip.height <= 0)
        return QStringLiteral("—");
    return QStringLiteral("%1×%2").arg(clip.width).arg(clip.height);
}

} // namespace

ClipRepository::ClipRepository(Settings *settings, QObject *parent)
    : QAbstractListModel(parent)
    , m_settings(settings)
{
    m_ffmpeg = new FfmpegJob(this);
    connect(m_ffmpeg, &FfmpegJob::finished, this, &ClipRepository::finishJob);
    connect(m_settings, &Settings::clipsDirectoryChanged, this, &ClipRepository::rescan);
}

int ClipRepository::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_clips.size();
}

QVariant ClipRepository::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_clips.size())
        return {};
    const ClipRecord &clip = m_clips.at(index.row());
    switch (role) {
    case IdRole:
        return clip.id;
    case PathRole:
        return clip.path;
    case NameRole:
        return clip.name;
    case GameRole:
        return AppCatalog::displayForStored(clip.game);
    case DateRole:
        return clip.created.toString(QStringLiteral("MMM d, yyyy · h:mm AP"));
    case DateMsRole:
        return clip.created.toMSecsSinceEpoch();
    case DurationRole:
        return clip.duration;
    case DurationTextRole:
        return clip.duration > 0 ? formatDuration(clip.duration) : QStringLiteral("—");
    case WidthRole:
        return clip.width;
    case HeightRole:
        return clip.height;
    case ResolutionRole:
        return resolutionText(clip);
    case FpsRole:
        return clip.fps;
    case SizeRole:
        return clip.bytes;
    case SizeTextRole:
        return formatBytes(clip.bytes);
    case ThumbnailRole:
        return clip.thumbnail;
    case ThumbnailUrlRole:
        return clip.thumbnail.isEmpty() ? QString() : QUrl::fromLocalFile(clip.thumbnail).toString();
    default:
        return {};
    }
}

QHash<int, QByteArray> ClipRepository::roleNames() const
{
    return {{IdRole, "id"},
            {PathRole, "path"},
            {NameRole, "name"},
            {GameRole, "game"},
            {DateRole, "date"},
            {DateMsRole, "dateMs"},
            {DurationRole, "duration"},
            {DurationTextRole, "durationText"},
            {WidthRole, "width"},
            {HeightRole, "height"},
            {ResolutionRole, "resolution"},
            {FpsRole, "fps"},
            {SizeRole, "bytes"},
            {SizeTextRole, "sizeText"},
            {ThumbnailRole, "thumbnail"},
            {ThumbnailUrlRole, "thumbnailUrl"}};
}

void ClipRepository::rescan()
{
    const int generation = ++m_generation;
    const QString root = m_settings->clipsDirectory();
    QDir().mkpath(root);
    (void)QtConcurrent::run([this, generation, root] {
        const QVector<ClipRecord> found = scanTree(root);
        QMetaObject::invokeMethod(this, [this, generation, found] {
            if (generation != m_generation)
                return;
            applyScan(found);
        }, Qt::QueuedConnection);
    });
}

void ClipRepository::applyScan(const QVector<ClipRecord> &clips)
{
    beginResetModel();
    m_clips = clips;
    std::sort(m_clips.begin(), m_clips.end(), [](const ClipRecord &a, const ClipRecord &b) {
        return a.created > b.created;
    });
    endResetModel();
    rebuildGames();
    emit countChanged();
    for (const ClipRecord &clip : m_clips) {
        const QString legacy = clip.path + QStringLiteral(".json");
        if (QFileInfo::exists(legacy))
            writeSidecar(clip);
        if (clip.duration <= 0 || clip.width <= 0)
            queueProbe(clip.path);
    }
}

void ClipRepository::rebuildGames()
{
    QStringList games;
    for (const ClipRecord &clip : m_clips) {
        const QString shown = AppCatalog::displayForStored(clip.game);
        if (!shown.isEmpty() && !games.contains(shown))
            games.append(shown);
    }
    games.sort(Qt::CaseInsensitive);
    if (games == m_games)
        return;
    m_games = games;
    emit gamesChanged();
}

void ClipRepository::ingest(const QString &path, const QString &game, double durationHint, const QVariantMap &source)
{
    if (!QFileInfo::exists(path))
        return;
    if (indexOfPath(path) >= 0) {
        queueProbe(path);
        return;
    }
    ClipRecord clip;
    const QFileInfo info(path);
    clip.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    clip.path = info.absoluteFilePath();
    clip.name = info.completeBaseName();
    clip.game = game.isEmpty() ? QStringLiteral("Desktop") : game;
    clip.source = source;
    clip.created = QDateTime::currentDateTime();
    clip.duration = durationHint;
    clip.bytes = info.size();
    writeSidecar(clip);
    beginInsertRows(QModelIndex(), 0, 0);
    m_clips.prepend(clip);
    endInsertRows();
    rebuildGames();
    emit countChanged();
    queueProbe(clip.path);
    queueThumb(clip.id);
    enforceLimit();
}

ClipInfo ClipRepository::infoForPath(const QString &path) const
{
    ClipInfo info;
    const int index = indexOfPath(path);
    if (index < 0)
        return info;
    const ClipRecord &clip = m_clips.at(index);
    info.found = true;
    info.id = clip.id;
    info.game = AppCatalog::displayForStored(clip.game);
    info.duration = clip.duration;
    info.fps = clip.fps > 1 ? clip.fps : 60;
    info.width = clip.width;
    info.height = clip.height;
    info.trimIn = clip.trimIn;
    info.trimOut = clip.trimOut > 0 ? clip.trimOut : clip.duration;
    info.tracks = clip.tracks;
    return info;
}

void ClipRepository::storeEdits(const QString &path, double trimIn, double trimOut, const QVariantList &tracks)
{
    const int index = indexOfPath(path);
    if (index < 0)
        return;
    ClipRecord &clip = m_clips[index];
    clip.trimIn = std::max(0.0, trimIn);
    clip.trimOut = std::max(clip.trimIn, trimOut);
    clip.tracks = tracks;
    writeSidecar(clip);
}

bool ClipRepository::renameClip(const QString &id, const QString &newBaseName)
{
    const int index = indexOfId(id);
    if (index < 0)
        return false;
    const QString base = Paths::sanitizeName(newBaseName);
    if (base.isEmpty() || base == QLatin1String("Desktop")) {
        setError(QStringLiteral("Enter a file name."));
        return false;
    }
    ClipRecord clip = m_clips.at(index);
    const QFileInfo info(clip.path);
    const QString target = info.dir().filePath(base + QLatin1Char('.') + info.suffix());
    if (QFileInfo::exists(target) && QFileInfo(target).absoluteFilePath() != info.absoluteFilePath()) {
        setError(QStringLiteral("A clip with that name already exists."));
        return false;
    }
    if (!QFile::rename(clip.path, target)) {
        setError(QStringLiteral("Could not rename the clip."));
        return false;
    }
    QFile::remove(clip.path + QStringLiteral(".json"));
    clip.path = target;
    clip.name = base;
    writeSidecar(clip);
    m_clips[index] = clip;
    emit dataChanged(this->index(index, 0), this->index(index, 0));
    emit metadataUpdated(clip.path);
    return true;
}

bool ClipRepository::deleteClip(const QString &id)
{
    const int index = indexOfId(id);
    if (index < 0)
        return false;
    const ClipRecord clip = m_clips.at(index);
    QFile::moveToTrash(clip.path);
    QFile::remove(clip.path + QStringLiteral(".json"));
    QFile::remove(QDir(Paths::clipMetaDir()).filePath(clip.id + QStringLiteral(".json")));
    if (!clip.thumbnail.isEmpty())
        QFile::remove(clip.thumbnail);
    beginRemoveRows(QModelIndex(), index, index);
    m_clips.removeAt(index);
    endRemoveRows();
    rebuildGames();
    emit countChanged();
    return true;
}

void ClipRepository::reveal(const QString &id)
{
    const int index = indexOfId(id);
    if (index >= 0)
        WinUtil::revealInExplorer(m_clips.at(index).path);
}

void ClipRepository::requestThumbnail(const QString &id)
{
    const int index = indexOfId(id);
    if (index < 0)
        return;
    if (!m_clips.at(index).thumbnail.isEmpty() && QFileInfo::exists(m_clips.at(index).thumbnail))
        return;
    queueThumb(id);
}

void ClipRepository::queueProbe(const QString &path)
{
    for (const Job &job : m_jobs) {
        if (job.kind == Job::Probe && job.path == path)
            return;
    }
    if (m_jobActive && m_current.kind == Job::Probe && m_current.path == path)
        return;
    Job job;
    job.kind = Job::Probe;
    job.path = path;
    m_jobs.append(job);
    pumpJobs();
}

void ClipRepository::queueThumb(const QString &id)
{
    const int index = indexOfId(id);
    if (index < 0)
        return;
    for (const Job &job : m_jobs) {
        if (job.kind == Job::Thumb && job.id == id)
            return;
    }
    Job job;
    job.kind = Job::Thumb;
    job.id = id;
    job.path = m_clips.at(index).path;
    job.output = QDir(Paths::thumbnailDir()).filePath(id + QStringLiteral(".jpg"));
    m_jobs.append(job);
    pumpJobs();
}

void ClipRepository::pumpJobs()
{
    if (m_jobActive || m_jobs.isEmpty())
        return;
    const QString probe = ffprobeExecutable();
    const QString ffmpeg = ffmpegExecutable();
    if (probe.isEmpty() || ffmpeg.isEmpty())
        return;
    m_current = m_jobs.takeFirst();
    m_jobActive = true;
    if (m_current.kind == Job::Probe) {
        m_ffmpeg->start(probe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-print_format"),
                                QStringLiteral("json"), QStringLiteral("-show_format"), QStringLiteral("-show_streams"),
                                m_current.path});
    } else {
        m_ffmpeg->start(ffmpeg, thumbnailArguments(m_current.path, m_current.output));
    }
}

void ClipRepository::finishJob(int exitCode)
{
    const Job job = m_current;
    m_jobActive = false;
    if (job.kind == Job::Thumb) {
        const int index = indexOfId(job.id);
        if (index >= 0 && exitCode == 0 && QFileInfo::exists(job.output)) {
            m_clips[index].thumbnail = job.output;
            writeSidecar(m_clips.at(index));
            emit dataChanged(this->index(index, 0), this->index(index, 0), {ThumbnailRole, ThumbnailUrlRole});
        }
    } else if (exitCode == 0) {
        const QJsonObject root = QJsonDocument::fromJson(m_ffmpeg->stdoutText().toUtf8()).object();
        const int index = indexOfPath(job.path);
        if (index >= 0) {
            ClipRecord &clip = m_clips[index];
            const QJsonObject format = root.value(QStringLiteral("format")).toObject();
            clip.duration = format.value(QStringLiteral("duration")).toString().toDouble();
            if (format.contains(QStringLiteral("size")))
                clip.bytes = format.value(QStringLiteral("size")).toString().toLongLong();
            if (clip.bytes <= 0)
                clip.bytes = QFileInfo(clip.path).size();
            QVariantList tracks;
            const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
            int audioIndex = 0;
            for (const QJsonValue &value : streams) {
                const QJsonObject stream = value.toObject();
                const QString type = stream.value(QStringLiteral("codec_type")).toString();
                if (type == QLatin1String("video") && clip.width <= 0) {
                    clip.width = stream.value(QStringLiteral("width")).toInt();
                    clip.height = stream.value(QStringLiteral("height")).toInt();
                    const QString rate = stream.value(QStringLiteral("avg_frame_rate")).toString();
                    const QStringList parts = rate.split(QLatin1Char('/'));
                    if (parts.size() == 2 && parts.at(1).toDouble() != 0)
                        clip.fps = parts.at(0).toDouble() / parts.at(1).toDouble();
                    else
                        clip.fps = rate.toDouble();
                } else if (type == QLatin1String("audio")) {
                    QVariantMap track;
                    track.insert(QStringLiteral("index"), audioIndex++);
                    track.insert(QStringLiteral("codec"), stream.value(QStringLiteral("codec_name")).toString());
                    track.insert(QStringLiteral("channels"), stream.value(QStringLiteral("channels")).toInt());
                    track.insert(QStringLiteral("sampleRate"), stream.value(QStringLiteral("sample_rate")).toString().toInt());
                    track.insert(QStringLiteral("enabled"), true);
                    track.insert(QStringLiteral("volume"), 1.0);
                    tracks.append(track);
                }
            }
            if (clip.tracks.isEmpty())
                clip.tracks = tracks;
            if (clip.trimOut <= 0)
                clip.trimOut = clip.duration;
            writeSidecar(clip);
            emit dataChanged(this->index(index, 0), this->index(index, 0));
            emit metadataUpdated(clip.path);
        }
    }
    pumpJobs();
}

void ClipRepository::writeSidecar(const ClipRecord &clip) const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), clip.id);
    object.insert(QStringLiteral("game"), clip.game);
    if (!clip.source.isEmpty())
        object.insert(QStringLiteral("source"), QJsonObject::fromVariantMap(clip.source));
    object.insert(QStringLiteral("created"), clip.created.toString(Qt::ISODate));
    object.insert(QStringLiteral("duration"), clip.duration);
    object.insert(QStringLiteral("fps"), clip.fps);
    object.insert(QStringLiteral("width"), clip.width);
    object.insert(QStringLiteral("height"), clip.height);
    object.insert(QStringLiteral("thumbnail"), clip.thumbnail);
    object.insert(QStringLiteral("trimIn"), clip.trimIn);
    object.insert(QStringLiteral("trimOut"), clip.trimOut);
    QJsonArray tracks;
    for (const QVariant &track : clip.tracks)
        tracks.append(QJsonObject::fromVariantMap(track.toMap()));
    object.insert(QStringLiteral("tracks"), tracks);
    object.insert(QStringLiteral("path"), QFileInfo(clip.path).absoluteFilePath());
    QFile file(QDir(Paths::clipMetaDir()).filePath(clip.id + QStringLiteral(".json")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.close();
        QFile::remove(clip.path + QStringLiteral(".json"));
    }
}

void ClipRepository::enforceLimit()
{
    if (!m_settings->autoCleanup() || m_settings->storageLimitGb() <= 0)
        return;
    const qint64 limit = qint64(m_settings->storageLimitGb()) * 1024 * 1024 * 1024;
    qint64 total = 0;
    for (const ClipRecord &clip : m_clips)
        total += clip.bytes;
    if (total <= limit)
        return;
    QVector<int> order;
    order.reserve(m_clips.size());
    for (int i = 0; i < m_clips.size(); ++i)
        order.append(i);
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        return m_clips.at(a).created < m_clips.at(b).created;
    });
    QStringList doomed;
    for (int index : order) {
        if (total <= limit || doomed.size() >= m_clips.size() - 1)
            break;
        total -= m_clips.at(index).bytes;
        doomed.append(m_clips.at(index).id);
    }
    for (const QString &id : doomed)
        deleteClip(id);
    Log::info(QStringLiteral("Storage limit removed %1 old clip(s).").arg(doomed.size()));
}

int ClipRepository::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_clips.size(); ++i) {
        if (m_clips.at(i).id == id)
            return i;
    }
    return -1;
}

int ClipRepository::indexOfPath(const QString &path) const
{
    const QString clean = QFileInfo(path).absoluteFilePath();
    for (int i = 0; i < m_clips.size(); ++i) {
        if (QFileInfo(m_clips.at(i).path).absoluteFilePath() == clean)
            return i;
    }
    return -1;
}

void ClipRepository::setError(const QString &error)
{
    m_lastError = error;
    emit lastErrorChanged();
    Log::warn(error);
}

} // namespace nuclip
