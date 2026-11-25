#include "core/Settings.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "platform/WinUtil.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace nuclip {
namespace {

int clampInt(int value, int lo, int hi)
{
    return std::clamp(value, lo, hi);
}

double clampUnit(double value)
{
    if (value < 0)
        return 0;
    if (value > 1)
        return 1;
    return value;
}

QString oneOf(const QString &value, std::initializer_list<const char *> allowed, const QString &fallback)
{
    for (const char *item : allowed) {
        if (value == QLatin1String(item))
            return value;
    }
    return fallback;
}

QString normalizeQuality(const QString &value, const QString &fallback)
{
    QString id = value;
    if (id == QLatin1String("low"))
        id = QStringLiteral("performance");
    else if (id == QLatin1String("medium"))
        id = QStringLiteral("balanced");
    return oneOf(id, {"performance", "balanced", "high", "ultra", "custom"}, fallback);
}

int snapFps(int value)
{
    static const int rates[] = {30, 60, 90, 120, 144, 165, 240};
    int best = 60;
    int bestDistance = 1000;
    for (int rate : rates) {
        const int distance = std::abs(rate - value);
        if (distance < bestDistance || (distance == bestDistance && rate > best)) {
            bestDistance = distance;
            best = rate;
        }
    }
    return best;
}

int audioForQuality(const QString &quality)
{
    if (quality == QLatin1String("performance") || quality == QLatin1String("low"))
        return 128;
    if (quality == QLatin1String("balanced") || quality == QLatin1String("medium"))
        return 160;
    if (quality == QLatin1String("ultra"))
        return 256;
    return 192;
}

} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(200);
    connect(m_saveTimer, &QTimer::timeout, this, &Settings::saveNow);
}

void Settings::setPersistEnabled(bool enabled)
{
    m_persist = enabled;
    if (!enabled)
        m_saveTimer->stop();
}

void Settings::load()
{
    m_loading = true;
    QFile file(Paths::settingsFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
        if (o.contains(QStringLiteral("launchOnStartup")))
            m_launchOnStartup = o.value(QStringLiteral("launchOnStartup")).toBool();
        if (o.contains(QStringLiteral("updatePrompt")))
            m_updatePrompt = o.value(QStringLiteral("updatePrompt")).toBool();
        if (o.contains(QStringLiteral("minimizeToTray")))
            m_minimizeToTray = o.value(QStringLiteral("minimizeToTray")).toBool();
        if (o.contains(QStringLiteral("notifications")))
            m_notifications = o.value(QStringLiteral("notifications")).toBool();
        if (o.contains(QStringLiteral("autoStartReplay")))
            m_autoStartReplay = o.value(QStringLiteral("autoStartReplay")).toBool();
        if (o.contains(QStringLiteral("saveDirectory")))
            m_saveDirectory = o.value(QStringLiteral("saveDirectory")).toString();

        if (o.contains(QStringLiteral("replaySeconds")))
            m_replaySeconds = clampInt(o.value(QStringLiteral("replaySeconds")).toInt(), 5, 600);
        if (o.contains(QStringLiteral("captureMode")))
            m_captureMode = oneOf(o.value(QStringLiteral("captureMode")).toString(), {"auto", "monitor"}, m_captureMode);
        if (o.contains(QStringLiteral("monitorIndex")))
            m_monitorIndex = std::max(0, o.value(QStringLiteral("monitorIndex")).toInt());
        if (o.contains(QStringLiteral("quality")))
            m_quality = normalizeQuality(o.value(QStringLiteral("quality")).toString(), m_quality);
        // Width and height are no longer recording settings. Capture uses the display.
        m_width = 0;
        m_height = 0;
        if (o.contains(QStringLiteral("fps")))
            m_fps = snapFps(o.value(QStringLiteral("fps")).toInt(60));
        if (o.contains(QStringLiteral("videoBitrateKbps")))
            m_videoBitrateKbps = clampInt(o.value(QStringLiteral("videoBitrateKbps")).toInt(), 500, 200000);
        if (o.contains(QStringLiteral("encoder")))
            m_encoder = oneOf(o.value(QStringLiteral("encoder")).toString(), {"auto", "nvenc", "amf", "qsv", "x264"}, m_encoder);
        if (o.contains(QStringLiteral("codec")))
            m_codec = oneOf(o.value(QStringLiteral("codec")).toString(), {"h264", "hevc", "av1"}, m_codec);
        if (o.contains(QStringLiteral("audioBitrateKbps")))
            m_audioBitrateKbps = clampInt(o.value(QStringLiteral("audioBitrateKbps")).toInt(), 64, 512);
        if (o.contains(QStringLiteral("showCursor")))
            m_showCursor = o.value(QStringLiteral("showCursor")).toBool();

        if (o.contains(QStringLiteral("systemAudio")))
            m_systemAudio = o.value(QStringLiteral("systemAudio")).toBool();
        if (o.contains(QStringLiteral("systemVolume")))
            m_systemVolume = clampUnit(o.value(QStringLiteral("systemVolume")).toDouble(1));
        if (o.contains(QStringLiteral("outputDeviceId")))
            m_outputDeviceId = o.value(QStringLiteral("outputDeviceId")).toString();
        if (o.contains(QStringLiteral("micEnabled")))
            m_micEnabled = o.value(QStringLiteral("micEnabled")).toBool();
        if (o.contains(QStringLiteral("micVolume")))
            m_micVolume = clampUnit(o.value(QStringLiteral("micVolume")).toDouble(1));
        if (o.contains(QStringLiteral("micDeviceId")))
            m_micDeviceId = o.value(QStringLiteral("micDeviceId")).toString();

        auto hotkey = [&](const char *key, QString &field) {
            if (o.contains(QLatin1String(key)))
                field = o.value(QLatin1String(key)).toString();
        };
        hotkey("hotkeyClip", m_hotkeyClip);
        hotkey("hotkeyClip15", m_hotkeyClip15);
        hotkey("hotkeyClip30", m_hotkeyClip30);
        hotkey("hotkeyClip60", m_hotkeyClip60);
        hotkey("hotkeyClip120", m_hotkeyClip120);
        hotkey("hotkeyScreenshot", m_hotkeyScreenshot);
        hotkey("hotkeyToggle", m_hotkeyToggle);
        hotkey("hotkeyPause", m_hotkeyPause);

        if (o.contains(QStringLiteral("themeMode")))
            m_themeMode = oneOf(o.value(QStringLiteral("themeMode")).toString(), {"dark", "light", "system"}, m_themeMode);
        if (o.contains(QStringLiteral("accent")))
            m_accent = oneOf(o.value(QStringLiteral("accent")).toString(),
                             {"green", "blue", "purple", "red", "orange", "pink", "cyan", "custom"}, m_accent);
        if (o.contains(QStringLiteral("customAccent")))
            m_customAccent = o.value(QStringLiteral("customAccent")).toString();

        if (o.contains(QStringLiteral("organizeByGame")))
            m_organizeByGame = o.value(QStringLiteral("organizeByGame")).toBool();
        if (o.contains(QStringLiteral("storageLimitGb")))
            m_storageLimitGb = clampInt(o.value(QStringLiteral("storageLimitGb")).toInt(), 0, 10000);
        if (o.contains(QStringLiteral("autoCleanup")))
            m_autoCleanup = o.value(QStringLiteral("autoCleanup")).toBool();

        if (o.contains(QStringLiteral("exportFormat")))
            m_exportFormat = oneOf(o.value(QStringLiteral("exportFormat")).toString(), {"mp4", "mkv", "webm"}, m_exportFormat);
        if (o.contains(QStringLiteral("exportQuality")))
            m_exportQuality = oneOf(o.value(QStringLiteral("exportQuality")).toString(),
                                    {"low", "medium", "high", "ultra"}, m_exportQuality);
        if (o.contains(QStringLiteral("exportHeight")))
            m_exportHeight = std::max(0, o.value(QStringLiteral("exportHeight")).toInt());
        if (o.contains(QStringLiteral("discordCompress")))
            m_discordCompress = o.value(QStringLiteral("discordCompress")).toBool();
        if (o.contains(QStringLiteral("discordTargetMb")))
            m_discordTargetMb = clampInt(o.value(QStringLiteral("discordTargetMb")).toInt(10), 2, 25);
        const QJsonObject overlay = o.value(QStringLiteral("overlay")).toObject();
        if (!overlay.isEmpty()) {
            if (overlay.contains(QStringLiteral("enabled")))
                m_overlayEnabled = overlay.value(QStringLiteral("enabled")).toBool();
            if (overlay.contains(QStringLiteral("clip")))
                m_overlayClip = overlay.value(QStringLiteral("clip")).toBool();
            if (overlay.contains(QStringLiteral("screenshot")))
                m_overlayScreenshot = overlay.value(QStringLiteral("screenshot")).toBool();
            const bool legacyRecording = overlay.contains(QStringLiteral("recording"))
                ? overlay.value(QStringLiteral("recording")).toBool()
                : true;
            m_overlayRecordStart = overlay.contains(QStringLiteral("recordStart"))
                ? overlay.value(QStringLiteral("recordStart")).toBool()
                : legacyRecording;
            m_overlayRecordStop = overlay.contains(QStringLiteral("recordStop"))
                ? overlay.value(QStringLiteral("recordStop")).toBool()
                : legacyRecording;
            if (overlay.contains(QStringLiteral("error")))
                m_overlayError = overlay.value(QStringLiteral("error")).toBool();
            if (overlay.contains(QStringLiteral("position"))) {
                m_overlayPosition = oneOf(overlay.value(QStringLiteral("position")).toString(),
                                          {"topLeft", "topCenter", "topRight", "centerLeft", "centerRight",
                                           "bottomLeft", "bottomCenter", "bottomRight"},
                                          m_overlayPosition);
            }
            if (overlay.contains(QStringLiteral("durationMs")))
                m_overlayDurationMs = clampInt(overlay.value(QStringLiteral("durationMs")).toInt(3000), 1000, 10000);
            if (overlay.contains(QStringLiteral("size")))
                m_overlaySize = oneOf(overlay.value(QStringLiteral("size")).toString(), {"compact", "normal", "large"}, m_overlaySize);
            if (overlay.contains(QStringLiteral("animation")))
                m_overlayAnimation = oneOf(overlay.value(QStringLiteral("animation")).toString(), {"slide", "fade", "none"}, m_overlayAnimation);
            if (overlay.contains(QStringLiteral("useAccent")))
                m_overlayUseAccent = overlay.value(QStringLiteral("useAccent")).toBool();
            if (overlay.contains(QStringLiteral("opacity")))
                m_overlayOpacity = std::clamp(overlay.value(QStringLiteral("opacity")).toDouble(0.95), 0.10, 1.0);
        }
        const QJsonObject sfx = o.value(QStringLiteral("sfx")).toObject();
        if (!sfx.isEmpty()) {
            if (sfx.contains(QStringLiteral("enabled")))
                m_sfxEnabled = sfx.value(QStringLiteral("enabled")).toBool();
            if (sfx.contains(QStringLiteral("volume")))
                m_sfxVolume = std::clamp(sfx.value(QStringLiteral("volume")).toDouble(0.7), 0.0, 1.0);
            if (sfx.contains(QStringLiteral("sound")))
                m_sfxSound = oneOf(sfx.value(QStringLiteral("sound")).toString(), {"nuclip"}, m_sfxSound);
            if (sfx.contains(QStringLiteral("clip")))
                m_sfxClip = sfx.value(QStringLiteral("clip")).toBool();
            if (sfx.contains(QStringLiteral("screenshot")))
                m_sfxScreenshot = sfx.value(QStringLiteral("screenshot")).toBool();
            if (sfx.contains(QStringLiteral("recordStart")))
                m_sfxRecordStart = sfx.value(QStringLiteral("recordStart")).toBool();
            if (sfx.contains(QStringLiteral("recordStop")))
                m_sfxRecordStop = sfx.value(QStringLiteral("recordStop")).toBool();
            if (sfx.contains(QStringLiteral("error")))
                m_sfxError = sfx.value(QStringLiteral("error")).toBool();
        }
        const QJsonObject discord = o.value(QStringLiteral("discord")).toObject();
        if (!discord.isEmpty()) {
            if (discord.contains(QStringLiteral("enabled")))
                m_discordEnabled = discord.value(QStringLiteral("enabled")).toBool();
            if (discord.contains(QStringLiteral("autoConnect")))
                m_discordAutoConnect = discord.value(QStringLiteral("autoConnect")).toBool();
            if (discord.contains(QStringLiteral("showGame")))
                m_discordShowGame = discord.value(QStringLiteral("showGame")).toBool();
            if (discord.contains(QStringLiteral("showRecording")))
                m_discordShowRecording = discord.value(QStringLiteral("showRecording")).toBool();
            if (discord.contains(QStringLiteral("showBuffer")))
                m_discordShowBuffer = discord.value(QStringLiteral("showBuffer")).toBool();
            if (discord.contains(QStringLiteral("showEditing")))
                m_discordShowEditing = discord.value(QStringLiteral("showEditing")).toBool();
        }
        const QJsonObject names = o.value(QStringLiteral("appNames")).toObject();
        for (auto it = names.begin(); it != names.end(); ++it) {
            const QString value = it.value().toString().trimmed();
            if (!it.key().isEmpty() && !value.isEmpty())
                m_appNames.insert(it.key(), value);
        }
    }
    m_loading = false;
    QDir().mkpath(clipsDirectory());
    Log::info(QStringLiteral("Settings loaded from %1").arg(Paths::settingsFile()));
}

QString Settings::clipsDirectory() const
{
    if (!m_saveDirectory.trimmed().isEmpty())
        return QDir::cleanPath(m_saveDirectory);
    return Paths::defaultClipsDir();
}

QString Settings::screenshotsDirectory() const
{
    return Paths::screenshotsDirForClips(clipsDirectory());
}

QString Settings::qualityLabel() const
{
    if (m_quality == QLatin1String("performance"))
        return QStringLiteral("Performance");
    if (m_quality == QLatin1String("balanced"))
        return QStringLiteral("Balanced");
    if (m_quality == QLatin1String("high"))
        return QStringLiteral("High");
    if (m_quality == QLatin1String("ultra"))
        return QStringLiteral("Ultra");
    return QStringLiteral("Custom");
}

int Settings::exportVideoBitrateKbps() const
{
    if (m_exportQuality == QLatin1String("low"))
        return 2500;
    if (m_exportQuality == QLatin1String("medium"))
        return 6000;
    if (m_exportQuality == QLatin1String("ultra"))
        return 20000;
    return 12000;
}

int Settings::audioBitrateFor(const QString &quality) const
{
    return audioForQuality(quality);
}

int Settings::recommendedVideoBitrate(int width, int height, int fps, const QString &codec,
                                      const QString &quality) const
{
    if (width < 320 || height < 240) {
        width = 1920;
        height = 1080;
    }
    const QString id = normalizeQuality(quality, QStringLiteral("high"));
    double bitsPerPixel = 0.12;
    if (id == QLatin1String("performance"))
        bitsPerPixel = 0.05;
    else if (id == QLatin1String("balanced"))
        bitsPerPixel = 0.08;
    else if (id == QLatin1String("ultra"))
        bitsPerPixel = 0.18;
    double codecScale = 1.0;
    if (codec == QLatin1String("hevc"))
        codecScale = 0.65;
    else if (codec == QLatin1String("av1"))
        codecScale = 0.55;
    const double kbps = double(width) * double(height) * double(std::max(30, fps)) * bitsPerPixel * codecScale / 1000.0;
    const int rounded = int(std::lround(kbps / 100.0)) * 100;
    return clampInt(rounded, 1500, 150000);
}

void Settings::applyDerivedBitrate(int width, int height)
{
    if (m_loading || m_quality == QLatin1String("custom"))
        return;
    m_applyingPreset = true;
    setVideoBitrateKbps(recommendedVideoBitrate(width, height, m_fps, m_codec, m_quality));
    setAudioBitrateKbps(audioForQuality(m_quality));
    m_applyingPreset = false;
}

void Settings::editCustomBitrate(int kbps)
{
    m_applyingPreset = true;
    setVideoBitrateKbps(kbps);
    m_applyingPreset = false;
    setQuality(QStringLiteral("custom"));
}

void Settings::applyQualityPreset(const QString &preset)
{
    const QString id = normalizeQuality(preset, QStringLiteral("high"));
    if (id == QLatin1String("custom")) {
        setQuality(id);
        return;
    }
    m_applyingPreset = true;
    setWidth(0);
    setHeight(0);
    setAudioBitrateKbps(audioForQuality(id));
    setVideoBitrateKbps(recommendedVideoBitrate(1920, 1080, m_fps, m_codec, id));
    m_applyingPreset = false;
    setQuality(id);
}

void Settings::markCustom()
{
    if (m_loading || m_applyingPreset || m_quality == QLatin1String("custom"))
        return;
    setQuality(QStringLiteral("custom"));
}

void Settings::scheduleSave()
{
    if (m_loading || !m_persist)
        return;
    m_saveTimer->start();
}

void Settings::saveNow()
{
    if (!m_persist)
        return;
    QJsonObject o;
    o.insert(QStringLiteral("launchOnStartup"), m_launchOnStartup);
    o.insert(QStringLiteral("updatePrompt"), m_updatePrompt);
    o.insert(QStringLiteral("minimizeToTray"), m_minimizeToTray);
    o.insert(QStringLiteral("notifications"), m_notifications);
    o.insert(QStringLiteral("autoStartReplay"), m_autoStartReplay);
    o.insert(QStringLiteral("saveDirectory"), m_saveDirectory);
    o.insert(QStringLiteral("replaySeconds"), m_replaySeconds);
    o.insert(QStringLiteral("captureMode"), m_captureMode);
    o.insert(QStringLiteral("monitorIndex"), m_monitorIndex);
    o.insert(QStringLiteral("quality"), m_quality);
    o.insert(QStringLiteral("width"), m_width);
    o.insert(QStringLiteral("height"), m_height);
    o.insert(QStringLiteral("fps"), m_fps);
    o.insert(QStringLiteral("videoBitrateKbps"), m_videoBitrateKbps);
    o.insert(QStringLiteral("encoder"), m_encoder);
    o.insert(QStringLiteral("codec"), m_codec);
    o.insert(QStringLiteral("audioBitrateKbps"), m_audioBitrateKbps);
    o.insert(QStringLiteral("showCursor"), m_showCursor);
    o.insert(QStringLiteral("systemAudio"), m_systemAudio);
    o.insert(QStringLiteral("systemVolume"), m_systemVolume);
    o.insert(QStringLiteral("outputDeviceId"), m_outputDeviceId);
    o.insert(QStringLiteral("micEnabled"), m_micEnabled);
    o.insert(QStringLiteral("micVolume"), m_micVolume);
    o.insert(QStringLiteral("micDeviceId"), m_micDeviceId);
    o.insert(QStringLiteral("hotkeyClip"), m_hotkeyClip);
    o.insert(QStringLiteral("hotkeyClip15"), m_hotkeyClip15);
    o.insert(QStringLiteral("hotkeyClip30"), m_hotkeyClip30);
    o.insert(QStringLiteral("hotkeyClip60"), m_hotkeyClip60);
    o.insert(QStringLiteral("hotkeyClip120"), m_hotkeyClip120);
    o.insert(QStringLiteral("hotkeyScreenshot"), m_hotkeyScreenshot);
    o.insert(QStringLiteral("hotkeyToggle"), m_hotkeyToggle);
    o.insert(QStringLiteral("hotkeyPause"), m_hotkeyPause);
    o.insert(QStringLiteral("themeMode"), m_themeMode);
    o.insert(QStringLiteral("accent"), m_accent);
    o.insert(QStringLiteral("customAccent"), m_customAccent);
    o.insert(QStringLiteral("organizeByGame"), m_organizeByGame);
    o.insert(QStringLiteral("storageLimitGb"), m_storageLimitGb);
    o.insert(QStringLiteral("autoCleanup"), m_autoCleanup);
    o.insert(QStringLiteral("exportFormat"), m_exportFormat);
    o.insert(QStringLiteral("exportQuality"), m_exportQuality);
    o.insert(QStringLiteral("exportHeight"), m_exportHeight);
    o.insert(QStringLiteral("discordCompress"), m_discordCompress);
    o.insert(QStringLiteral("discordTargetMb"), m_discordTargetMb);
    QJsonObject overlay;
    overlay.insert(QStringLiteral("enabled"), m_overlayEnabled);
    overlay.insert(QStringLiteral("clip"), m_overlayClip);
    overlay.insert(QStringLiteral("screenshot"), m_overlayScreenshot);
    overlay.insert(QStringLiteral("recordStart"), m_overlayRecordStart);
    overlay.insert(QStringLiteral("recordStop"), m_overlayRecordStop);
    overlay.insert(QStringLiteral("error"), m_overlayError);
    overlay.insert(QStringLiteral("position"), m_overlayPosition);
    overlay.insert(QStringLiteral("durationMs"), m_overlayDurationMs);
    overlay.insert(QStringLiteral("size"), m_overlaySize);
    overlay.insert(QStringLiteral("animation"), m_overlayAnimation);
    overlay.insert(QStringLiteral("useAccent"), m_overlayUseAccent);
    overlay.insert(QStringLiteral("opacity"), m_overlayOpacity);
    o.insert(QStringLiteral("overlay"), overlay);
    QJsonObject sfx;
    sfx.insert(QStringLiteral("enabled"), m_sfxEnabled);
    sfx.insert(QStringLiteral("volume"), m_sfxVolume);
    sfx.insert(QStringLiteral("sound"), m_sfxSound);
    sfx.insert(QStringLiteral("clip"), m_sfxClip);
    sfx.insert(QStringLiteral("screenshot"), m_sfxScreenshot);
    sfx.insert(QStringLiteral("recordStart"), m_sfxRecordStart);
    sfx.insert(QStringLiteral("recordStop"), m_sfxRecordStop);
    sfx.insert(QStringLiteral("error"), m_sfxError);
    o.insert(QStringLiteral("sfx"), sfx);
    QJsonObject discord;
    discord.insert(QStringLiteral("enabled"), m_discordEnabled);
    discord.insert(QStringLiteral("autoConnect"), m_discordAutoConnect);
    discord.insert(QStringLiteral("showGame"), m_discordShowGame);
    discord.insert(QStringLiteral("showRecording"), m_discordShowRecording);
    discord.insert(QStringLiteral("showBuffer"), m_discordShowBuffer);
    discord.insert(QStringLiteral("showEditing"), m_discordShowEditing);
    o.insert(QStringLiteral("discord"), discord);
    QJsonObject names;
    for (auto it = m_appNames.cbegin(); it != m_appNames.cend(); ++it)
        names.insert(it.key(), it.value());
    o.insert(QStringLiteral("appNames"), names);

    QFile file(Paths::settingsFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Log::error(QStringLiteral("Could not write settings: %1").arg(file.errorString()));
        return;
    }
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

QString Settings::appDisplayName(const QString &stableId) const
{
    return m_appNames.value(stableId);
}

void Settings::setAppDisplayName(const QString &stableId, const QString &name)
{
    const QString id = stableId.trimmed();
    if (id.isEmpty())
        return;
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (!m_appNames.remove(id))
            return;
    } else if (m_appNames.value(id) == trimmed) {
        return;
    } else {
        m_appNames.insert(id, trimmed.left(48));
    }
    emit appNamesChanged();
    scheduleSave();
}

void Settings::setLaunchOnStartup(bool value)
{
    if (m_launchOnStartup == value)
        return;
    m_launchOnStartup = value;
    if (!m_loading)
        WinUtil::setLaunchOnStartup(value);
    emit launchOnStartupChanged();
    scheduleSave();
}

void Settings::setUpdatePrompt(bool value)
{
    if (m_updatePrompt == value)
        return;
    m_updatePrompt = value;
    emit updatePromptChanged();
    scheduleSave();
}

void Settings::setMinimizeToTray(bool value)
{
    if (m_minimizeToTray == value)
        return;
    m_minimizeToTray = value;
    emit minimizeToTrayChanged();
    scheduleSave();
}

void Settings::setNotifications(bool value)
{
    if (m_notifications == value)
        return;
    m_notifications = value;
    emit notificationsChanged();
    scheduleSave();
}

void Settings::setAutoStartReplay(bool value)
{
    if (m_autoStartReplay == value)
        return;
    m_autoStartReplay = value;
    emit autoStartReplayChanged();
    scheduleSave();
}

void Settings::setSaveDirectory(const QString &value)
{
    const QString clean = value.trimmed().isEmpty() ? QString() : QDir::cleanPath(value);
    if (m_saveDirectory == clean)
        return;
    m_saveDirectory = clean;
    QDir().mkpath(clipsDirectory());
    emit saveDirectoryChanged();
    emit clipsDirectoryChanged();
    scheduleSave();
}

void Settings::setReplaySeconds(int value)
{
    value = clampInt(value, 5, 600);
    if (m_replaySeconds == value)
        return;
    m_replaySeconds = value;
    emit replaySecondsChanged();
    scheduleSave();
}

void Settings::setCaptureMode(const QString &value)
{
    const QString id = oneOf(value, {"auto", "monitor"}, m_captureMode);
    if (m_captureMode == id)
        return;
    m_captureMode = id;
    emit captureModeChanged();
    scheduleSave();
}

void Settings::setMonitorIndex(int value)
{
    value = std::max(0, value);
    if (m_monitorIndex == value)
        return;
    m_monitorIndex = value;
    emit monitorIndexChanged();
    scheduleSave();
}

void Settings::setQuality(const QString &value)
{
    const QString id = normalizeQuality(value, m_quality);
    if (m_quality == id)
        return;
    m_quality = id;
    emit qualityChanged();
    scheduleSave();
}

void Settings::setWidth(int value)
{
    value = std::max(0, value);
    if (m_width == value)
        return;
    m_width = value;
    markCustom();
    emit videoChanged();
    scheduleSave();
}

void Settings::setHeight(int value)
{
    value = std::max(0, value);
    if (m_height == value)
        return;
    m_height = value;
    markCustom();
    emit videoChanged();
    scheduleSave();
}

void Settings::setFps(int value)
{
    value = snapFps(value);
    if (m_fps == value)
        return;
    m_fps = value;
    emit videoChanged();
    scheduleSave();
}

void Settings::setVideoBitrateKbps(int value)
{
    value = clampInt(value, 500, 200000);
    if (m_videoBitrateKbps == value)
        return;
    m_videoBitrateKbps = value;
    emit videoChanged();
    scheduleSave();
}

void Settings::setEncoder(const QString &value)
{
    const QString id = oneOf(value, {"auto", "nvenc", "amf", "qsv", "x264"}, m_encoder);
    if (m_encoder == id)
        return;
    m_encoder = id;
    emit encoderChanged();
    scheduleSave();
}

void Settings::setCodec(const QString &value)
{
    const QString id = oneOf(value, {"h264", "hevc", "av1"}, m_codec);
    if (m_codec == id)
        return;
    m_codec = id;
    emit codecChanged();
    emit videoChanged();
    scheduleSave();
}

void Settings::setAudioBitrateKbps(int value)
{
    value = clampInt(value, 64, 512);
    if (m_audioBitrateKbps == value)
        return;
    m_audioBitrateKbps = value;
    emit audioBitrateChanged();
    scheduleSave();
}

void Settings::setShowCursor(bool value)
{
    if (m_showCursor == value)
        return;
    m_showCursor = value;
    emit showCursorChanged();
    scheduleSave();
}

void Settings::setSystemAudio(bool value)
{
    if (m_systemAudio == value)
        return;
    m_systemAudio = value;
    emit systemAudioChanged();
    scheduleSave();
}

void Settings::setSystemVolume(double value)
{
    value = clampUnit(value);
    if (std::abs(m_systemVolume - value) < 0.0001)
        return;
    m_systemVolume = value;
    emit systemVolumeChanged();
    scheduleSave();
}

void Settings::setOutputDeviceId(const QString &value)
{
    if (m_outputDeviceId == value)
        return;
    m_outputDeviceId = value;
    emit outputDeviceIdChanged();
    scheduleSave();
}

void Settings::setMicEnabled(bool value)
{
    if (m_micEnabled == value)
        return;
    m_micEnabled = value;
    emit micEnabledChanged();
    scheduleSave();
}

void Settings::setMicVolume(double value)
{
    value = clampUnit(value);
    if (std::abs(m_micVolume - value) < 0.0001)
        return;
    m_micVolume = value;
    emit micVolumeChanged();
    scheduleSave();
}

void Settings::setMicDeviceId(const QString &value)
{
    if (m_micDeviceId == value)
        return;
    m_micDeviceId = value;
    emit micDeviceIdChanged();
    scheduleSave();
}

void Settings::setHotkeyClip(const QString &value)
{
    if (m_hotkeyClip == value)
        return;
    m_hotkeyClip = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyClip15(const QString &value)
{
    if (m_hotkeyClip15 == value)
        return;
    m_hotkeyClip15 = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyClip30(const QString &value)
{
    if (m_hotkeyClip30 == value)
        return;
    m_hotkeyClip30 = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyClip60(const QString &value)
{
    if (m_hotkeyClip60 == value)
        return;
    m_hotkeyClip60 = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyClip120(const QString &value)
{
    if (m_hotkeyClip120 == value)
        return;
    m_hotkeyClip120 = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyScreenshot(const QString &value)
{
    if (m_hotkeyScreenshot == value)
        return;
    m_hotkeyScreenshot = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyToggle(const QString &value)
{
    if (m_hotkeyToggle == value)
        return;
    m_hotkeyToggle = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setHotkeyPause(const QString &value)
{
    if (m_hotkeyPause == value)
        return;
    m_hotkeyPause = value;
    emit hotkeysChanged();
    scheduleSave();
}

void Settings::setThemeMode(const QString &value)
{
    const QString id = oneOf(value, {"dark", "light", "system"}, m_themeMode);
    if (m_themeMode == id)
        return;
    m_themeMode = id;
    emit themeModeChanged();
    scheduleSave();
}

void Settings::setAccent(const QString &value)
{
    const QString id = oneOf(value, {"green", "blue", "purple", "red", "orange", "pink", "cyan", "custom"}, m_accent);
    if (m_accent == id)
        return;
    m_accent = id;
    emit accentChanged();
    scheduleSave();
}

void Settings::setCustomAccent(const QString &value)
{
    QString hex = value.trimmed();
    if (!hex.startsWith(QLatin1Char('#')))
        hex.prepend(QLatin1Char('#'));
    if (hex.size() != 7)
        return;
    for (int i = 1; i < hex.size(); ++i) {
        const QChar c = hex.at(i);
        const bool ok = (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
            || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
        if (!ok)
            return;
    }
    hex = hex.toUpper();
    if (m_customAccent == hex)
        return;
    m_customAccent = hex;
    emit accentChanged();
    scheduleSave();
}

void Settings::setOrganizeByGame(bool value)
{
    if (m_organizeByGame == value)
        return;
    m_organizeByGame = value;
    emit organizeByGameChanged();
    scheduleSave();
}

void Settings::setStorageLimitGb(int value)
{
    value = clampInt(value, 0, 10000);
    if (m_storageLimitGb == value)
        return;
    m_storageLimitGb = value;
    emit storageLimitChanged();
    scheduleSave();
}

void Settings::setAutoCleanup(bool value)
{
    if (m_autoCleanup == value)
        return;
    m_autoCleanup = value;
    emit storageLimitChanged();
    scheduleSave();
}

void Settings::setExportFormat(const QString &value)
{
    const QString id = oneOf(value, {"mp4", "mkv", "webm"}, m_exportFormat);
    if (m_exportFormat == id)
        return;
    m_exportFormat = id;
    emit exportChanged();
    scheduleSave();
}

void Settings::setExportQuality(const QString &value)
{
    const QString id = oneOf(value, {"low", "medium", "high", "ultra"}, m_exportQuality);
    if (m_exportQuality == id)
        return;
    m_exportQuality = id;
    emit exportChanged();
    scheduleSave();
}

void Settings::setExportHeight(int value)
{
    value = std::max(0, value);
    if (m_exportHeight == value)
        return;
    m_exportHeight = value;
    emit exportChanged();
    scheduleSave();
}

void Settings::setDiscordCompress(bool value)
{
    if (m_discordCompress == value)
        return;
    m_discordCompress = value;
    emit exportChanged();
    scheduleSave();
}

void Settings::setDiscordTargetMb(int value)
{
    value = clampInt(value, 2, 25);
    if (m_discordTargetMb == value)
        return;
    m_discordTargetMb = value;
    emit exportChanged();
    scheduleSave();
}

void Settings::setOverlayEnabled(bool value)
{
    if (m_overlayEnabled == value)
        return;
    m_overlayEnabled = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayClip(bool value)
{
    if (m_overlayClip == value)
        return;
    m_overlayClip = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayScreenshot(bool value)
{
    if (m_overlayScreenshot == value)
        return;
    m_overlayScreenshot = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayRecordStart(bool value)
{
    if (m_overlayRecordStart == value)
        return;
    m_overlayRecordStart = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayRecordStop(bool value)
{
    if (m_overlayRecordStop == value)
        return;
    m_overlayRecordStop = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayError(bool value)
{
    if (m_overlayError == value)
        return;
    m_overlayError = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayPosition(const QString &value)
{
    const QString next = oneOf(value, {"topLeft", "topCenter", "topRight", "centerLeft", "centerRight",
                                      "bottomLeft", "bottomCenter", "bottomRight"},
                               m_overlayPosition);
    if (m_overlayPosition == next)
        return;
    m_overlayPosition = next;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayDurationMs(int value)
{
    value = clampInt(value, 1000, 10000);
    if (m_overlayDurationMs == value)
        return;
    m_overlayDurationMs = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlaySize(const QString &value)
{
    const QString next = oneOf(value, {"compact", "normal", "large"}, m_overlaySize);
    if (m_overlaySize == next)
        return;
    m_overlaySize = next;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayAnimation(const QString &value)
{
    const QString next = oneOf(value, {"slide", "fade", "none"}, m_overlayAnimation);
    if (m_overlayAnimation == next)
        return;
    m_overlayAnimation = next;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayUseAccent(bool value)
{
    if (m_overlayUseAccent == value)
        return;
    m_overlayUseAccent = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setOverlayOpacity(double value)
{
    value = std::clamp(value, 0.10, 1.0);
    if (qAbs(m_overlayOpacity - value) < 0.005)
        return;
    m_overlayOpacity = value;
    emit overlayChanged();
    scheduleSave();
}

void Settings::setSfxEnabled(bool value)
{
    if (m_sfxEnabled == value)
        return;
    m_sfxEnabled = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxVolume(double value)
{
    value = std::clamp(value, 0.0, 1.0);
    if (qAbs(m_sfxVolume - value) < 0.005)
        return;
    m_sfxVolume = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxSound(const QString &value)
{
    const QString next = oneOf(value, {"nuclip"}, m_sfxSound);
    if (m_sfxSound == next)
        return;
    m_sfxSound = next;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxClip(bool value)
{
    if (m_sfxClip == value)
        return;
    m_sfxClip = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxScreenshot(bool value)
{
    if (m_sfxScreenshot == value)
        return;
    m_sfxScreenshot = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxRecordStart(bool value)
{
    if (m_sfxRecordStart == value)
        return;
    m_sfxRecordStart = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxRecordStop(bool value)
{
    if (m_sfxRecordStop == value)
        return;
    m_sfxRecordStop = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setSfxError(bool value)
{
    if (m_sfxError == value)
        return;
    m_sfxError = value;
    emit sfxChanged();
    scheduleSave();
}

void Settings::setDiscordEnabled(bool value)
{
    if (m_discordEnabled == value)
        return;
    m_discordEnabled = value;
    emit discordChanged();
    scheduleSave();
}

void Settings::setDiscordAutoConnect(bool value)
{
    if (m_discordAutoConnect == value)
        return;
    m_discordAutoConnect = value;
    emit discordChanged();
    scheduleSave();
}

void Settings::setDiscordShowGame(bool value)
{
    if (m_discordShowGame == value)
        return;
    m_discordShowGame = value;
    emit discordChanged();
    scheduleSave();
}

void Settings::setDiscordShowRecording(bool value)
{
    if (m_discordShowRecording == value)
        return;
    m_discordShowRecording = value;
    emit discordChanged();
    scheduleSave();
}

void Settings::setDiscordShowBuffer(bool value)
{
    if (m_discordShowBuffer == value)
        return;
    m_discordShowBuffer = value;
    emit discordChanged();
    scheduleSave();
}

void Settings::setDiscordShowEditing(bool value)
{
    if (m_discordShowEditing == value)
        return;
    m_discordShowEditing = value;
    emit discordChanged();
    scheduleSave();
}

} // namespace nuclip
