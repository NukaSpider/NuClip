#include "recording/RecordingEngine.h"

#include "platform/WinUtil.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "library/ClipRepository.h"
#include "platform/AppCatalog.h"
#include "platform/DeviceCatalog.h"
#include "platform/GameDetector.h"
#include "recording/CaptureCommand.h"
#include "recording/FfmpegJob.h"
#include "recording/FfmpegLocator.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

#include <algorithm>

namespace nuclip {

RecordingEngine::RecordingEngine(Settings *settings, DeviceCatalog *devices, GameDetector *detector,
                                 ClipRepository *library, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_devices(devices)
    , m_detector(detector)
    , m_library(library)
    , m_system(std::make_unique<WasapiInput>())
    , m_mic(std::make_unique<WasapiInput>())
{
    m_bufferDir = Paths::bufferDir();
    m_capture = new FfmpegJob(this);
    m_concat = new FfmpegJob(this);
    m_shot = new FfmpegJob(this);
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(500);
    connect(m_tickTimer, &QTimer::timeout, this, &RecordingEngine::tick);
    connect(m_capture, &FfmpegJob::finished, this, &RecordingEngine::onCaptureFinished);
    connect(m_capture, &FfmpegJob::tailChanged, this, &RecordingEngine::detailsChanged);
    connect(m_concat, &FfmpegJob::finished, this, &RecordingEngine::onConcatFinished);
    connect(m_shot, &FfmpegJob::finished, this, [this](int code) {
        if (code == 0 && QFileInfo::exists(m_shotPath))
            emit screenshotSaved(m_shotPath);
        else
            setError(QStringLiteral("Screenshot failed. %1").arg(m_shot->tail().right(400)));
    });
    connect(m_settings, &Settings::systemVolumeChanged, this, [this] {
        m_system->setVolume(float(m_settings->systemVolume()));
    });
    connect(m_settings, &Settings::micVolumeChanged, this, [this] {
        m_mic->setVolume(float(m_settings->micVolume()));
    });
    connect(m_settings, &Settings::replaySecondsChanged, this, &RecordingEngine::bufferChanged);
    connect(m_settings, &Settings::hotkeysChanged, this, &RecordingEngine::bufferChanged);
}

RecordingEngine::~RecordingEngine()
{
    m_token++;
    if (m_capture && m_capture->running())
        m_capture->kill();
    stopAudio();
}

QString RecordingEngine::stateText() const
{
    switch (m_state) {
    case Starting:
        return QStringLiteral("Starting");
    case Recording:
        return m_saving ? QStringLiteral("Saving clip") : QStringLiteral("Recording");
    case Paused:
        return QStringLiteral("Paused");
    case Stopping:
        return QStringLiteral("Stopping");
    case Error:
        return QStringLiteral("Error");
    case Idle:
    default:
        return QStringLiteral("Idle");
    }
}

QString RecordingEngine::mode() const
{
    if (m_mode == Mode::Session)
        return QStringLiteral("session");
    if (m_mode == Mode::Replay)
        return QStringLiteral("replay");
    return QStringLiteral("idle");
}

QString RecordingEngine::details() const
{
    return m_capture ? m_capture->tail() : QString();
}

int RecordingEngine::bufferSeconds() const
{
    int seconds = std::clamp(m_settings->replaySeconds(), 5, 600);
    const auto consider = [&](const QString &sequence, int value) {
        if (!sequence.trimmed().isEmpty())
            seconds = std::max(seconds, value);
    };
    consider(m_settings->hotkeyClip15(), 15);
    consider(m_settings->hotkeyClip30(), 30);
    consider(m_settings->hotkeyClip60(), 60);
    consider(m_settings->hotkeyClip120(), 120);
    return seconds;
}

void RecordingEngine::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    if (state != Starting)
        m_booting = false;
    if (state == Recording) {
        m_busyRetried = false;
        m_tickTimer->start();
    }
    else if (state != Paused)
        m_tickTimer->stop();
    emit stateChanged();
}

void RecordingEngine::setError(const QString &error)
{
    m_error = error.trimmed();
    emit errorChanged();
    if (!m_error.isEmpty())
        Log::error(m_error);
}

void RecordingEngine::setWarning(const QString &warning)
{
    m_warning = warning.trimmed();
    emit warningChanged();
    if (!m_warning.isEmpty())
        Log::warn(m_warning);
}

void RecordingEngine::startReplay()
{
    beginStart(Mode::Replay, false);
}

void RecordingEngine::startSession()
{
    beginStart(Mode::Session, false);
}

void RecordingEngine::beginStart(Mode mode, bool resume)
{
    if (m_state == Recording || m_state == Starting || m_state == Stopping)
        return;
    WinUtil::closeProcessesFrom(ffmpegExecutable());
    if (!resume && m_state == Paused)
        return;
    const QString ffmpeg = ffmpegExecutable();
    if (ffmpeg.isEmpty()) {
        setError(QStringLiteral("FFmpeg was not found next to NuClip."));
        setState(Error);
        return;
    }
    m_mode = mode;
    m_intent = Intent::None;
    m_keepSegments = resume;
    if (!resume) {
        m_retried = false;
        m_elapsed = 0;
        m_elapsedAccum = 0;
        emit elapsedChanged();
    }
    setError({});
    if (!resume)
        setWarning({});
    if (!resume)
        m_codecOverride.clear();
    setState(Starting);
    const int token = ++m_token;
    const auto resolve = [this] {
        const QString codec = m_codecOverride.isEmpty() ? m_settings->codec() : m_codecOverride;
        return m_devices->resolveEncoder(m_settings->encoder(), codec);
    };
    if (m_devices->probing()) {
        connect(m_devices, &DeviceCatalog::probeFinished, this, [this, token, resolve] {
            if (token != m_token)
                return;
            launch(resolve());
        }, Qt::SingleShotConnection);
        return;
    }
    Q_UNUSED(resume);
    launch(resolve());
}

void RecordingEngine::launch(const QString &encoderId)
{
    if (m_state != Starting)
        return;
    const QString requested = m_settings->encoder();
    if (requested != QLatin1String("auto") && requested != encoderId)
        setWarning(QStringLiteral("%1 is not available. Using %2.")
                       .arg(m_devices->encoderLabel(requested), m_devices->encoderLabel(encoderId)));
    m_activeEncoder = encoderId;
    if (!m_keepSegments)
        removeSegments();
    m_keepSegments = false;

    const int monitor = chooseMonitor();
    m_activeMonitor = m_devices->monitorLabel(monitor);
    ++m_pipeGeneration;
    const QString pipeBase = QStringLiteral("\\\\.\\pipe\\nuclip_%1_%2")
                                 .arg(QCoreApplication::applicationPid())
                                 .arg(m_pipeGeneration);
    QStringList pipes;
    QString audioError;
    if (m_settings->systemAudio()) {
        WasapiInput::Config config;
        config.loopback = true;
        config.deviceId = m_settings->outputDeviceId();
        config.pipeName = pipeBase + QStringLiteral("_sys");
        if (m_system->start(config, &audioError)) {
            m_system->setVolume(float(m_settings->systemVolume()));
            pipes << m_system->pipeName();
        } else if (!audioError.isEmpty()) {
            setWarning(audioError);
        }
    }
    if (m_settings->micEnabled()) {
        WasapiInput::Config config;
        config.loopback = false;
        config.deviceId = m_settings->micDeviceId();
        config.pipeName = pipeBase + QStringLiteral("_mic");
        QString micError;
        if (m_mic->start(config, &micError)) {
            m_mic->setVolume(float(m_settings->micVolume()));
            pipes << m_mic->pipeName();
        } else if (!micError.isEmpty()) {
            setWarning(m_warning.isEmpty() ? micError : m_warning + QLatin1Char(' ') + micError);
        }
    }

    int captureWidth = 0;
    int captureHeight = 0;
    int refresh = 0;
    m_devices->monitorGeometry(monitor, &captureWidth, &captureHeight, &refresh);
    const QString codec = m_codecOverride.isEmpty() ? m_settings->codec() : m_codecOverride;
    const int askedFps = m_settings->fps();
    const int usedFps = m_devices->supportedFps(askedFps, monitor);
    if (usedFps != askedFps) {
        const QString note = refresh > 0
            ? QStringLiteral("This display refreshes at %1 Hz, so recording uses %2 fps.").arg(refresh).arg(usedFps)
            : QStringLiteral("This display did not report a higher refresh rate, so recording uses %1 fps.").arg(usedFps);
        setWarning(m_warning.isEmpty() ? note : m_warning + QLatin1Char(' ') + note);
    }

    CaptureRequest request;
    request.monitor = monitor;
    request.fps = usedFps;
    request.cursor = m_settings->showCursor();
    request.encoder = encoderId;
    request.codec = codec;
    if (m_settings->quality() == QLatin1String("custom")) {
        request.width = m_settings->width();
        request.height = m_settings->height();
        request.videoBitrateKbps = m_settings->videoBitrateKbps();
        request.audioBitrateKbps = m_settings->audioBitrateKbps();
    } else {
        request.videoBitrateKbps = m_settings->recommendedVideoBitrate(captureWidth, captureHeight, usedFps, codec,
                                                                       m_settings->quality());
        request.audioBitrateKbps = m_settings->audioBitrateFor(m_settings->quality());
    }
    request.pipes = pipes;
    request.segmentStart = nextSegmentIndex();
    request.pattern = QDir::fromNativeSeparators(QDir(m_bufferDir).filePath(QStringLiteral("seg_%06d.ts")));
    m_capture->start(ffmpegExecutable(), captureArguments(request));
    m_segmentWatch = -1;
    m_segmentWatchAt = QDateTime::currentMSecsSinceEpoch();
    m_clockStart = QDateTime::currentMSecsSinceEpoch();
    if (m_state == Starting && m_capture->running())
        setState(Recording);
}

int RecordingEngine::chooseMonitor() const
{
    int index = std::max(0, m_settings->monitorIndex());
    if (m_settings->captureMode() == QLatin1String("auto") && m_detector && m_detector->live())
        index = m_detector->monitorIndex();
    if (index >= m_devices->monitors().size())
        index = 0;
    return index;
}

void RecordingEngine::onCaptureFinished(int code)
{
    const QString tail = m_capture->tail();
    stopAudio();
    if (m_intent == Intent::Pause) {
        m_elapsedAccum = m_elapsed;
        m_intent = Intent::None;
        setState(Paused);
        return;
    }
    if (m_intent == Intent::Recycle) {
        m_intent = Intent::None;
        removeSegments();
        m_elapsedAccum = m_elapsed;
        m_retried = true;
        setState(Idle);
        beginStart(m_mode == Mode::None ? Mode::Replay : m_mode, true);
        return;
    }
    if (m_intent == Intent::Stop) {
        m_intent = Intent::None;
        if (m_mode == Mode::Session) {
            const QStringList files = listSegments();
            if (!files.isEmpty()) {
                m_sessionSave = true;
                startConcat(files, makeClipPath(gameName()), gameName(), files.size());
                return;
            }
        }
        removeSegments();
        m_elapsed = 0;
        m_elapsedAccum = 0;
        m_mode = Mode::None;
        setState(Idle);
        return;
    }

    const bool captureProblem = tail.contains(QStringLiteral("ddagrab"), Qt::CaseInsensitive)
        || tail.contains(QStringLiteral("DXGI"), Qt::CaseInsensitive);
    const bool displayBusy = tail.contains(QStringLiteral("too many open duplication"), Qt::CaseInsensitive)
        || tail.contains(QStringLiteral("device or resource busy"), Qt::CaseInsensitive)
        || tail.contains(QStringLiteral("Failed to configure output pad"), Qt::CaseInsensitive);
    if (displayBusy && !m_busyRetried) {
        m_busyRetried = true;
        Log::warn(QStringLiteral("Display capture was busy. Releasing leftover capture processes and trying again."));
        setState(Starting);
        QTimer::singleShot(700, this, [this] {
            if (m_state != Starting)
                return;
            setState(Idle);
            startReplay();
        });
        return;
    }
    if (!captureProblem && !m_retried && m_activeEncoder != QLatin1String("x264") && code != 0 && m_elapsed < 2.0) {
        m_retried = true;
        m_codecOverride = QStringLiteral("h264");
        setWarning(QStringLiteral("The hardware encoder failed. NuClip switched to software H.264."));
        m_elapsedAccum = 0;
        setState(Starting);
        launch(QStringLiteral("x264"));
        return;
    }
    if (!tail.isEmpty())
        Log::error(tail.right(500));
    setState(Error);
    if (displayBusy)
        setError(QStringLiteral("The display is already being captured. Close any other capture app, then press Retry."));
    else if (captureProblem)
        setError(QStringLiteral("This display could not be captured. A fullscreen app may be holding it."));
    else
        setError(tail.isEmpty() ? QStringLiteral("Recording stopped unexpectedly.") : QStringLiteral("Recording stopped before it could start."));
    Q_UNUSED(code);
}

void RecordingEngine::stop()
{
    if (m_state == Idle || m_state == Stopping)
        return;
    if (m_state == Starting) {
        ++m_token;
        if (m_capture->running()) {
            m_intent = Intent::Stop;
            requestStopProcess();
            setState(Stopping);
        } else {
            stopAudio();
            setState(Idle);
        }
        return;
    }
    if (m_state == Paused) {
        if (m_mode == Mode::Session) {
            const QStringList files = listSegments();
            if (!files.isEmpty()) {
                setState(Stopping);
                m_sessionSave = true;
                startConcat(files, makeClipPath(gameName()), gameName(), files.size());
                return;
            }
        }
        removeSegments();
        m_elapsed = 0;
        m_elapsedAccum = 0;
        m_mode = Mode::None;
        setState(Idle);
        return;
    }
    if (m_state != Recording && m_state != Error)
        return;
    m_intent = Intent::Stop;
    setState(Stopping);
    if (m_capture->running())
        requestStopProcess();
    else
        onCaptureFinished(0);
}

void RecordingEngine::toggle()
{
    if (m_state == Recording || m_state == Paused || m_state == Starting)
        stop();
    else
        startReplay();
}

void RecordingEngine::pause()
{
    if (m_state != Recording)
        return;
    m_intent = Intent::Pause;
    setState(Stopping);
    requestStopProcess();
}

void RecordingEngine::resume()
{
    if (m_state != Paused)
        return;
    m_elapsedAccum = m_elapsed;
    m_retried = true;
    beginStart(m_mode == Mode::None ? Mode::Replay : m_mode, true);
}

void RecordingEngine::togglePause()
{
    if (m_state == Recording)
        pause();
    else if (m_state == Paused)
        resume();
}

void RecordingEngine::requestStopProcess()
{
    const int token = m_token;
    m_capture->requestQuit();
    QTimer::singleShot(5000, this, [this, token] {
        if (token == m_token && m_capture->running())
            m_capture->kill();
    });
}

void RecordingEngine::stopAudio()
{
    m_system->stop();
    m_mic->stop();
    m_systemLevel = 0;
    m_micLevel = 0;
    emit levelsChanged();
}

void RecordingEngine::saveClip(int seconds)
{
    saveClipInternal(seconds, true);
}

void RecordingEngine::saveClipInternal(int seconds, bool acknowledge)
{
    if (m_state != Recording && m_state != Paused) {
        setError(QStringLiteral("The replay buffer is not running."));
        return;
    }
    if (seconds <= 0)
        seconds = m_settings->replaySeconds();
    if (m_saving) {
        m_pendingSeconds = seconds;
        if (acknowledge) {
            emit captureAccepted();
            emit clipAccepted(gameName(), seconds);
        }
        return;
    }
    QStringList files = listSegments();
    if (m_state == Recording && !files.isEmpty())
        files.removeLast();
    if (files.isEmpty()) {
        setError(QStringLiteral("Not enough video has been recorded yet."));
        return;
    }
    if (files.size() > seconds)
        files = files.mid(files.size() - seconds);
    if (m_mode == Mode::Replay && !files.isEmpty()) {
        const qint64 ageMs = QFileInfo(files.last()).lastModified().msecsTo(QDateTime::currentDateTime());
        if (ageMs > 4000) {
            setError(QStringLiteral("The replay buffer stalled. It is restarting. Clip again in a moment."));
            recycleCapture();
            return;
        }
    }
    if (acknowledge) {
        emit captureAccepted();
        emit clipAccepted(gameName(), files.size());
    }
    startConcat(files, makeClipPath(gameName()), gameName(), files.size());
}

void RecordingEngine::screenshot()
{
    if (m_shot->running())
        return;
    QDir().mkpath(m_settings->screenshotsDirectory());
    m_shotPath = QDir(m_settings->screenshotsDirectory())
                     .filePath(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))
                               + QStringLiteral(".png"));
    if (m_state == Recording || m_state == Paused) {
        QStringList files = listSegments();
        if (m_state == Recording && !files.isEmpty())
            files.removeLast();
        if (files.isEmpty()) {
            setError(QStringLiteral("Wait a moment for the first second of video, then try the screenshot again."));
            return;
        }
        emit captureAccepted();
        emit screenshotAccepted();
        m_shot->start(ffmpegExecutable(), frameArguments(files.last(), m_shotPath));
        return;
    }
    emit captureAccepted();
    emit screenshotAccepted();
    m_shot->start(ffmpegExecutable(), screenshotArguments(chooseMonitor(), m_shotPath));
}

QString RecordingEngine::gameName() const
{
    if (!m_detector)
        return QStringLiteral("Desktop");
    if (m_detector->gameOpen() || m_detector->live()) {
        const QString name = m_detector->name();
        if (!name.isEmpty())
            return name;
    }
    return QStringLiteral("Desktop");
}

QString RecordingEngine::makeClipPath(const QString &game) const
{
    QString directory = m_settings->clipsDirectory();
    if (m_settings->organizeByGame()) {
        directory = QDir(directory).filePath(Paths::sanitizeName(game));
        QDir().mkpath(directory);
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString base = AppCatalog::fileToken(game) + QLatin1Char('_') + stamp;
    QString path = QDir(directory).filePath(base + QStringLiteral(".mp4"));
    int suffix = 2;
    while (QFileInfo::exists(path))
        path = QDir(directory).filePath(base + QLatin1Char('_') + QString::number(suffix++) + QStringLiteral(".mp4"));
    return path;
}

void RecordingEngine::startConcat(const QStringList &files, const QString &output, const QString &game,
                                  double durationHint)
{
    QFile list(QDir(m_bufferDir).filePath(QStringLiteral("concat.txt")));
    if (!list.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(QStringLiteral("Could not prepare the clip."));
        return;
    }
    for (const QString &file : files) {
        const QString path = QDir::fromNativeSeparators(QFileInfo(file).absoluteFilePath()).replace('\'', "'\\''");
        list.write("file '");
        list.write(path.toUtf8());
        list.write("'\n");
    }
    list.close();
    m_concatOutput = output;
    m_concatGame = game;
    m_concatDuration = durationHint;
    m_saving = true;
    emit savingChanged();
    emit stateChanged();
    QDir().mkpath(QFileInfo(output).absolutePath());
    m_concat->start(ffmpegExecutable(), concatArguments(list.fileName(), output));
}

void RecordingEngine::onConcatFinished(int code)
{
    const bool sessionSave = m_sessionSave;
    m_saving = false;
    m_sessionSave = false;
    emit savingChanged();
    if (code == 0 && QFileInfo(m_concatOutput).size() > 1000) {
        m_library->ingest(m_concatOutput, m_concatGame, m_concatDuration, m_detector ? m_detector->source() : QVariantMap{});
        emit clipSaved(m_concatOutput, m_concatGame, m_concatDuration);
        setError({});
    } else {
        setError(QStringLiteral("Could not save the clip. %1").arg(m_concat->tail().right(400)));
    }
    if (sessionSave) {
        removeSegments();
        m_elapsed = 0;
        m_elapsedAccum = 0;
        m_mode = Mode::None;
        setState(Idle);
    } else {
        emit stateChanged();
    }
    if (m_pendingSeconds > 0 && (m_state == Recording || m_state == Paused)) {
        const int seconds = m_pendingSeconds;
        m_pendingSeconds = 0;
        saveClipInternal(seconds, false);
    }
}

void RecordingEngine::maybeAutoStart()
{
    if (!m_settings->autoStartReplay()) {
        m_booting = false;
        emit stateChanged();
        return;
    }
    if (m_state != Idle && m_state != Error)
        return;
    startReplay();
}

void RecordingEngine::startup()
{
    WinUtil::closeProcessesFrom(ffmpegExecutable());
    removeSegments();
    maybeAutoStart();
}

QStringList RecordingEngine::listSegments() const
{
    QStringList files;
    const QFileInfoList infos = QDir(m_bufferDir).entryInfoList({QStringLiteral("seg_*.ts")}, QDir::Files, QDir::Name);
    for (const QFileInfo &info : infos) {
        if (info.size() > 0)
            files.append(info.absoluteFilePath());
    }
    return files;
}

int RecordingEngine::nextSegmentIndex() const
{
    int next = 0;
    for (const QString &file : listSegments()) {
        const QString base = QFileInfo(file).completeBaseName();
        next = std::max(next, base.mid(4).toInt() + 1);
    }
    return next;
}

void RecordingEngine::removeSegments()
{
    for (const QString &file : listSegments())
        QFile::remove(file);
}

void RecordingEngine::watchSegments()
{
    if (m_mode != Mode::Replay || m_state != Recording || m_intent != Intent::None || m_saving)
        return;
    const int index = nextSegmentIndex();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (index != m_segmentWatch) {
        m_segmentWatch = index;
        m_segmentWatchAt = now;
        m_recycleStreak = 0;
        return;
    }
    if (m_segmentWatchAt == 0)
        m_segmentWatchAt = now;
    if (now - m_segmentWatchAt < 3000 || now < m_recycleAfter)
        return;
    recycleCapture();
}

void RecordingEngine::recycleCapture()
{
    if (m_state != Recording || m_intent != Intent::None || !m_capture->running())
        return;
    Log::warn(QStringLiteral("Replay buffer stopped rolling. Restarting capture."));
    ++m_recycleStreak;
    m_recycleAfter = QDateTime::currentMSecsSinceEpoch() + std::min(30000, 3000 * m_recycleStreak);
    m_intent = Intent::Recycle;
    setState(Stopping);
    requestStopProcess();
}

void RecordingEngine::cleanupSegments()
{
    if (m_mode != Mode::Replay || m_state != Recording)
        return;
    QStringList files = listSegments();
    const int keep = bufferSeconds() + 2;
    if (files.size() <= keep + 1)
        return;
    const int removable = files.size() - keep - 1;
    for (int i = 0; i < removable; ++i)
        QFile::remove(files.at(i));
}

void RecordingEngine::tick()
{
    if (m_state == Recording) {
        m_elapsed = m_elapsedAccum + (QDateTime::currentMSecsSinceEpoch() - m_clockStart) / 1000.0;
        emit elapsedChanged();
        if (++m_cleanupTicks >= 4) {
            m_cleanupTicks = 0;
            cleanupSegments();
        }
        watchSegments();
    }
    const qreal system = m_system->failed() ? 0 : m_system->level();
    const qreal mic = m_mic->failed() ? 0 : m_mic->level();
    if (!qFuzzyCompare(system + 1, m_systemLevel + 1) || !qFuzzyCompare(mic + 1, m_micLevel + 1)) {
        m_systemLevel = system;
        m_micLevel = mic;
        emit levelsChanged();
    }
    if (m_system->failed() && m_warning.isEmpty())
        setWarning(m_system->error());
}

void RecordingEngine::shutdown()
{
    if (m_capture->running()) {
        m_intent = Intent::Stop;
        m_capture->requestQuit();
        m_capture->wait(2500);
        if (m_capture->running())
            m_capture->kill();
    }
    stopAudio();
    if (m_mode == Mode::Session) {
        const QStringList files = listSegments();
        if (!files.isEmpty()) {
            const QString output = makeClipPath(gameName());
            QFile list(QDir(m_bufferDir).filePath(QStringLiteral("shutdown.txt")));
            if (list.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                for (const QString &file : files) {
                    list.write("file '");
                    list.write(QDir::fromNativeSeparators(file).toUtf8());
                    list.write("'\n");
                }
                list.close();
                m_concat->start(ffmpegExecutable(), concatArguments(list.fileName(), output));
                m_concat->wait(15000);
                if (QFileInfo(output).size() > 1000)
                    m_library->ingest(output, gameName(), files.size(), m_detector ? m_detector->source() : QVariantMap{});
            }
        }
    }
    removeSegments();
    m_state = Idle;
}

} // namespace nuclip
