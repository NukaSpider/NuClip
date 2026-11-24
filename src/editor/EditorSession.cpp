#include "editor/EditorSession.h"

#include "core/Paths.h"
#include "core/Settings.h"
#include "export/Exporter.h"
#include "library/ClipRepository.h"
#include "recording/FfmpegJob.h"
#include "recording/FfmpegLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include <algorithm>

namespace nuclip {

TrackModel::TrackModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tracks.size();
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return {};
    const QVariantMap track = m_tracks.at(index.row()).toMap();
    switch (role) {
    case IndexRole:
        return track.value(QStringLiteral("index")).toInt();
    case CodecRole:
        return track.value(QStringLiteral("codec")).toString();
    case ChannelsRole:
        return track.value(QStringLiteral("channels")).toInt();
    case SampleRateRole:
        return track.value(QStringLiteral("sampleRate")).toInt();
    case EnabledRole:
        return track.value(QStringLiteral("enabled")).toBool();
    case VolumeRole:
        return track.value(QStringLiteral("volume")).toDouble();
    case LabelRole: {
        const int channels = track.value(QStringLiteral("channels")).toInt();
        const QString layout = channels >= 2 ? QStringLiteral("Stereo") : QStringLiteral("Mono");
        return QStringLiteral("Track %1 · %2 · %3")
            .arg(track.value(QStringLiteral("index")).toInt() + 1)
            .arg(track.value(QStringLiteral("codec")).toString().toUpper())
            .arg(layout);
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> TrackModel::roleNames() const
{
    return {{IndexRole, "streamIndex"}, {CodecRole, "codec"}, {ChannelsRole, "channels"},
            {SampleRateRole, "sampleRate"}, {EnabledRole, "enabled"}, {VolumeRole, "volume"}, {LabelRole, "label"}};
}

void TrackModel::setTracks(const QVariantList &tracks)
{
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
}

int TrackModel::rowForStream(int streamIndex) const
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).toMap().value(QStringLiteral("index")).toInt() == streamIndex)
            return i;
    }
    return -1;
}

bool TrackModel::setEnabled(int streamIndex, bool enabled)
{
    const int row = rowForStream(streamIndex);
    if (row < 0)
        return false;
    QVariantMap track = m_tracks[row].toMap();
    track.insert(QStringLiteral("enabled"), enabled);
    m_tracks[row] = track;
    emit dataChanged(index(row, 0), index(row, 0), {EnabledRole});
    return true;
}

bool TrackModel::setVolume(int streamIndex, double volume)
{
    const int row = rowForStream(streamIndex);
    if (row < 0)
        return false;
    QVariantMap track = m_tracks[row].toMap();
    track.insert(QStringLiteral("volume"), std::clamp(volume, 0.0, 2.0));
    m_tracks[row] = track;
    emit dataChanged(index(row, 0), index(row, 0), {VolumeRole});
    return true;
}

EditorSession::EditorSession(Settings *settings, ClipRepository *library, Exporter *exporter, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_library(library)
    , m_exporter(exporter)
    , m_tracks(new TrackModel(this))
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(300);
    connect(m_saveTimer, &QTimer::timeout, this, [this] {
        if (!m_path.isEmpty())
            m_library->storeEdits(m_path, m_trimIn, m_trimOut, m_tracks->tracks());
    });
    connect(m_library, &ClipRepository::metadataUpdated, this, [this](const QString &path) {
        if (path == m_path)
            applyInfo();
    });
}

QUrl EditorSession::mediaUrl() const
{
    return m_path.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_path);
}

QString EditorSession::resolution() const
{
    if (m_width <= 0 || m_height <= 0)
        return QStringLiteral("—");
    return QStringLiteral("%1×%2").arg(m_width).arg(m_height);
}

void EditorSession::openPath(const QString &path)
{
    m_loading = true;
    m_path = QFileInfo(path).absoluteFilePath();
    m_name = QFileInfo(m_path).completeBaseName();
    applyInfo();
    m_loading = false;
    emit sourceChanged();
    emit trimChanged();
    requestWaveforms();
}

QString EditorSession::waveformFor(int audioIndex) const
{
    return m_waveFiles.value(audioIndex);
}

namespace {

QString waveFileFor(const QString &path, int stream)
{
    return Paths::thumbnailDir() + QStringLiteral("/wave-") + QString::number(qHash(path))
        + QStringLiteral("-") + QString::number(stream) + QStringLiteral(".png");
}

} // namespace

void EditorSession::requestWaveforms()
{
    ++m_waveToken;
    m_waveQueue.clear();
    m_waveFiles.clear();
    m_waveStream = -1;
    ++m_waveRevision;
    emit waveformChanged();
    if (m_path.isEmpty())
        return;
    const ClipInfo info = m_library->infoForPath(m_path);
    for (const QVariant &value : info.tracks) {
        m_waveQueue.append(value.toMap().value(QStringLiteral("index")).toInt());
        if (m_waveQueue.size() >= 4)
            break;
    }
    if (m_waveQueue.isEmpty())
        m_waveQueue.append(0);
    if (!m_wave) {
        m_wave = new FfmpegJob(this);
        connect(m_wave, &FfmpegJob::finished, this, [this](int code) {
            if (m_waveExpect != m_waveToken) {
                if (!m_wave->running())
                    pumpWaveform();
                return;
            }
            if (code == 0 && m_waveStream >= 0 && QFileInfo(m_waveOutput).size() > 80)
                m_waveFiles.insert(m_waveStream, QUrl::fromLocalFile(m_waveOutput).toString());
            m_waveStream = -1;
            ++m_waveRevision;
            emit waveformChanged();
            pumpWaveform();
        });
    } else if (m_wave->running()) {
        m_wave->kill();
        return;
    }
    pumpWaveform();
}

void EditorSession::pumpWaveform()
{
    if (m_waveQueue.isEmpty() || !m_wave || m_path.isEmpty() || m_wave->running())
        return;
    const int stream = m_waveQueue.takeFirst();
    const QString file = waveFileFor(m_path, stream);
    if (QFileInfo::exists(file) && QFileInfo(file).size() > 80) {
        m_waveFiles.insert(stream, QUrl::fromLocalFile(file).toString());
        ++m_waveRevision;
        emit waveformChanged();
        pumpWaveform();
        return;
    }
    QDir().mkpath(Paths::thumbnailDir());
    m_waveStream = stream;
    m_waveOutput = file;
    m_waveExpect = m_waveToken;
    const QString filter = QStringLiteral("[0:a:%1]aformat=channel_layouts=mono,showwavespic=s=960x40:colors=0x35D69A")
                               .arg(stream);
    m_wave->start(ffmpegExecutable(), {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-y"),
        QStringLiteral("-i"), m_path,
        QStringLiteral("-filter_complex"), filter,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        file
    });
}

void EditorSession::applyInfo()
{
    const ClipInfo info = m_library->infoForPath(m_path);
    if (!info.found)
        return;
    const bool trimDirty = m_saveTimer->isActive();
    m_game = info.game;
    m_duration = info.duration;
    m_fps = info.fps > 1 ? info.fps : 60;
    m_width = info.width;
    m_height = info.height;
    if (!trimDirty) {
        m_trimIn = std::max(0.0, info.trimIn);
        m_trimOut = info.trimOut > 0 ? info.trimOut : info.duration;
    }
    if (m_tracks->tracks().isEmpty() && !info.tracks.isEmpty())
        m_tracks->setTracks(info.tracks);
    if (!m_loading) {
        emit sourceChanged();
        emit trimChanged();
    }
}

void EditorSession::setTrimIn(double value)
{
    const double frame = 1.0 / std::max(1.0, m_fps);
    value = std::clamp(value, 0.0, std::max(0.0, m_trimOut - frame));
    if (qAbs(m_trimIn - value) < 0.0001)
        return;
    m_trimIn = value;
    emit trimChanged();
    scheduleSave();
}

void EditorSession::setTrimOut(double value)
{
    const double frame = 1.0 / std::max(1.0, m_fps);
    const double upper = m_duration > 0 ? m_duration : value;
    value = std::clamp(value, m_trimIn + frame, std::max(m_trimIn + frame, upper));
    if (qAbs(m_trimOut - value) < 0.0001)
        return;
    m_trimOut = value;
    emit trimChanged();
    scheduleSave();
}

void EditorSession::setTrackEnabled(int streamIndex, bool enabled)
{
    if (m_tracks->setEnabled(streamIndex, enabled))
        scheduleSave();
}

void EditorSession::setTrackVolume(int streamIndex, double volume)
{
    if (m_tracks->setVolume(streamIndex, volume))
        scheduleSave();
}

void EditorSession::exportCurrent()
{
    if (m_path.isEmpty())
        return;
    m_saveTimer->stop();
    m_library->storeEdits(m_path, m_trimIn, m_trimOut, m_tracks->tracks());
    ExportRequest request;
    request.source = m_path;
    request.game = m_game;
    request.trimIn = m_trimIn;
    request.trimOut = m_trimOut > m_trimIn ? m_trimOut : m_duration;
    request.sourceHeight = m_height;
    request.tracks = m_tracks->tracks();
    m_exporter->start(request);
}

void EditorSession::scheduleSave()
{
    if (!m_loading)
        m_saveTimer->start();
}

} // namespace nuclip
