#include "platform/DiscordPresence.h"

#include "core/Log.h"
#include "core/Settings.h"
#include "editor/EditorSession.h"
#include "export/Exporter.h"
#include "platform/AppCatalog.h"
#include "platform/GameDetector.h"
#include "recording/RecordingEngine.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <cstring>

#include <QtGlobal>

namespace nuclip {

namespace {

constexpr quint32 kHandshake = 0;
constexpr quint32 kFrame = 1;
constexpr quint32 kClose = 2;
constexpr quint32 kPing = 3;
constexpr quint32 kPong = 4;

struct GameAsset {
    const char *stableId;
    const char *key;
};

// Discord fetches this. An unknown asset key is the question-mark placeholder.
QString presenceLogo()
{
    return QStringLiteral("https://raw.githubusercontent.com/NukaSpider/nuclip-presence/main/nuclip.png");
}

// Add a row only after that key is uploaded on the Discord application.
// Local game icons are never sent.
constexpr GameAsset kGameAssets[] = {
    {nullptr, nullptr},
};

QString registeredGameAsset(const QString &stableId)
{
    for (const GameAsset &asset : kGameAssets) {
        if (!asset.stableId)
            break;
        if (stableId == QLatin1String(asset.stableId))
            return QString::fromLatin1(asset.key);
    }
    return {};
}

QString safeGameName(QString name)
{
    name = AppCatalog::displayForStored(name).simplified();
    if (name.isEmpty() || name.compare(QStringLiteral("Desktop"), Qt::CaseInsensitive) == 0)
        return {};
    if (name.contains(QLatin1Char('\\')) || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char(':')))
        return {};
    if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
        return {};
    if (name.size() > 48)
        name.truncate(48);
    return name;
}

struct CopyInput {
    bool showGame = true;
    bool showRecording = true;
    bool showBuffer = true;
    bool showEditing = true;
    bool exporting = false;
    bool editing = false;
    QString mode;
    int recordState = 0;
    QString game;
    QString stableId;
    int bufferSeconds = 0;
};

struct Copy {
    QString details;
    QString state;
    QString largeImage;
    QString smallImage;
    QString kind;
    bool timed = false;
};

Copy composeCopy(const CopyInput &in)
{
    Copy copy;
    copy.largeImage = presenceLogo();
    const QString game = in.showGame ? safeGameName(in.game) : QString();
    if (in.showGame && !game.isEmpty()) {
        const QString key = registeredGameAsset(in.stableId);
        if (!key.isEmpty())
            copy.largeImage = key;
    }

    if (in.showEditing && in.exporting) {
        copy.details = QStringLiteral("Exporting a clip");
        copy.kind = QStringLiteral("export");
        copy.timed = true;
        return copy;
    }
    if (in.showEditing && in.editing) {
        copy.details = QStringLiteral("Editing a clip");
        copy.kind = QStringLiteral("edit");
        copy.timed = true;
        return copy;
    }

    const bool live = in.recordState == RecordingEngine::Recording || in.recordState == RecordingEngine::Starting
        || in.recordState == RecordingEngine::Paused;
    if (in.showRecording && live && (in.mode == QLatin1String("session") || in.mode == QLatin1String("replay"))) {
        const bool session = in.mode == QLatin1String("session");
        if (in.recordState == RecordingEngine::Paused)
            copy.details = session ? QStringLiteral("Recording paused") : QStringLiteral("Capture paused");
        else if (in.recordState == RecordingEngine::Starting)
            copy.details = QStringLiteral("Using NuClip");
        else if (!game.isEmpty())
            copy.details = session ? QStringLiteral("Recording %1").arg(game) : QStringLiteral("Capturing %1").arg(game);
        else
            copy.details = QStringLiteral("Using NuClip");

        if (session) {
            copy.state = QStringLiteral("Continuous Recording");
        } else if (in.showBuffer && in.bufferSeconds > 0) {
            copy.state = QStringLiteral("Replay Buffer · %1 seconds").arg(in.bufferSeconds);
        } else {
            copy.state = QStringLiteral("Replay Buffer Active");
        }
        if (in.recordState == RecordingEngine::Starting)
            copy.state = QStringLiteral("Starting capture");
        copy.kind = session ? QStringLiteral("session") : QStringLiteral("replay");
        copy.timed = in.recordState != RecordingEngine::Starting;
        return copy;
    }

    copy.details = QStringLiteral("Ready to capture.");
    copy.kind = QStringLiteral("idle");
    return copy;
}

} // namespace

DiscordPresence::DiscordPresence(Settings *settings, GameDetector *detector, RecordingEngine *recording,
                                 EditorSession *editor, Exporter *exporter, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_detector(detector)
    , m_recording(recording)
    , m_editor(editor)
    , m_exporter(exporter)
{
    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::connected, this, &DiscordPresence::sendHandshake);
    connect(m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::readFrames);
    connect(m_socket, &QLocalSocket::disconnected, this, [this] {
        m_ready = false;
        m_buffer.clear();
        m_lastPayload.clear();
        if (m_closing || m_stopped || m_rejected)
            return;
        if (wantsLink())
            failLink(QStringLiteral("Reconnecting"), true);
        else
            setStatus(m_settings->discordEnabled() ? QStringLiteral("Not connected") : QStringLiteral("Off"));
    });
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        if (m_closing || m_ready || m_stopped || m_rejected)
            return;
        if (error == QLocalSocket::ServerNotFoundError || error == QLocalSocket::ConnectionRefusedError) {
            tryPipe(m_pipe + 1);
            return;
        }
        failLink(QStringLiteral("Reconnecting"), true);
    });

    m_apply = new QTimer(this);
    m_apply->setSingleShot(true);
    m_apply->setInterval(300);
    connect(m_apply, &QTimer::timeout, this, &DiscordPresence::apply);

    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_retry->setInterval(6000);
    connect(m_retry, &QTimer::timeout, this, [this] {
        if (!m_stopped && wantsLink() && !m_ready)
            beginConnect();
    });

    m_probe = new QTimer(this);
    m_probe->setSingleShot(true);
    m_probe->setInterval(1200);
    connect(m_probe, &QTimer::timeout, this, [this] {
        if (m_socket->state() == QLocalSocket::ConnectingState)
            tryPipe(m_pipe + 1);
    });

    const auto kick = [this] { scheduleApply(); };
    connect(m_settings, &Settings::discordChanged, this, [this] {
        const bool enabled = m_settings->discordEnabled();
        const bool automatic = m_settings->discordAutoConnect();
        if (enabled && automatic && (!m_wasEnabled || !m_wasAutomatic))
            m_holdOff = false;
        m_wasEnabled = enabled;
        m_wasAutomatic = automatic;
        scheduleApply();
    });
    connect(m_detector, &GameDetector::changed, this, kick);
    connect(m_recording, &RecordingEngine::stateChanged, this, kick);
    connect(m_recording, &RecordingEngine::bufferChanged, this, kick);
    connect(m_editor, &EditorSession::sourceChanged, this, kick);
    connect(m_exporter, &Exporter::changed, this, kick);
    scheduleApply();
}

QVariantList DiscordPresence::assets() const
{
    QVariantList list;
    const auto add = [&](const QString &key, const QString &role, const QString &label) {
        QVariantMap item;
        item.insert(QStringLiteral("key"), key);
        item.insert(QStringLiteral("role"), role);
        item.insert(QStringLiteral("label"), label);
        list.append(item);
    };
    for (const GameAsset &asset : kGameAssets) {
        if (!asset.stableId)
            break;
        add(QString::fromLatin1(asset.key), QStringLiteral("Game"), QString::fromLatin1(asset.stableId));
    }
    return list;
}

void DiscordPresence::startup()
{
    m_stopped = false;
    m_wasEnabled = m_settings->discordEnabled();
    m_wasAutomatic = m_settings->discordAutoConnect();
    m_holdOff = !(m_wasEnabled && m_wasAutomatic);
    scheduleApply();
}

void DiscordPresence::shutdown()
{
    m_stopped = true;
    m_retry->stop();
    m_probe->stop();
    m_apply->stop();
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        sendActivity({}, true);
        m_socket->waitForBytesWritten(200);
        m_socket->disconnectFromServer();
    }
    m_closing = true;
    m_socket->abort();
    m_closing = false;
    m_ready = false;
    m_lastPayload.clear();
}

void DiscordPresence::connectNow()
{
    m_holdOff = false;
    m_rejected = false;
    m_stopped = false;
    if (!m_settings->discordEnabled())
        m_settings->setDiscordEnabled(true);
    beginConnect();
}

void DiscordPresence::disconnectNow()
{
    m_holdOff = true;
    m_retry->stop();
    if (m_socket->state() == QLocalSocket::ConnectedState)
        sendActivity({}, true);
    m_closing = true;
    m_socket->abort();
    m_closing = false;
    m_ready = false;
    m_lastPayload.clear();
    setStatus(QStringLiteral("Not connected"));
    emit statusChanged();
}

void DiscordPresence::setSurface(int page)
{
    if (m_surface == page)
        return;
    m_surface = page;
    scheduleApply();
}

DiscordPresence::Activity DiscordPresence::compose() const
{
    CopyInput in;
    in.showGame = m_settings->discordShowGame();
    in.showRecording = m_settings->discordShowRecording();
    in.showBuffer = m_settings->discordShowBuffer();
    in.showEditing = m_settings->discordShowEditing();
    in.exporting = m_exporter->running();
    in.editing = m_surface == 2 && m_editor->ready();
    in.mode = m_recording->mode();
    in.recordState = m_recording->state();
    in.bufferSeconds = std::max(0, m_recording->bufferSeconds());
    if (m_detector->gameOpen()) {
        in.game = m_detector->gameName();
        in.stableId = m_detector->gameStableId();
    } else if (m_detector->live()) {
        in.game = m_detector->name();
        in.stableId = m_detector->stableId();
    }
    const Copy copy = composeCopy(in);
    Activity activity;
    activity.details = copy.details;
    activity.state = copy.state;
    activity.largeImage = copy.largeImage;
    activity.smallImage = copy.smallImage;
    activity.kind = copy.kind;
    activity.timed = copy.timed;
    return activity;
}

QString DiscordPresence::applicationId() const
{
    return QStringLiteral("1574482365814935573");
}

bool DiscordPresence::wantsLink() const
{
    if (m_stopped || m_holdOff || !m_settings->discordEnabled())
        return false;
    return applicationId().size() >= 17;
}

void DiscordPresence::scheduleApply()
{
    if (!m_stopped)
        m_apply->start();
}

void DiscordPresence::apply()
{
    const Activity activity = compose();
    if (m_kind != activity.kind) {
        m_kind = activity.kind;
        m_startedAt = activity.timed ? QDateTime::currentSecsSinceEpoch() : 0;
    }
    const bool changed = m_details != activity.details || m_state != activity.state
        || m_largeAsset != activity.largeImage || m_smallAsset != activity.smallImage;
    m_details = activity.details;
    m_state = activity.state;
    m_largeAsset = activity.largeImage;
    m_smallAsset = activity.smallImage;
    if (changed)
        emit previewChanged();

    if (!m_settings->discordEnabled()) {
        m_rejected = false;
        if (m_ready || m_socket->state() != QLocalSocket::UnconnectedState) {
            if (m_socket->state() == QLocalSocket::ConnectedState)
                sendActivity({}, true);
            m_closing = true;
            m_socket->abort();
            m_closing = false;
            m_ready = false;
            m_lastPayload.clear();
        }
        m_retry->stop();
        setStatus(QStringLiteral("Off"));
        return;
    }
    if (!wantsLink()) {
        setStatus(QStringLiteral("Not connected"));
        return;
    }
    if (!m_ready) {
        if (!m_rejected && m_socket->state() == QLocalSocket::UnconnectedState && !m_retry->isActive())
            beginConnect();
        return;
    }
    sendActivity(activity, false);
}

void DiscordPresence::beginConnect()
{
    if (!wantsLink())
        return;
    m_retry->stop();
    tryPipe(0);
}

void DiscordPresence::tryPipe(int index)
{
    m_probe->stop();
    if (!wantsLink())
        return;
    if (index > 9) {
        m_closing = true;
        m_socket->abort();
        m_closing = false;
        failLink(QStringLiteral("Discord is not running"), true);
        return;
    }
    m_pipe = index;
    m_ready = false;
    m_buffer.clear();
    setStatus(QStringLiteral("Looking for Discord"));
    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_closing = true;
        m_socket->abort();
        m_closing = false;
    }
    m_socket->connectToServer(QStringLiteral("discord-ipc-%1").arg(index));
    m_probe->start();
}

void DiscordPresence::sendHandshake()
{
    m_probe->stop();
    QJsonObject hello;
    hello.insert(QStringLiteral("v"), 1);
    hello.insert(QStringLiteral("client_id"), applicationId());
    sendFrame(kHandshake, QJsonDocument(hello).toJson(QJsonDocument::Compact));
    setStatus(QStringLiteral("Connecting"));
}

void DiscordPresence::sendActivity(const Activity &activity, bool clear)
{
    if (m_socket->state() != QLocalSocket::ConnectedState)
        return;
    QJsonObject args;
    args.insert(QStringLiteral("pid"), static_cast<int>(QCoreApplication::applicationPid()));
    if (!clear) {
        QJsonObject body;
        body.insert(QStringLiteral("name"), QStringLiteral("NuClip"));
        body.insert(QStringLiteral("type"), 0);
        body.insert(QStringLiteral("details"), activity.details.left(128));
        if (!activity.state.isEmpty())
            body.insert(QStringLiteral("state"), activity.state.left(128));
        QJsonObject assets;
        if (!activity.largeImage.isEmpty()) {
            assets.insert(QStringLiteral("large_image"), activity.largeImage);
            assets.insert(QStringLiteral("large_text"), QStringLiteral("NuClip"));
        }
        if (!activity.smallImage.isEmpty()) {
            assets.insert(QStringLiteral("small_image"), activity.smallImage);
            assets.insert(QStringLiteral("small_text"), QStringLiteral("NuClip"));
        }
        if (!assets.isEmpty())
            body.insert(QStringLiteral("assets"), assets);
        if (activity.timed && m_startedAt > 0) {
            QJsonObject timestamps;
            timestamps.insert(QStringLiteral("start"), m_startedAt);
            body.insert(QStringLiteral("timestamps"), timestamps);
        }
        args.insert(QStringLiteral("activity"), body);
    }
    QJsonObject command;
    command.insert(QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY"));
    command.insert(QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    command.insert(QStringLiteral("args"), args);
    const QByteArray payload = QJsonDocument(command).toJson(QJsonDocument::Compact);
    if (!clear && payload == m_lastPayload)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!clear && m_lastSendMs > 0 && now - m_lastSendMs < 1500) {
        m_lastPayload.clear();
        QTimer::singleShot(1600, this, [this] { scheduleApply(); });
        return;
    }
    sendFrame(kFrame, payload);
    m_lastSendMs = now;
    if (!clear)
        m_lastPayload = payload;
    else
        m_lastPayload.clear();
}

void DiscordPresence::sendFrame(quint32 opcode, const QByteArray &payload)
{
    QByteArray frame(8 + payload.size(), Qt::Uninitialized);
    const quint32 length = quint32(payload.size());
    memcpy(frame.data(), &opcode, 4);
    memcpy(frame.data() + 4, &length, 4);
    memcpy(frame.data() + 8, payload.constData(), payload.size());
    m_socket->write(frame);
}

void DiscordPresence::readFrames()
{
    m_buffer.append(m_socket->readAll());
    while (m_buffer.size() >= 8) {
        quint32 opcode = 0;
        quint32 length = 0;
        memcpy(&opcode, m_buffer.constData(), 4);
        memcpy(&length, m_buffer.constData() + 4, 4);
        if (length > 65536) {
            failLink(QStringLiteral("Reconnecting"), true);
            return;
        }
        if (m_buffer.size() < 8 + int(length))
            return;
        const QByteArray payload = m_buffer.mid(8, int(length));
        m_buffer.remove(0, 8 + int(length));
        onFrame(opcode, payload);
    }
}

void DiscordPresence::onFrame(quint32 opcode, const QByteArray &payload)
{
    if (opcode == kPing) {
        sendFrame(kPong, payload);
        return;
    }
    if (opcode == kClose) {
        const int code = QJsonDocument::fromJson(payload).object().value(QStringLiteral("code")).toInt();
        Log::warn(QStringLiteral("Discord Rich Presence closed (%1)").arg(code));
        if (code == 4000)
            m_rejected = true;
        m_ready = false;
        m_closing = true;
        if (m_socket->state() != QLocalSocket::UnconnectedState)
            m_socket->abort();
        m_closing = false;
        const bool rejected = code == 4000;
        failLink(rejected ? QStringLiteral("Discord rejected the application ID") : QStringLiteral("Reconnecting"),
                 !rejected);
        return;
    }
    if (opcode != kFrame && opcode != kHandshake)
        return;
    const QJsonObject message = QJsonDocument::fromJson(payload).object();
    const QString event = message.value(QStringLiteral("evt")).toString();
    if (event == QLatin1String("READY")) {
        m_ready = true;
        m_loggedAbsent = false;
        m_lastPayload.clear();
        setStatus(QStringLiteral("Connected"));
        emit statusChanged();
        Log::info(QStringLiteral("Discord Rich Presence connected"));
        scheduleApply();
        return;
    }
    if (event == QLatin1String("ERROR")) {
        const int code = message.value(QStringLiteral("data")).toObject().value(QStringLiteral("code")).toInt();
        Log::warn(QStringLiteral("Discord Rich Presence was rejected (%1)").arg(code));
        if (code == 4000)
            m_rejected = true;
        m_ready = false;
        m_closing = true;
        m_socket->abort();
        m_closing = false;
        failLink(code == 4000 ? QStringLiteral("Discord rejected the application ID") : QStringLiteral("Reconnecting"),
                 code != 4000);
    }
}

void DiscordPresence::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void DiscordPresence::scheduleRetry()
{
    if (!m_stopped && wantsLink() && !m_retry->isActive())
        m_retry->start();
}

void DiscordPresence::failLink(const QString &status, bool retry)
{
    m_ready = false;
    if (status == QLatin1String("Discord is not running")) {
        if (!m_loggedAbsent) {
            m_loggedAbsent = true;
            Log::info(QStringLiteral("Discord is not running"));
        }
    }
    setStatus(status);
    if (retry)
        scheduleRetry();
    else
        m_retry->stop();
}

QString discordPresenceSelfCheck()
{
    const auto check = [](const CopyInput &in, const QString &details, const QString &state) {
        const Copy copy = composeCopy(in);
        if (copy.details != details || copy.state != state)
            return QStringLiteral("expected \"%1\" / \"%2\", got \"%3\" / \"%4\"")
                .arg(details, state, copy.details, copy.state);
        if (copy.details.contains(QLatin1Char('\\')) || copy.state.contains(QStringLiteral(".exe")))
            return QStringLiteral("presence leaked a path or executable");
        if (!copy.largeImage.startsWith(QStringLiteral("https://")))
            return QStringLiteral("presence image was not set");
        return QString();
    };
    CopyInput idle;
    if (const QString error = check(idle, QStringLiteral("Ready to capture."), QString()); !error.isEmpty())
        return error;

    CopyInput replay;
    replay.mode = QStringLiteral("replay");
    replay.recordState = RecordingEngine::Recording;
    replay.game = QStringLiteral("Roblox");
    replay.bufferSeconds = 30;
    if (const QString error = check(replay, QStringLiteral("Capturing Roblox"), QStringLiteral("Replay Buffer · 30 seconds"));
        !error.isEmpty())
        return error;

    replay.showGame = false;
    replay.showBuffer = false;
    if (const QString error = check(replay, QStringLiteral("Using NuClip"), QStringLiteral("Replay Buffer Active"));
        !error.isEmpty())
        return error;

    CopyInput session;
    session.mode = QStringLiteral("session");
    session.recordState = RecordingEngine::Recording;
    session.game = QStringLiteral("Minecraft");
    if (const QString error = check(session, QStringLiteral("Recording Minecraft"), QStringLiteral("Continuous Recording"));
        !error.isEmpty())
        return error;

    CopyInput hidden = session;
    hidden.showRecording = false;
    hidden.editing = true;
    if (const QString error = check(hidden, QStringLiteral("Editing a clip"), QString()); !error.isEmpty())
        return error;

    CopyInput exporting = hidden;
    exporting.exporting = true;
    if (const QString error = check(exporting, QStringLiteral("Exporting a clip"), QString()); !error.isEmpty())
        return error;

    CopyInput leaked;
    leaked.mode = QStringLiteral("replay");
    leaked.recordState = RecordingEngine::Recording;
    leaked.game = QStringLiteral("C:\\Users\\clip.exe");
    if (const QString error = check(leaked, QStringLiteral("Using NuClip"), QStringLiteral("Replay Buffer Active"));
        !error.isEmpty())
        return error;
    return {};
}

} // namespace nuclip
