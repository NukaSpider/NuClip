#include "platform/GameDetector.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "platform/AppCatalog.h"
#include "platform/DeviceCatalog.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QUrl>

#include <algorithm>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <tlhelp32.h>

namespace nuclip {
namespace {

struct VersionInfo {
    QString description;
    QString product;
    QString company;
};

QString versionString(const QByteArray &data, const wchar_t *key, WORD language, WORD codepage)
{
    wchar_t query[96];
    swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\%s", language, codepage, key);
    wchar_t *value = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), query, reinterpret_cast<void **>(&value), &length) || !value)
        return {};
    return QString::fromWCharArray(value).trimmed();
}

VersionInfo readVersion(const QString &path)
{
    static QHash<QString, VersionInfo> cache;
    if (cache.contains(path))
        return cache.value(path);
    VersionInfo info;
    const std::wstring native = path.toStdWString();
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(native.c_str(), &handle);
    if (size) {
        QByteArray data(int(size), 0);
        if (GetFileVersionInfoW(native.c_str(), 0, size, data.data())) {
            struct Translation {
                WORD language;
                WORD codepage;
            };
            Translation *translation = nullptr;
            UINT length = 0;
            if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void **>(&translation), &length)
                && translation && length >= sizeof(Translation)) {
                info.description = versionString(data, L"FileDescription", translation->language, translation->codepage);
                info.product = versionString(data, L"ProductName", translation->language, translation->codepage);
                info.company = versionString(data, L"CompanyName", translation->language, translation->codepage);
            }
        }
    }
    cache.insert(path, info);
    return info;
}

QString processPath(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t buffer[1024];
    DWORD size = 1024;
    QString path;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size))
        path = QString::fromWCharArray(buffer, int(size));
    CloseHandle(process);
    return path;
}

DWORD parentPid(DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD parent = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                parent = entry.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return parent;
}

bool cloaked(HWND hwnd)
{
    BOOL value = FALSE;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &value, sizeof(value))) && value;
}

QImage imageFromIcon(HICON icon)
{
    ICONINFO info{};
    if (!GetIconInfo(icon, &info))
        return {};
    BITMAP color{};
    const int got = info.hbmColor ? GetObject(info.hbmColor, sizeof(color), &color) : 0;
    const int width = got ? color.bmWidth : 32;
    const int height = got ? color.bmHeight : 32;
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO header{};
    header.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    header.bmiHeader.biWidth = width;
    header.bmiHeader.biHeight = -height;
    header.bmiHeader.biPlanes = 1;
    header.bmiHeader.biBitCount = 32;
    void *bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &header, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ previous = SelectObject(memory, dib);
    DrawIconEx(memory, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL);
    QImage image(width, height, QImage::Format_ARGB32);
    if (bits)
        memcpy(image.bits(), bits, size_t(width * height * 4));
    bool hasAlpha = false;
    auto *pixels = reinterpret_cast<QRgb *>(image.bits());
    const int count = image.width() * image.height();
    for (int i = 0; i < count; ++i) {
        if (pixels[i] >> 24) {
            hasAlpha = true;
            break;
        }
    }
    if (!hasAlpha) {
        for (int i = 0; i < count; ++i) {
            if (qRed(pixels[i]) || qGreen(pixels[i]) || qBlue(pixels[i]))
                pixels[i] |= 0xFF000000;
        }
    }
    SelectObject(memory, previous);
    DeleteObject(dib);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    if (info.hbmColor)
        DeleteObject(info.hbmColor);
    if (info.hbmMask)
        DeleteObject(info.hbmMask);
    return image;
}

QString cachedIcon(const QString &path)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return {};
    const QString directory = QDir(Paths::dataDir()).filePath(QStringLiteral("icons"));
    QDir().mkpath(directory);
    const QString key = QString::number(qHash(QFileInfo(path).absoluteFilePath()), 16);
    const QString png = QDir(directory).filePath(key + QStringLiteral(".png"));
    const QFileInfo exe(path);
    const QFileInfo cached(png);
    if (cached.exists() && cached.lastModified() >= exe.lastModified())
        return QUrl::fromLocalFile(png).toString();
    HICON iconLarge = nullptr;
    HICON iconSmall = nullptr;
    const std::wstring native = path.toStdWString();
    ExtractIconExW(native.c_str(), 0, &iconLarge, &iconSmall, 1);
    HICON icon = iconLarge ? iconLarge : iconSmall;
    QImage image = icon ? imageFromIcon(icon) : QImage();
    if (iconLarge)
        DestroyIcon(iconLarge);
    if (iconSmall)
        DestroyIcon(iconSmall);
    if (image.isNull() || image.width() < 2)
        return {};
    if (!image.save(png, "PNG"))
        return {};
    return QUrl::fromLocalFile(png).toString();
}

struct WindowSeen {
    HWND hwnd = nullptr;
    DWORD pid = 0;
    QString path;
    int area = 0;
    bool known = false;
};

struct Search {
    HWND skip = nullptr;
    WindowSeen best;
};

BOOL CALLBACK collectGame(HWND hwnd, LPARAM param)
{
    auto *search = reinterpret_cast<Search *>(param);
    if (hwnd == search->skip || !IsWindowVisible(hwnd) || IsIconic(hwnd) || cloaked(hwnd))
        return TRUE;
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect))
        return TRUE;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width < 320 || height < 240)
        return TRUE;
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return TRUE;
    const int monitorWidth = int(info.rcMonitor.right - info.rcMonitor.left);
    const int monitorHeight = int(info.rcMonitor.bottom - info.rcMonitor.top);
    const int monitorArea = std::max(1, monitorWidth * monitorHeight);
    const int area = width * height;
    if (area * 100 < monitorArea * 55)
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    const QString path = processPath(pid);
    const QString exe = QFileInfo(path).fileName();
    if (exe.isEmpty() || AppCatalog::isIgnored(exe) || AppCatalog::isLauncher(exe))
        return TRUE;
    AppFacts facts;
    facts.exeName = exe;
    if (AppCatalog::isRuntime(exe)) {
        wchar_t title[256];
        title[0] = 0;
        GetWindowTextW(hwnd, title, 256);
        facts.windowTitle = QString::fromWCharArray(title);
    }
    const bool known = AppCatalog::resolve(facts).recognized;
    const bool better = !search->best.hwnd || (known && !search->best.known) || (known == search->best.known && area > search->best.area);
    if (better) {
        search->best.hwnd = hwnd;
        search->best.pid = pid;
        search->best.path = path;
        search->best.area = area;
        search->best.known = known;
    }
    return TRUE;
}

struct OpenGame {
    DWORD pid = 0;
    QString name;
    QString stableId;
    QString path;
    int area = 0;
};

struct GameSearch {
    Settings *settings = nullptr;
    QHash<DWORD, OpenGame> games;
};

BOOL CALLBACK collectOpenGames(HWND hwnd, LPARAM param)
{
    auto *search = reinterpret_cast<GameSearch *>(param);
    if (!IsWindowVisible(hwnd) && !IsIconic(hwnd))
        return TRUE;
    if (cloaked(hwnd))
        return TRUE;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId())
        return TRUE;
    const QString path = processPath(pid);
    const QString exe = QFileInfo(path).fileName();
    if (exe.isEmpty() || AppCatalog::isIgnored(exe) || AppCatalog::isLauncher(exe))
        return TRUE;

    wchar_t title[256];
    title[0] = 0;
    GetWindowTextW(hwnd, title, 256);
    const VersionInfo version = readVersion(path);
    AppFacts facts;
    facts.exeName = exe;
    facts.exePath = path;
    facts.windowTitle = QString::fromWCharArray(title);
    facts.productName = version.product;
    facts.fileDescription = version.description;
    facts.companyName = version.company;
    facts.pid = pid;
    AppMatch match = AppCatalog::resolve(facts);
    if (!AppCatalog::isGame(match.stableId))
        return TRUE;
    if (search->settings) {
        const QString custom = search->settings->appDisplayName(match.stableId).trimmed();
        if (!custom.isEmpty())
            match.displayName = custom;
    }

    RECT rect{};
    int area = 0;
    if (!IsIconic(hwnd) && GetWindowRect(hwnd, &rect))
        area = std::max(0, int(rect.right - rect.left)) * std::max(0, int(rect.bottom - rect.top));
    OpenGame &slot = search->games[pid];
    if (slot.pid != 0 && slot.area >= area)
        return TRUE;
    slot.pid = pid;
    slot.name = match.displayName;
    slot.stableId = match.stableId;
    slot.path = path;
    slot.area = area;
    return TRUE;
}

HWND gameBehindLauncher(HWND launcher)
{
    Search search;
    search.skip = launcher;
    EnumWindows(collectGame, reinterpret_cast<LPARAM>(&search));
    return search.best.known ? search.best.hwnd : nullptr;
}

bool fullscreenOn(HWND hwnd)
{
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect))
        return false;
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info))
        return false;
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const int monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
    const int monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;
    return monitorWidth > 0 && monitorHeight > 0 && width >= int(monitorWidth * 0.9) && height >= int(monitorHeight * 0.9);
}

int monitorAt(HWND hwnd, DeviceCatalog *devices)
{
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect) || !devices)
        return 0;
    return devices->indexForPoint((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
}

} // namespace

GameDetector::GameDetector(DeviceCatalog *devices, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_devices(devices)
    , m_settings(settings)
{
}

QString GameDetector::name() const
{
    if (m_gameOpen && !m_gameName.isEmpty())
        return m_gameName;
    return m_name.isEmpty() ? QStringLiteral("Desktop") : m_name;
}

QString GameDetector::stableId() const
{
    if (m_gameOpen && !m_gameStableId.isEmpty())
        return m_gameStableId;
    return m_stableId;
}

QString GameDetector::icon() const
{
    if (m_gameOpen && !m_gameIcon.isEmpty())
        return m_gameIcon;
    return m_icon;
}

QVariantMap GameDetector::source() const
{
    QVariantMap map;
    map.insert(QStringLiteral("exe"), m_exe);
    map.insert(QStringLiteral("path"), m_exePath);
    map.insert(QStringLiteral("title"), m_windowTitle);
    map.insert(QStringLiteral("className"), m_windowClass);
    map.insert(QStringLiteral("product"), m_productName);
    map.insert(QStringLiteral("description"), m_fileDescription);
    map.insert(QStringLiteral("company"), m_companyName);
    map.insert(QStringLiteral("stableId"), m_stableId);
    return map;
}

void GameDetector::setCustomName(const QString &name)
{
    const QString id = stableId();
    if (!m_settings || id.isEmpty())
        return;
    m_settings->setAppDisplayName(id, name);
    poll();
}

void GameDetector::clearCustomName()
{
    setCustomName(QString());
}

bool GameDetector::updatePreferredGame(unsigned long foregroundPid)
{
    GameSearch search;
    search.settings = m_settings;
    EnumWindows(collectOpenGames, reinterpret_cast<LPARAM>(&search));

    const OpenGame *chosen = nullptr;
    const auto pick = [&](DWORD pid) -> const OpenGame * {
        const auto it = search.games.constFind(pid);
        return it == search.games.cend() ? nullptr : &it.value();
    };
    if (const OpenGame *front = pick(DWORD(foregroundPid)))
        chosen = front;
    else if (const OpenGame *held = pick(DWORD(m_gamePid)))
        chosen = held;
    else {
        int bestArea = -1;
        for (auto it = search.games.cbegin(); it != search.games.cend(); ++it) {
            if (!chosen || it.value().area > bestArea) {
                chosen = &it.value();
                bestArea = it.value().area;
            }
        }
    }

    const bool open = chosen != nullptr;
    const QString name = chosen ? chosen->name : QString();
    const QString stableId = chosen ? chosen->stableId : QString();
    const QString path = chosen ? chosen->path : QString();
    const unsigned long pid = chosen ? chosen->pid : 0;
    const QString icon = !open ? QString() : (path == m_gamePath ? m_gameIcon : cachedIcon(path));
    if (open == m_gameOpen && name == m_gameName && stableId == m_gameStableId && pid == m_gamePid && icon == m_gameIcon)
        return false;
    m_gameOpen = open;
    m_gameName = name;
    m_gameStableId = stableId;
    m_gamePath = path;
    m_gameIcon = icon;
    m_gamePid = pid;
    return true;
}

void GameDetector::poll()
{
    HWND hwnd = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (hwnd)
        GetWindowThreadProcessId(hwnd, &foregroundPid);
    if (!hwnd || foregroundPid == GetCurrentProcessId()) {
        if (updatePreferredGame(0))
            emit changed();
        return;
    }
    DWORD pid = foregroundPid;

    QString path = processPath(pid);
    QString exe = QFileInfo(path).fileName();
    if (AppCatalog::isLauncher(exe)) {
        if (HWND game = gameBehindLauncher(hwnd)) {
            hwnd = game;
            GetWindowThreadProcessId(hwnd, &pid);
            path = processPath(pid);
            exe = QFileInfo(path).fileName();
        }
    }
    static const char *kHelpers[] = {"easyanticheat.exe", "easyanticheat_eos.exe"};
    const QString helper = exe.toLower();
    for (const char *item : kHelpers) {
        if (helper == QLatin1String(item)) {
            const DWORD parent = parentPid(pid);
            const QString parentPath = processPath(parent);
            const QString parentExe = QFileInfo(parentPath).fileName();
            if (!parentExe.isEmpty() && !AppCatalog::isIgnored(parentExe)) {
                path = parentPath;
                exe = parentExe;
            }
            break;
        }
    }

    if (exe.isEmpty() || AppCatalog::isIgnored(exe)) {
        const bool shouldNotify = m_live || !m_name.isEmpty();
        m_live = false;
        m_fullscreen = false;
        m_name.clear();
        m_exe.clear();
        m_stableId.clear();
        m_icon.clear();
        m_windowTitle.clear();
        m_productName.clear();
        m_exePath.clear();
        m_fileDescription.clear();
        m_companyName.clear();
        m_windowClass.clear();
        if (shouldNotify || updatePreferredGame(0))
            emit changed();
        return;
    }

    wchar_t title[512];
    title[0] = 0;
    GetWindowTextW(hwnd, title, 512);
    wchar_t className[128];
    className[0] = 0;
    GetClassNameW(hwnd, className, 128);
    const VersionInfo version = readVersion(path);

    AppFacts facts;
    facts.exeName = exe;
    facts.exePath = path;
    facts.windowTitle = QString::fromWCharArray(title);
    facts.windowClass = QString::fromWCharArray(className);
    facts.fileDescription = version.description;
    facts.productName = version.product;
    facts.companyName = version.company;
    facts.pid = pid;
    AppMatch match = AppCatalog::resolve(facts);
    if (m_settings) {
        const QString custom = m_settings->appDisplayName(match.stableId).trimmed();
        if (!custom.isEmpty())
            match.displayName = custom;
    }
    const bool full = fullscreenOn(hwnd);
    const int monitor = monitorAt(hwnd, m_devices);
    const QString icon = path == m_exePath ? m_icon : cachedIcon(path);
    if (match.displayName == m_name && exe == m_exe && match.stableId == m_stableId && full == m_fullscreen && m_live
        && monitor == m_monitorIndex && icon == m_icon && facts.windowTitle == m_windowTitle) {
        if (updatePreferredGame(pid))
            emit changed();
        return;
    }
    const bool renamed = match.displayName != m_name || exe != m_exe;
    m_name = match.displayName;
    m_exe = exe;
    m_exePath = path;
    m_stableId = match.stableId;
    m_icon = icon;
    m_windowTitle = facts.windowTitle;
    m_windowClass = facts.windowClass;
    m_productName = facts.productName;
    m_fileDescription = facts.fileDescription;
    m_companyName = facts.companyName;
    m_fullscreen = full;
    m_live = true;
    m_monitorIndex = monitor;
    if (renamed)
        Log::info(QStringLiteral("Application %1 (%2)").arg(m_name, m_exe));
    updatePreferredGame(pid);
    emit changed();
}

} // namespace nuclip
