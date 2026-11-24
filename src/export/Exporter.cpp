#include "export/Exporter.h"

#include "core/Format.h"
#include "core/Settings.h"
#include "library/ClipRepository.h"
#include "platform/DeviceCatalog.h"
#include "recording/FfmpegJob.h"
#include "recording/FfmpegLocator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace nuclip {
namespace {

QStringList encoderArgs(const QString &encoder, int bitrateKbps)
{
    const QString rate = QString::number(std::max(120, bitrateKbps)) + QLatin1Char('k');
    const QString peak = QString::number(std::max(120, int(bitrateKbps * 1.2))) + QLatin1Char('k');
    if (encoder == QLatin1String("nvenc")) {
        return {QStringLiteral("-c:v"), QStringLiteral("h264_nvenc"), QStringLiteral("-preset"), QStringLiteral("p6"),
                QStringLiteral("-rc"), QStringLiteral("vbr"), QStringLiteral("-b:v"), rate, QStringLiteral("-maxrate"),
                peak, QStringLiteral("-bufsize"), rate};
    }
    if (encoder == QLatin1String("amf")) {
        return {QStringLiteral("-c:v"), QStringLiteral("h264_amf"), QStringLiteral("-quality"), QStringLiteral("quality"),
                QStringLiteral("-rc"), QStringLiteral("vbr_peak"), QStringLiteral("-b:v"), rate,
                QStringLiteral("-maxrate"), peak};
    }
    if (encoder == QLatin1String("qsv")) {
        return {QStringLiteral("-c:v"), QStringLiteral("h264_qsv"), QStringLiteral("-b:v"), rate,
                QStringLiteral("-maxrate"), peak};
    }
    return {QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-preset"), QStringLiteral("medium"),
            QStringLiteral("-b:v"), rate, QStringLiteral("-maxrate"), peak, QStringLiteral("-bufsize"), rate,
            QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p")};
}

} // namespace

Exporter::Exporter(Settings *settings, DeviceCatalog *devices, ClipRepository *library, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_devices(devices)
    , m_library(library)
{
    m_job = new FfmpegJob(this);
    connect(m_job, &FfmpegJob::progress, this, [this](double seconds, double speed) {
        const double span = std::max(0.1, m_request.trimOut - m_request.trimIn);
        const double phase = std::clamp(seconds / span, 0.0, 1.0);
        m_progress = m_discordPhase ? (0.7 + phase * 0.3) : (m_settings->discordCompress() ? phase * 0.7 : phase);
        if (speed > 0.05) {
            const double remaining = std::max(0.0, span - seconds) / speed;
            m_eta = QStringLiteral("%1 left").arg(formatDuration(remaining));
        }
        publish();
    });
    connect(m_job, &FfmpegJob::finished, this, [this](int code) {
        if (m_cancel) {
            m_running = false;
            m_status = QStringLiteral("Export canceled");
            publish();
            return;
        }
        if (!m_discordPhase)
            finishQuality(code);
        else
            finishDiscordAttempt(code);
    });
}

void Exporter::publish()
{
    emit changed();
}

QString Exporter::uniqueFile(const QString &directory, const QString &base, const QString &extension)
{
    QString path = QDir(directory).filePath(base + QLatin1Char('.') + extension);
    int suffix = 2;
    while (QFileInfo::exists(path))
        path = QDir(directory).filePath(base + QString::number(suffix++) + QLatin1Char('.') + extension);
    return path;
}

QString exportDirectory(const QFileInfo &source)
{
    if (source.dir().dirName().compare(QLatin1String("Exports"), Qt::CaseInsensitive) == 0)
        return source.absolutePath();
    const QString directory = QDir(source.absolutePath()).filePath(QStringLiteral("Exports"));
    QDir().mkpath(directory);
    return directory;
}

void Exporter::start(const ExportRequest &request)
{
    if (m_running)
        return;
    if (request.source.isEmpty() || !QFileInfo::exists(request.source) || request.trimOut <= request.trimIn) {
        m_error = QStringLiteral("Choose a valid trim range before exporting.");
        publish();
        return;
    }
    m_request = request;
    m_cancel = false;
    m_discordPhase = false;
    m_discordTries = 0;
    m_bestDiscordBytes = 0;
    m_bestDiscordPath.clear();
    m_error.clear();
    m_eta.clear();
    m_outputSize.clear();
    m_discordPath.clear();
    m_discordSize.clear();
    m_progress = 0;
    m_encoder = m_devices->resolveEncoder(m_settings->encoder());
    const QFileInfo source(request.source);
    const QString extension = m_settings->exportFormat();
    m_outputPath = uniqueFile(exportDirectory(source), source.completeBaseName(), extension);
    if (QFileInfo(m_outputPath).absoluteFilePath() == source.absoluteFilePath()) {
        m_error = QStringLiteral("Refusing to overwrite the original clip.");
        publish();
        return;
    }
    m_running = true;
    m_status = QStringLiteral("Exporting edited copy");
    publish();
    beginQuality();
}

void Exporter::cancel()
{
    if (!m_running)
        return;
    m_cancel = true;
    m_status = QStringLiteral("Canceling");
    publish();
    m_job->kill();
}

QStringList Exporter::qualityArguments(const QString &output, int videoBitrateKbps, bool discord) const
{
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("info"),
                     QStringLiteral("-y"), QStringLiteral("-i"), m_request.source, QStringLiteral("-ss"),
                     QString::number(m_request.trimIn, 'f', 3), QStringLiteral("-to"),
                     QString::number(m_request.trimOut, 'f', 3)};
    const int height = discord ? 720 : m_settings->exportHeight();
    if (height > 0) {
        args << QStringLiteral("-vf")
             << QStringLiteral("scale=-2:%1:force_original_aspect_ratio=decrease:force_divisible_by=2").arg(height);
    }
    QStringList audioFilters;
    QStringList maps{QStringLiteral("-map"), QStringLiteral("0:v:0")};
    int outputIndex = 0;
    for (const QVariant &value : m_request.tracks) {
        const QVariantMap track = value.toMap();
        if (!track.value(QStringLiteral("enabled")).toBool())
            continue;
        const int index = track.value(QStringLiteral("index")).toInt();
        const double volume = std::clamp(track.value(QStringLiteral("volume")).toDouble(), 0.0, 2.0);
        const QString label = QStringLiteral("a%1").arg(outputIndex++);
        audioFilters << QStringLiteral("[0:a:%1]volume=%2[%3]").arg(index).arg(volume, 0, 'f', 3).arg(label);
        maps << QStringLiteral("-map") << QStringLiteral("[%1]").arg(label);
    }
    if (!audioFilters.isEmpty())
        args << QStringLiteral("-filter_complex") << audioFilters.join(QLatin1Char(';'));
    args << maps;
    const bool webm = !discord && m_settings->exportFormat() == QLatin1String("webm");
    if (webm) {
        args << QStringLiteral("-c:v") << QStringLiteral("libvpx-vp9") << QStringLiteral("-b:v")
             << (QString::number(std::max(120, videoBitrateKbps)) + QLatin1Char('k')) << QStringLiteral("-deadline")
             << QStringLiteral("good") << QStringLiteral("-cpu-used") << QStringLiteral("4") << QStringLiteral("-row-mt")
             << QStringLiteral("1");
        if (outputIndex > 0)
            args << QStringLiteral("-c:a") << QStringLiteral("libopus") << QStringLiteral("-b:a") << QStringLiteral("128k");
    } else {
        args << encoderArgs(discord ? QStringLiteral("x264") : m_encoder, videoBitrateKbps);
        if (outputIndex > 0)
            args << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a") << QStringLiteral("160k");
    }
    if (!webm)
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");
    args << output;
    return args;
}

void Exporter::beginQuality()
{
    m_discordPhase = false;
    m_job->start(ffmpegExecutable(), qualityArguments(m_outputPath, m_settings->exportVideoBitrateKbps(), false));
}

void Exporter::finishQuality(int code)
{
    if (code != 0 || !QFileInfo::exists(m_outputPath)) {
        m_running = false;
        m_error = QStringLiteral("Export failed. %1").arg(m_job->tail().right(400));
        m_status = QStringLiteral("Export failed");
        publish();
        return;
    }
    m_outputSize = formatBytes(QFileInfo(m_outputPath).size());
    const QString path = m_outputPath;
    const QString game = m_request.game;
    const double span = std::max(0.0, m_request.trimOut - m_request.trimIn);
    QTimer::singleShot(0, this, [this, path, game, span] {
        if (m_cancel)
            return;
        m_library->ingest(path, game, span);
        if (!m_settings->discordCompress()) {
            m_running = false;
            m_progress = 1;
            m_status = QStringLiteral("Saved %1").arg(QFileInfo(path).fileName());
            m_eta.clear();
            publish();
            return;
        }
        const QFileInfo saved(path);
        m_discordPath = uniqueFile(saved.absolutePath(), saved.completeBaseName() + QStringLiteral("_compressed"),
                                   QStringLiteral("mp4"));
        const double duration = std::max(0.1, span);
        const qint64 targetBytes = qint64(m_settings->discordTargetMb()) * 1024 * 1024;
        const double audioBps = 160000.0;
        double videoBps = (double(targetBytes) * 8.0) / duration - audioBps;
        videoBps = std::max(120000.0, videoBps * 0.90);
        m_discordBitrate = int(videoBps / 1000.0);
        m_discordTries = 0;
        beginDiscord();
    });
}

void Exporter::beginDiscord()
{
    m_discordPhase = true;
    m_status = m_discordTries == 0 ? QStringLiteral("Compressing a Discord copy")
                                   : QStringLiteral("Discord copy was too large. Retrying.");
    publish();
    const QString attempt = m_discordPath + QStringLiteral(".part.mp4");
    m_job->start(ffmpegExecutable(), qualityArguments(attempt, m_discordBitrate, true));
}

void Exporter::finishDiscordAttempt(int code)
{
    const QString attempt = m_discordPath + QStringLiteral(".part.mp4");
    const qint64 size = QFileInfo(attempt).size();
    const qint64 target = qint64(m_settings->discordTargetMb()) * 1024 * 1024;
    if (code != 0 || size <= 0) {
        QFile::remove(attempt);
        m_running = false;
        m_error = QStringLiteral("The edited copy was saved, but the Discord copy failed.");
        m_status = QStringLiteral("Edited copy saved");
        publish();
        return;
    }
    if (m_bestDiscordBytes == 0 || size < m_bestDiscordBytes) {
        if (!m_bestDiscordPath.isEmpty())
            QFile::remove(m_bestDiscordPath);
        const QString stored = m_discordPath + QStringLiteral(".best.mp4");
        QFile::remove(stored);
        QFile::rename(attempt, stored);
        m_bestDiscordPath = stored;
        m_bestDiscordBytes = size;
    } else {
        QFile::remove(attempt);
    }
    ++m_discordTries;
    if (size > target && m_discordTries < 3) {
        const double ratio = double(target) / double(size);
        m_discordBitrate = std::max(120, int(m_discordBitrate * ratio * 0.92));
        beginDiscord();
        return;
    }
    const QString savedDiscord = m_discordPath;
    const QString best = m_bestDiscordPath;
    const QString game = m_request.game;
    const double span = std::max(0.0, m_request.trimOut - m_request.trimIn);
    QTimer::singleShot(0, this, [this, savedDiscord, best, game, span, target] {
        if (m_cancel)
            return;
        QFile::remove(savedDiscord);
        QFile::rename(best, savedDiscord);
        m_discordSize = formatBytes(QFileInfo(savedDiscord).size());
        m_library->ingest(savedDiscord, game, span);
        m_running = false;
        m_progress = 1;
        m_eta.clear();
        if (QFileInfo(savedDiscord).size() > target)
            m_status = QStringLiteral("Edited copy saved. Smallest Discord copy is still %1.").arg(m_discordSize);
        else
            m_status = QStringLiteral("Saved the edit and a %1 Discord copy.").arg(m_discordSize);
        publish();
    });
}

} // namespace nuclip
