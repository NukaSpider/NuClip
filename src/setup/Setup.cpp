#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <tlhelp32.h>

#include "dist/FeedConfig.h"

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr UINT kStatusMessage = WM_APP + 1;
constexpr COLORREF kBg = RGB(8, 8, 8);
constexpr COLORREF kText = RGB(242, 242, 242);
constexpr COLORREF kSecondary = RGB(146, 146, 146);
constexpr COLORREF kMuted = RGB(98, 98, 98);
constexpr COLORREF kAccent = RGB(0x35, 0xD6, 0x9A);
constexpr COLORREF kOnAccent = RGB(7, 20, 15);
constexpr COLORREF kBorder = RGB(36, 36, 36);
constexpr COLORREF kDanger = RGB(240, 113, 120);
constexpr COLORREF kButtonIdle = RGB(20, 20, 20);

enum class Phase { Lookup, Ready, Busy, Done, Failed };

struct ReleaseChoice {
    std::wstring version;
    std::wstring notes;
    std::wstring package;
    std::wstring sha256;
};

struct State {
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    bool updateMode = false;
    bool uninstallMode = false;
    std::wstring feedUrl;
    std::wstring requestedVersion;
    bool versionFromArgs = false;
    std::vector<ReleaseChoice> releases;
    int selected = 0;
    bool versionOpen = false;
    bool versionHot = false;
    int versionScroll = 0;
    int versionHover = -1;
    RECT versionBox{};
    std::vector<RECT> versionRows;
    std::wstring version;
    std::wstring notes;
    std::wstring package;
    std::wstring sha256;
    std::wstring status = L"Looking up the current version.";
    std::wstring installedExe;
    int percent = 0;
    Phase phase = Phase::Lookup;
    std::mutex mu;
    std::atomic<bool> cancel{false};
    std::atomic<bool> running{false};
    std::thread worker;
    HFONT title = nullptr;
    HFONT body = nullptr;
    HFONT label = nullptr;
    HFONT button = nullptr;
    int dpi = 96;
    RECT primary{};
    RECT closeBtn{};
    bool primaryHot = false;
    bool closeHot = false;
    bool primaryDown = false;
    bool opened = false;
};

State g;

int px(int logical)
{
    return MulDiv(logical, g.dpi, 96);
}

std::wstring widen(const std::string &text)
{
    if (text.empty())
        return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), count);
    return out;
}

std::wstring feedDefault()
{
    return widen(NUCLIP_FEED_URL);
}

std::wstring swapToFallback(const std::wstring &url)
{
    const std::wstring primary = widen(NUCLIP_SITE_ORIGIN);
    const std::wstring fallback = widen(NUCLIP_FALLBACK_ORIGIN);
    const size_t pos = url.find(primary);
    if (pos == std::wstring::npos)
        return {};
    std::wstring next = url;
    next.replace(pos, primary.size(), fallback);
    return next;
}

void publish()
{
    if (g.hwnd)
        PostMessageW(g.hwnd, kStatusMessage, 0, 0);
}

void setProgress(const std::wstring &status, int percent)
{
    {
        std::lock_guard<std::mutex> lock(g.mu);
        g.status = status;
        g.percent = percent;
    }
    publish();
}

void setPhase(Phase phase, const std::wstring &status)
{
    {
        std::lock_guard<std::mutex> lock(g.mu);
        g.phase = phase;
        g.status = status;
    }
    publish();
}

std::wstring jsonString(const std::string &json, const char *key)
{
    const std::string token = std::string("\"") + key + "\"";
    size_t pos = json.find(token);
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos + token.size());
    if (pos == std::string::npos)
        return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};
    std::string value;
    for (size_t i = pos + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            const char next = json[++i];
            if (next == 'n')
                value.push_back('\n');
            else if (next != 'r')
                value.push_back(next);
            continue;
        }
        if (c == '"')
            break;
        value.push_back(c);
    }
    return widen(value);
}

bool crackUrl(const std::wstring &url, std::wstring &host, std::wstring &object, INTERNET_PORT &port, bool &secure, std::wstring &error)
{
    URL_COMPONENTS parts{};
    wchar_t hostBuf[256]{};
    wchar_t pathBuf[2048]{};
    wchar_t extraBuf[1024]{};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = hostBuf;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = pathBuf;
    parts.dwUrlPathLength = 2048;
    parts.lpszExtraInfo = extraBuf;
    parts.dwExtraInfoLength = 1024;
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts)) {
        error = L"The download address is not valid.";
        return false;
    }
    const bool local = _wcsicmp(hostBuf, L"localhost") == 0 || wcscmp(hostBuf, L"127.0.0.1") == 0;
    if (parts.nScheme == INTERNET_SCHEME_HTTPS)
        secure = true;
    else if (parts.nScheme == INTERNET_SCHEME_HTTP && local)
        secure = false;
    else {
        error = L"The download has to use HTTPS.";
        return false;
    }
    host = hostBuf;
    object = pathBuf;
    object += extraBuf;
    port = parts.nPort;
    return true;
}

struct HttpGet {
    std::wstring error;
    std::vector<unsigned char> body;
    fs::path file;
    bool toFile = false;
};

bool httpGet(const std::wstring &url, HttpGet &out, const bool progress)
{
    std::wstring host, object, error;
    INTERNET_PORT port = 0;
    bool secure = true;
    if (!crackUrl(url, host, object, port, secure, error)) {
        out.error = error;
        return false;
    }
    HINTERNET session = WinHttpOpen(L"NuClipSetup/0.1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        out.error = L"Could not open a connection.";
        return false;
    }
    WinHttpSetTimeouts(session, 8000, 8000, 20000, 20000);
    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    HINTERNET request = connect
        ? WinHttpOpenRequest(connect, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                             WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0)
        : nullptr;
    bool ok = request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr);
    if (ok) {
        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        ok = status >= 200 && status < 300;
        if (!ok)
            out.error = L"The server returned an error.";
    } else {
        out.error = L"Could not reach the download server.";
    }

    DWORD length = 0;
    if (ok) {
        DWORD size = sizeof(length);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &length, &size, WINHTTP_NO_HEADER_INDEX);
    }

    std::ofstream file;
    if (ok && out.toFile) {
        file.open(out.file, std::ios::binary | std::ios::trunc);
        ok = file.good();
        if (!ok)
            out.error = L"Could not save the download.";
    }

    DWORD readTotal = 0;
    while (ok) {
        if (g.cancel) {
            ok = false;
            out.error = L"Cancelled.";
            break;
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            ok = false;
            out.error = L"The download stopped.";
            break;
        }
        if (available == 0)
            break;
        std::vector<unsigned char> chunk(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            ok = false;
            out.error = L"The download stopped.";
            break;
        }
        if (out.toFile)
            file.write(reinterpret_cast<const char *>(chunk.data()), read);
        else
            out.body.insert(out.body.end(), chunk.begin(), chunk.begin() + read);
        readTotal += read;
        if (progress) {
            int percent = length > 0 ? static_cast<int>((static_cast<unsigned long long>(readTotal) * 100) / length) : 0;
            if (percent > 99)
                percent = 99;
            setProgress(L"Downloading", percent);
        }
        if (out.body.size() > 2 * 1024 * 1024 && !out.toFile) {
            ok = false;
            out.error = L"The release file was larger than expected.";
        }
    }

    if (request)
        WinHttpCloseHandle(request);
    if (connect)
        WinHttpCloseHandle(connect);
    if (session)
        WinHttpCloseHandle(session);
    return ok;
}

bool sha256Of(const fs::path &path, std::wstring &hex, std::wstring &error)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        error = L"Could not check the download.";
        return false;
    }
    DWORD objectSize = 0;
    DWORD ignored = 0;
    BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &ignored, 0);
    std::vector<unsigned char> object(objectSize);
    std::vector<unsigned char> digest(32);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        error = L"Could not check the download.";
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    std::vector<char> buffer(1024 * 256);
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = file.gcount();
        if (count > 0)
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(count), 0);
    }
    const bool finished = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) == 0;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!finished) {
        error = L"Could not check the download.";
        return false;
    }
    static const wchar_t *digits = L"0123456789abcdef";
    hex.clear();
    hex.reserve(64);
    for (unsigned char byte : digest) {
        hex.push_back(digits[byte >> 4]);
        hex.push_back(digits[byte & 0x0f]);
    }
    return true;
}

bool sameHash(std::wstring left, std::wstring right)
{
    for (wchar_t &c : left)
        c = static_cast<wchar_t>(towlower(c));
    for (wchar_t &c : right)
        c = static_cast<wchar_t>(towlower(c));
    return left == right;
}

bool extractZip(const fs::path &zip, const fs::path &dest, std::wstring &error)
{
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const std::wstring tar = std::wstring(system) + L"\\tar.exe";
    if (!fs::exists(tar)) {
        error = L"Windows tar was not found, so the download could not be unpacked.";
        return false;
    }
    const std::wstring command = L"\"" + tar + L"\" -xf \"" + zip.wstring() + L"\" -C \"" + dest.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        error = L"The download could not be unpacked.";
        return false;
    }
    WaitForSingleObject(process.hProcess, 120000);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (code != 0) {
        error = L"The download could not be unpacked.";
        return false;
    }
    return true;
}

fs::path findPayload(const fs::path &root)
{
    if (fs::exists(root / L"NuClip.exe"))
        return root;
    std::error_code ec;
    for (const fs::directory_entry &entry : fs::directory_iterator(root, ec)) {
        if (entry.is_directory() && fs::exists(entry.path() / L"NuClip.exe"))
            return entry.path();
    }
    return {};
}

bool fileWritable(const fs::path &file)
{
    if (!fs::exists(file))
        return true;
    HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    CloseHandle(handle);
    return true;
}

std::wstring processImage(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t buffer[MAX_PATH * 2]{};
    DWORD size = MAX_PATH * 2;
    std::wstring image;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size))
        image.assign(buffer, size);
    CloseHandle(process);
    return image;
}

bool pathInDirectory(const std::wstring &image, const fs::path &directory)
{
    std::wstring root = fs::absolute(directory).wstring();
    while (!root.empty() && (root.back() == L'\\' || root.back() == L'/'))
        root.pop_back();
    if (image.size() < root.size() || _wcsnicmp(image.c_str(), root.c_str(), static_cast<int>(root.size())) != 0)
        return false;
    return image.size() == root.size() || image[root.size()] == L'\\' || image[root.size()] == L'/';
}

std::vector<DWORD> processesIn(const fs::path &directory)
{
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return pids;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == GetCurrentProcessId())
                continue;
            const std::wstring image = processImage(entry.th32ProcessID);
            if (!image.empty() && pathInDirectory(image, directory))
                pids.push_back(entry.th32ProcessID);
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return pids;
}

void signalQuit()
{
    HANDLE pipe = CreateFileW(L"\\\\.\\pipe\\NuClipInstance", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
        return;
    const char message[] = "quit";
    DWORD written = 0;
    WriteFile(pipe, message, 4, &written, nullptr);
    CloseHandle(pipe);
}

void terminatePids(const std::vector<DWORD> &pids)
{
    for (DWORD pid : pids) {
        HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!process)
            continue;
        TerminateProcess(process, 1);
        WaitForSingleObject(process, 4000);
        CloseHandle(process);
    }
}

bool releaseInstalledApp(const fs::path &exe)
{
    const fs::path app = exe.parent_path();
    if (!fs::exists(exe))
        return true;
    setProgress(L"Closing NuClip", 100);
    signalQuit();
    for (int i = 0; i < 60; ++i) {
        if (g.cancel)
            return false;
        const std::vector<DWORD> pids = processesIn(app);
        if (pids.empty() && fileWritable(exe))
            return true;
        if (i == 12)
            terminatePids(pids);
        Sleep(250);
    }
    terminatePids(processesIn(app));
    for (int i = 0; i < 20; ++i) {
        if (g.cancel)
            return false;
        if (processesIn(app).empty() && fileWritable(exe))
            return true;
        Sleep(250);
    }
    return false;
}

void createShortcut(const fs::path &linkPath, const fs::path &target)
{
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))))
        return;
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(target.parent_path().c_str());
    link->SetDescription(L"NuClip");
    link->SetIconLocation(target.c_str(), 0);
    IPersistFile *file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
        fs::create_directories(linkPath.parent_path());
        file->Save(linkPath.c_str(), TRUE);
        file->Release();
    }
    link->Release();
}

void installShortcuts(const fs::path &exe)
{
    PWSTR programs = nullptr;
    PWSTR desktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &programs))) {
        createShortcut(fs::path(programs) / L"NuClip.lnk", exe);
        CoTaskMemFree(programs);
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop))) {
        createShortcut(fs::path(desktop) / L"NuClip.lnk", exe);
        CoTaskMemFree(desktop);
    }
}

fs::path localRoot();

void removeShortcut(const fs::path &linkPath)
{
    std::error_code ec;
    if (!fs::exists(linkPath))
        return;
    fs::remove(linkPath, ec);
    SHChangeNotify(SHCNE_DELETE, SHCNF_PATHW, linkPath.c_str(), nullptr);
}

void removeShortcuts()
{
    PWSTR programs = nullptr;
    PWSTR desktop = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &programs))) {
        removeShortcut(fs::path(programs) / L"NuClip.lnk");
        CoTaskMemFree(programs);
    }
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop))) {
        removeShortcut(fs::path(desktop) / L"NuClip.lnk");
        CoTaskMemFree(desktop);
    }
}

void setRegistryString(HKEY key, const wchar_t *name, const std::wstring &value)
{
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

DWORD folderSizeKb(const fs::path &root)
{
    std::error_code ec;
    unsigned long long bytes = 0;
    if (!fs::exists(root))
        return 0;
    for (const fs::directory_entry &entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (entry.is_regular_file(ec))
            bytes += entry.file_size(ec);
    }
    const unsigned long long kb = bytes / 1024;
    return kb > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : static_cast<DWORD>(kb);
}

void registerUninstall(const fs::path &exe, const fs::path &setup, const std::wstring &version)
{
    HKEY key = nullptr;
    const wchar_t *path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NuClip";
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;
    setRegistryString(key, L"DisplayName", L"NuClip");
    setRegistryString(key, L"DisplayVersion", version.empty() ? L"0.1.8" : version);
    setRegistryString(key, L"Publisher", L"NuClip");
    setRegistryString(key, L"InstallLocation", setup.parent_path().wstring());
    setRegistryString(key, L"DisplayIcon", exe.wstring());
    const std::wstring command = L"\"" + setup.wstring() + L"\" --uninstall";
    setRegistryString(key, L"UninstallString", command);
    setRegistryString(key, L"QuietUninstallString", command);
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t date[16]{};
    swprintf_s(date, L"%04u%02u%02u", now.wYear, now.wMonth, now.wDay);
    setRegistryString(key, L"InstallDate", date);
    const DWORD language = 1033;
    RegSetValueExW(key, L"Language", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&language), sizeof(language));
    const DWORD yes = 1;
    RegSetValueExW(key, L"NoModify", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&yes), sizeof(yes));
    RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&yes), sizeof(yes));
    const DWORD sizeKb = folderSizeKb(exe.parent_path());
    if (sizeKb > 0)
        RegSetValueExW(key, L"EstimatedSize", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&sizeKb), sizeof(sizeKb));
    RegCloseKey(key);
}

void removeStartupEntry()
{
    HKEY run = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &run) != ERROR_SUCCESS)
        return;
    RegDeleteValueW(run, L"NuClip");
    RegCloseKey(run);
}

bool uninstallPackage(std::wstring &error)
{
    const fs::path root = localRoot();
    const fs::path exe = root / L"app" / L"NuClip.exe";
    if (!releaseInstalledApp(exe)) {
        error = g.cancel ? L"Cancelled." : L"NuClip is still running, so it was not removed.";
        return false;
    }
    setProgress(L"Removing NuClip", 50);
    removeShortcuts();
    removeStartupEntry();
    std::error_code ec;
    fs::remove_all(root / L"app", ec);
    fs::remove_all(root / L"staging", ec);
    fs::remove_all(root / L"app.previous", ec);
    fs::remove_all(root / L"logs", ec);
    fs::remove_all(root / L"buffer", ec);
    fs::remove_all(root / L"thumbnails", ec);
    fs::remove_all(root / L"clipmeta", ec);
    fs::remove(root / L"settings.json", ec);
    fs::remove(root / L"NuClipSetup.exe", ec);
    fs::remove(root / L"NuClipSetup.exe.old", ec);
    if (fs::exists(exe)) {
        error = L"Some NuClip files are still in use.";
        return false;
    }
    fs::remove_all(root, ec);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\NuClip");
    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    MoveFileExW(self, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    return true;
}

fs::path localRoot()
{
    PWSTR local = nullptr;
    fs::path root;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
        root = fs::path(local) / L"NuClip";
        CoTaskMemFree(local);
    }
    fs::create_directories(root);
    return root;
}

ReleaseChoice releaseFromObject(const std::string &json)
{
    ReleaseChoice release;
    release.version = jsonString(json, "version");
    release.package = jsonString(json, "package");
    release.notes = jsonString(json, "notes");
    release.sha256 = jsonString(json, "sha256");
    if (release.notes.size() > 180)
        release.notes.resize(180);
    return release;
}

std::vector<ReleaseChoice> releasesFromFeed(const std::string &json)
{
    std::vector<ReleaseChoice> list;
    const size_t key = json.find("\"versions\"");
    size_t cursor = key == std::string::npos ? std::string::npos : json.find('[', key);
    if (cursor != std::string::npos) {
        int depth = 0;
        bool inString = false;
        size_t objectStart = std::string::npos;
        for (size_t i = cursor + 1; i < json.size(); ++i) {
            const char c = json[i];
            if (inString) {
                if (c == '\\' && i + 1 < json.size()) {
                    ++i;
                    continue;
                }
                if (c == '"')
                    inString = false;
                continue;
            }
            if (c == '"') {
                inString = true;
                continue;
            }
            if (c == '{') {
                if (depth == 0)
                    objectStart = i;
                ++depth;
                continue;
            }
            if (c == '}' && depth > 0) {
                --depth;
                if (depth == 0 && objectStart != std::string::npos) {
                    const ReleaseChoice release = releaseFromObject(json.substr(objectStart, i - objectStart + 1));
                    if (!release.version.empty() && !release.package.empty())
                        list.push_back(release);
                    objectStart = std::string::npos;
                }
                continue;
            }
            if (c == ']' && depth == 0)
                break;
        }
    }
    if (list.empty()) {
        const ReleaseChoice release = releaseFromObject(json);
        if (!release.version.empty() && !release.package.empty())
            list.push_back(release);
    }
    return list;
}

bool sameVersion(const std::wstring &left, const std::wstring &right)
{
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

void applyRelease(int index)
{
    if (index < 0 || index >= static_cast<int>(g.releases.size()))
        return;
    g.selected = index;
    const ReleaseChoice &release = g.releases[static_cast<size_t>(index)];
    g.version = release.version;
    g.notes = release.notes;
    g.package = release.package;
    g.sha256 = release.sha256;
    g.requestedVersion = release.version;
}

bool fetchFeed(std::wstring &error)
{
    HttpGet response;
    std::wstring used = g.feedUrl;
    if (!httpGet(used, response, false)) {
        const std::wstring alt = g.cancel ? std::wstring() : swapToFallback(used);
        if (alt.empty()) {
            error = response.error.empty() ? L"Could not read the release list." : response.error;
            return false;
        }
        response = {};
        used = alt;
        if (!httpGet(used, response, false)) {
            error = response.error.empty() ? L"Could not read the release list." : response.error;
            return false;
        }
    }
    const std::string json(response.body.begin(), response.body.end());
    std::vector<ReleaseChoice> releases = releasesFromFeed(json);
    if (releases.empty()) {
        error = L"The release list is missing a version.";
        return false;
    }
    if (used.rfind(widen(NUCLIP_FALLBACK_ORIGIN), 0) == 0) {
        for (ReleaseChoice &release : releases) {
            const std::wstring moved = swapToFallback(release.package);
            if (!moved.empty())
                release.package = moved;
        }
    }
    std::lock_guard<std::mutex> lock(g.mu);
    int index = 0;
    bool found = g.requestedVersion.empty();
    if (!g.requestedVersion.empty()) {
        for (int i = 0; i < static_cast<int>(releases.size()); ++i) {
            if (sameVersion(releases[static_cast<size_t>(i)].version, g.requestedVersion)) {
                index = i;
                found = true;
                break;
            }
        }
    }
    if (g.versionFromArgs && !found) {
        error = L"That version is not in the release list.";
        return false;
    }
    g.releases = std::move(releases);
    applyRelease(index);
    return true;
}

bool installPackage(std::wstring &error)
{
    std::wstring package;
    std::wstring version;
    std::wstring sha;
    {
        std::lock_guard<std::mutex> lock(g.mu);
        package = g.package;
        version = g.version;
        sha = g.sha256;
    }
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    const fs::path zip = fs::path(temp) / (L"NuClip-" + version + L".zip");
    HttpGet download;
    download.toFile = true;
    download.file = zip;
    setProgress(L"Downloading", 0);
    if (!httpGet(package, download, true)) {
        const std::wstring alt = g.cancel ? std::wstring() : swapToFallback(package);
        std::error_code ec;
        fs::remove(zip, ec);
        if (alt.empty()) {
            error = download.error.empty() ? L"The download did not finish." : download.error;
            return false;
        }
        HttpGet retry;
        retry.toFile = true;
        retry.file = zip;
        setProgress(L"Downloading", 0);
        if (!httpGet(alt, retry, true)) {
            fs::remove(zip, ec);
            error = retry.error.empty() ? L"The download did not finish." : retry.error;
            return false;
        }
    }
    if (!sha.empty()) {
        setProgress(L"Checking the download", 100);
        std::wstring actual;
        if (!sha256Of(zip, actual, error) || !sameHash(actual, sha)) {
            std::error_code ec;
            fs::remove(zip, ec);
            if (error.empty())
                error = L"The download did not match its checksum.";
            return false;
        }
    }

    const fs::path root = localRoot();
    const fs::path staging = root / L"staging";
    const fs::path app = root / L"app";
    const fs::path previous = root / L"app.previous";
    std::error_code ec;
    fs::remove_all(staging, ec);
    fs::create_directories(staging);
    setProgress(L"Installing", 100);
    if (!extractZip(zip, staging, error)) {
        fs::remove(zip, ec);
        return false;
    }
    fs::remove(zip, ec);
    const fs::path payload = findPayload(staging);
    if (payload.empty()) {
        error = L"The download did not contain NuClip.";
        return false;
    }
    const fs::path exe = app / L"NuClip.exe";
    if (!releaseInstalledApp(exe)) {
        error = g.cancel ? L"Cancelled." : L"NuClip is still running, so the update stopped.";
        return false;
    }
    fs::remove_all(previous, ec);
    if (fs::exists(app)) {
        try {
            fs::rename(app, previous);
        } catch (...) {
            error = L"Close NuClip, then try again.";
            return false;
        }
    }
    try {
        if (payload == staging)
            fs::rename(staging, app);
        else {
            fs::rename(payload, app);
            fs::remove_all(staging, ec);
        }
    } catch (...) {
        if (fs::exists(previous) && !fs::exists(app))
            fs::rename(previous, app);
        error = L"The new files could not be moved into place.";
        return false;
    }
    fs::remove_all(previous, ec);

    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const fs::path setupDest = root / L"NuClipSetup.exe";
    const fs::path bundled = app / L"NuClipSetup.exe";
    std::error_code copyError;
    fs::path setupSource = self;
    if (fs::exists(bundled)) {
        std::error_code timeError;
        const auto bundledTime = fs::last_write_time(bundled, timeError);
        const auto selfTime = fs::last_write_time(self, timeError);
        if (!timeError && bundledTime > selfTime)
            setupSource = bundled;
    }
    if (fs::absolute(setupSource, copyError) != fs::absolute(setupDest, copyError))
        fs::copy_file(setupSource, setupDest, fs::copy_options::overwrite_existing, copyError);
    installShortcuts(app / L"NuClip.exe");
    registerUninstall(app / L"NuClip.exe", setupDest, version);
    {
        std::lock_guard<std::mutex> lock(g.mu);
        g.installedExe = (app / L"NuClip.exe").wstring();
    }
    return true;
}

void runUninstall()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::wstring error;
    const bool ok = g.cancel ? false : uninstallPackage(error);
    if (g.cancel)
        setPhase(Phase::Failed, L"Cancelled.");
    else if (!ok)
        setPhase(Phase::Failed, error.empty() ? L"NuClip could not be removed." : error);
    else
        setPhase(Phase::Done, L"NuClip has been removed.");
    CoUninitialize();
}

void startUninstall()
{
    if (g.running)
        return;
    if (g.worker.joinable())
        g.worker.join();
    g.cancel = false;
    setPhase(Phase::Busy, L"Closing NuClip");
    g.running = true;
    g.worker = std::thread([] {
        runUninstall();
        g.running = false;
    });
}

void runJob(bool install)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::wstring error;
    bool ok = fetchFeed(error);
    if (ok && install && !g.cancel)
        ok = installPackage(error);
    if (g.cancel) {
        setPhase(Phase::Failed, L"Cancelled.");
    } else if (!ok) {
        setPhase(Phase::Failed, error.empty() ? L"Something went wrong." : error);
    } else if (install) {
        setPhase(Phase::Done, L"NuClip is installed.");
    } else {
        std::wstring version;
        {
            std::lock_guard<std::mutex> lock(g.mu);
            version = g.version;
        }
        setPhase(Phase::Ready, L"Ready to install.");
    }
    CoUninitialize();
}

void startJob(bool install)
{
    if (g.running)
        return;
    if (g.worker.joinable())
        g.worker.join();
    g.cancel = false;
    g.versionOpen = false;
    if (install)
        setPhase(Phase::Busy, L"Downloading");
    else
        setPhase(Phase::Lookup, L"Looking up the current version.");
    g.running = true;
    g.worker = std::thread([install] {
        runJob(install);
        g.running = false;
    });
}

void joinWorker()
{
    g.cancel = true;
    if (g.worker.joinable())
        g.worker.join();
}

HFONT makeFont(int size, int weight)
{
    return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Variable Text");
}

void rebuildFonts()
{
    if (g.title)
        DeleteObject(g.title);
    if (g.body)
        DeleteObject(g.body);
    if (g.label)
        DeleteObject(g.label);
    if (g.button)
        DeleteObject(g.button);
    g.title = makeFont(px(32), FW_LIGHT);
    g.body = makeFont(px(15), FW_NORMAL);
    g.label = makeFont(px(12), FW_NORMAL);
    g.button = makeFont(px(13), FW_SEMIBOLD);
}

void drawText(HDC hdc, HFONT font, COLORREF color, RECT rect, const std::wstring &value, UINT format)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    SelectObject(hdc, font);
    DrawTextW(hdc, value.c_str(), -1, &rect, format);
}

void paint(HWND hwnd)
{
    PAINTSTRUCT paintStruct{};
    HDC screen = BeginPaint(hwnd, &paintStruct);
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right;
    const int height = client.bottom;
    HDC hdc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    HBITMAP previous = static_cast<HBITMAP>(SelectObject(hdc, bitmap));

    HBRUSH background = CreateSolidBrush(kBg);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    HPEN borderPen = CreatePen(PS_SOLID, 1, kBorder);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
    Rectangle(hdc, 0, 0, width, height);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    Phase phase;
    std::wstring status;
    std::wstring notes;
    std::wstring version;
    int percent = 0;
    {
        std::lock_guard<std::mutex> lock(g.mu);
        phase = g.phase;
        status = g.status;
        notes = g.notes;
        version = g.version;
        percent = g.percent;
    }

    const int margin = px(28);
    const int mark = px(16);
    RECT markRect{margin, px(16), margin + mark, px(16) + mark};
    HPEN tick = CreatePen(PS_SOLID, 1, kText);
    HPEN previousPen = static_cast<HPEN>(SelectObject(hdc, tick));
    const int arm = px(5);
    auto corner = [&](int x, int y, int dx, int dy) {
        MoveToEx(hdc, x, y + dy, nullptr);
        LineTo(hdc, x, y);
        LineTo(hdc, x + dx, y);
    };
    corner(markRect.left, markRect.top, arm, arm);
    corner(markRect.right, markRect.top, -arm, arm);
    corner(markRect.left, markRect.bottom, arm, -arm);
    corner(markRect.right, markRect.bottom, -arm, -arm);
    SelectObject(hdc, previousPen);
    DeleteObject(tick);

    RECT word{markRect.right + px(8), px(12), width - px(48), px(34)};
    drawText(hdc, g.label, kText, word, L"NUCLIP", DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    g.closeBtn = RECT{width - px(40), 0, width, px(40)};
    if (g.closeHot) {
        HBRUSH hot = CreateSolidBrush(RGB(28, 28, 28));
        FillRect(hdc, &g.closeBtn, hot);
        DeleteObject(hot);
    }
    RECT closeLabel = g.closeBtn;
    drawText(hdc, g.body, g.closeHot ? kText : kMuted, closeLabel, L"\u00D7", DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT kicker{margin, px(78), width - margin, px(96)};
    SetTextCharacterExtra(hdc, px(2));
    const wchar_t *kickerText = L"INSTALL";
    if (g.uninstallMode)
        kickerText = L"UNINSTALL";
    else if (g.updateMode)
        kickerText = L"UPDATE";
    drawText(hdc, g.label, kMuted, kicker, kickerText, DT_LEFT | DT_SINGLELINE);
    SetTextCharacterExtra(hdc, 0);

    const wchar_t *headline = L"NuClip";
    if (phase == Phase::Done)
        headline = g.uninstallMode ? L"Removed" : L"Ready";
    else if (phase == Phase::Failed)
        headline = L"Stopped";
    RECT title{margin, px(104), width - margin, px(150)};
    drawText(hdc, g.title, kText, title, headline, DT_LEFT | DT_SINGLELINE);

    const wchar_t *body = L"Choose a version. Latest is selected. Clips and settings stay where they are.";
    if (g.uninstallMode)
        body = L"Removes NuClip from this Windows account. Clips in your Videos folder stay.";
    else if (g.updateMode)
        body = L"The installed copy will be replaced. Clips and settings stay where they are.";
    RECT bodyRect{margin, px(162), width - margin, px(228)};
    drawText(hdc, g.body, kSecondary, bodyRect, body, DT_LEFT | DT_WORDBREAK);

    std::vector<ReleaseChoice> releases;
    int selected = 0;
    {
        std::lock_guard<std::mutex> lock(g.mu);
        releases = g.releases;
        selected = g.selected;
    }
    const bool canPick = !g.uninstallMode && releases.size() > 1 && (phase == Phase::Ready || phase == Phase::Failed);
    g.versionBox = RECT{margin, px(236), width - margin, px(272)};
    if (!g.uninstallMode) {
        HBRUSH boxBrush = CreateSolidBrush(canPick && g.versionHot ? RGB(28, 28, 28) : kButtonIdle);
        FillRect(hdc, &g.versionBox, boxBrush);
        DeleteObject(boxBrush);
        HPEN boxPen = CreatePen(PS_SOLID, 1, canPick && (g.versionHot || g.versionOpen) ? kAccent : kBorder);
        HPEN previousBoxPen = static_cast<HPEN>(SelectObject(hdc, boxPen));
        HBRUSH previousBoxBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
        Rectangle(hdc, g.versionBox.left, g.versionBox.top, g.versionBox.right, g.versionBox.bottom);
        SelectObject(hdc, previousBoxBrush);
        SelectObject(hdc, previousBoxPen);
        DeleteObject(boxPen);
        std::wstring shown = L"Looking up versions";
        if (!version.empty()) {
            shown = version;
            if (selected == 0)
                shown += L"    latest";
        }
        RECT versionText{g.versionBox.left + px(12), g.versionBox.top, g.versionBox.right - px(28), g.versionBox.bottom};
        drawText(hdc, g.body, version.empty() ? kMuted : kText, versionText, shown, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (canPick) {
            const int cx = g.versionBox.right - px(16);
            const int cy = (g.versionBox.top + g.versionBox.bottom) / 2;
            HPEN chevron = CreatePen(PS_SOLID, px(1) < 1 ? 1 : px(1), kMuted);
            HPEN previousChevron = static_cast<HPEN>(SelectObject(hdc, chevron));
            MoveToEx(hdc, cx - px(4), cy - px(2), nullptr);
            LineTo(hdc, cx, cy + px(2));
            LineTo(hdc, cx + px(4), cy - px(2));
            SelectObject(hdc, previousChevron);
            DeleteObject(chevron);
        }
    }
    g.versionRows.clear();
    if (!notes.empty() && phase != Phase::Failed && !g.versionOpen) {
        RECT notesRect{margin, px(284), width - margin, px(360)};
        drawText(hdc, g.label, kMuted, notesRect, notes.c_str(), DT_LEFT | DT_WORDBREAK);
    }
    if (g.versionOpen && canPick) {
        const int rowHeight = px(32);
        const int listTop = g.versionBox.bottom + px(4);
        const int listLimit = height - px(148);
        int visible = (listLimit - listTop) / rowHeight;
        if (visible < 1)
            visible = 1;
        if (visible > static_cast<int>(releases.size()))
            visible = static_cast<int>(releases.size());
        const int maxScroll = static_cast<int>(releases.size()) - visible;
        if (g.versionScroll > maxScroll)
            g.versionScroll = maxScroll;
        if (g.versionScroll < 0)
            g.versionScroll = 0;
        RECT list{margin, listTop, width - margin, listTop + visible * rowHeight};
        HBRUSH listBrush = CreateSolidBrush(RGB(16, 16, 16));
        FillRect(hdc, &list, listBrush);
        DeleteObject(listBrush);
        HPEN listPen = CreatePen(PS_SOLID, 1, kBorder);
        HPEN previousListPen = static_cast<HPEN>(SelectObject(hdc, listPen));
        HBRUSH previousListBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)));
        Rectangle(hdc, list.left, list.top, list.right, list.bottom);
        SelectObject(hdc, previousListBrush);
        SelectObject(hdc, previousListPen);
        DeleteObject(listPen);
        for (int row = 0; row < visible; ++row) {
            const int index = g.versionScroll + row;
            RECT item{list.left + 1, list.top + row * rowHeight, list.right - 1, list.top + (row + 1) * rowHeight};
            g.versionRows.push_back(item);
            if (index == g.versionHover) {
                HBRUSH hover = CreateSolidBrush(RGB(28, 28, 28));
                FillRect(hdc, &item, hover);
                DeleteObject(hover);
            }
            std::wstring label = releases[static_cast<size_t>(index)].version;
            if (index == 0)
                label += L"    latest";
            RECT itemText{item.left + px(12), item.top, item.right - px(12), item.bottom};
            drawText(hdc, g.body, index == selected ? kAccent : kText, itemText, label, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    RECT track{margin, height - px(132), width - margin, height - px(128)};
    HBRUSH trackBrush = CreateSolidBrush(kBorder);
    FillRect(hdc, &track, trackBrush);
    DeleteObject(trackBrush);
    if (phase == Phase::Busy || phase == Phase::Done) {
        const int span = track.right - track.left;
        const int fill = phase == Phase::Done ? span : span * std::max(0, std::min(100, percent)) / 100;
        RECT bar = track;
        bar.right = bar.left + fill;
        HBRUSH accent = CreateSolidBrush(kAccent);
        FillRect(hdc, &bar, accent);
        DeleteObject(accent);
    }

    RECT statusRect{margin, height - px(118), width - margin, height - px(78)};
    const COLORREF statusColor = phase == Phase::Failed ? kDanger : kSecondary;
    drawText(hdc, g.label, statusColor, statusRect, status.c_str(), DT_LEFT | DT_WORDBREAK);

    const wchar_t *label = L"Install";
    bool enabled = false;
    if (g.uninstallMode)
        label = L"Uninstall";
    else if (g.updateMode && phase == Phase::Ready)
        label = L"Update";
    if (phase == Phase::Ready)
        enabled = true;
    else if (phase == Phase::Failed) {
        label = L"Try again";
        enabled = true;
    } else if (phase == Phase::Done) {
        label = g.uninstallMode ? L"Close" : L"Open NuClip";
        enabled = true;
    } else if (phase == Phase::Busy) {
        label = L"Working";
    }
    const int buttonWidth = px(148);
    g.primary = RECT{margin, height - px(64), margin + buttonWidth, height - px(28)};
    const bool hot = enabled && g.primaryHot;
    HBRUSH buttonBrush = CreateSolidBrush(enabled ? (hot ? RGB(61, 228, 166) : kAccent) : kButtonIdle);
    FillRect(hdc, &g.primary, buttonBrush);
    DeleteObject(buttonBrush);
    drawText(hdc, g.button, enabled ? kOnAccent : kMuted, g.primary, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BitBlt(screen, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, previous);
    DeleteObject(bitmap);
    DeleteDC(hdc);
    EndPaint(hwnd, &paintStruct);
}

bool hit(const RECT &rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

void primaryAction()
{
    Phase phase;
    bool hasPackage = false;
    {
        std::lock_guard<std::mutex> lock(g.mu);
        phase = g.phase;
        hasPackage = !g.package.empty();
    }
    if (g.uninstallMode) {
        if (phase == Phase::Ready || phase == Phase::Failed)
            startUninstall();
        else if (phase == Phase::Done)
            DestroyWindow(g.hwnd);
        return;
    }
    if (phase == Phase::Ready)
        startJob(true);
    else if (phase == Phase::Failed)
        startJob(hasPackage);
    else if (phase == Phase::Done) {
        std::wstring exe;
        {
            std::lock_guard<std::mutex> lock(g.mu);
            exe = g.installedExe;
        }
        if (!exe.empty())
            ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, fs::path(exe).parent_path().c_str(), SW_SHOWNORMAL);
        DestroyWindow(g.hwnd);
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        g.hwnd = hwnd;
        g.dpi = static_cast<int>(GetDpiForWindow(hwnd));
        rebuildFonts();
        if (g.uninstallMode)
            setPhase(Phase::Ready, L"");
        else if (g.updateMode)
            startJob(true);
        else
            startJob(false);
        return 0;
    case WM_DPICHANGED: {
        g.dpi = HIWORD(wparam);
        rebuildFonts();
        const RECT *suggested = reinterpret_cast<const RECT *>(lparam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case kStatusMessage:
        InvalidateRect(hwnd, nullptr, FALSE);
        if (g.updateMode && !g.opened) {
            Phase phase;
            std::wstring exe;
            {
                std::lock_guard<std::mutex> lock(g.mu);
                phase = g.phase;
                exe = g.installedExe;
            }
            if (phase == Phase::Done && !exe.empty()) {
                g.opened = true;
                ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, fs::path(exe).parent_path().c_str(), SW_SHOWNORMAL);
                DestroyWindow(hwnd);
            }
        }
        return 0;
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);
        const bool primary = hit(g.primary, x, y);
        const bool close = hit(g.closeBtn, x, y);
        const bool version = hit(g.versionBox, x, y);
        int hover = -1;
        if (g.versionOpen) {
            for (int i = 0; i < static_cast<int>(g.versionRows.size()); ++i) {
                if (hit(g.versionRows[static_cast<size_t>(i)], x, y))
                    hover = g.versionScroll + i;
            }
        }
        if (primary != g.primaryHot || close != g.closeHot || version != g.versionHot || hover != g.versionHover) {
            g.primaryHot = primary;
            g.closeHot = close;
            g.versionHot = version;
            g.versionHover = hover;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&track);
        return 0;
    }
    case WM_MOUSELEAVE:
        g.primaryHot = false;
        g.closeHot = false;
        g.versionHot = false;
        g.versionHover = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        if (g.versionOpen) {
            const int steps = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
            g.versionScroll -= steps;
            if (g.versionScroll < 0)
                g.versionScroll = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);
        if (hit(g.closeBtn, x, y))
            DestroyWindow(hwnd);
        else if (hit(g.primary, x, y)) {
            g.versionOpen = false;
            primaryAction();
        } else if (g.versionOpen) {
            bool picked = false;
            for (int i = 0; i < static_cast<int>(g.versionRows.size()); ++i) {
                if (!hit(g.versionRows[static_cast<size_t>(i)], x, y))
                    continue;
                {
                    std::lock_guard<std::mutex> lock(g.mu);
                    applyRelease(g.versionScroll + i);
                }
                picked = true;
                break;
            }
            g.versionOpen = false;
            if (!picked && hit(g.versionBox, x, y))
                g.versionOpen = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (hit(g.versionBox, x, y)) {
            Phase phase;
            bool multiple = false;
            {
                std::lock_guard<std::mutex> lock(g.mu);
                phase = g.phase;
                multiple = g.releases.size() > 1;
            }
            if (!g.uninstallMode && multiple && (phase == Phase::Ready || phase == Phase::Failed)) {
                g.versionOpen = true;
                g.versionScroll = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wparam == VK_RETURN) {
            g.versionOpen = false;
            primaryAction();
        } else if (wparam == VK_ESCAPE) {
            if (g.versionOpen) {
                g.versionOpen = false;
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                DestroyWindow(hwnd);
            }
        }
        return 0;
    case WM_NCHITTEST: {
        POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(hwnd, &point);
        if (hit(g.closeBtn, point.x, point.y) || hit(g.primary, point.x, point.y))
            return HTCLIENT;
        if (point.y < px(48))
            return HTCAPTION;
        return HTCLIENT;
    }
    case WM_DESTROY:
        joinWorker();
        g.hwnd = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g.instance = instance;
    g.feedUrl = feedDefault();
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; argv && i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--update") == 0)
            g.updateMode = true;
        else if (_wcsicmp(argv[i], L"--uninstall") == 0)
            g.uninstallMode = true;
        else if (_wcsicmp(argv[i], L"--feed") == 0 && i + 1 < argc)
            g.feedUrl = argv[++i];
        else if (_wcsicmp(argv[i], L"--version") == 0 && i + 1 < argc) {
            g.requestedVersion = argv[++i];
            g.versionFromArgs = !g.requestedVersion.empty();
        }
    }
    if (argv)
        LocalFree(argv);

    if (!g.uninstallMode) {
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        const fs::path app = localRoot() / L"app";
        if (pathInDirectory(self, app)) {
            wchar_t temp[MAX_PATH]{};
            GetTempPathW(MAX_PATH, temp);
            const fs::path staged = fs::path(temp) / L"NuClipSetup-run.exe";
            std::error_code copyError;
            fs::copy_file(self, staged, fs::copy_options::overwrite_existing, copyError);
            if (!copyError) {
                std::wstring args = L"--feed \"" + g.feedUrl + L"\"";
                if (g.updateMode)
                    args += L" --update";
                if (g.versionFromArgs && !g.requestedVersion.empty())
                    args += L" --version " + g.requestedVersion;
                ShellExecuteW(nullptr, L"open", staged.c_str(), args.c_str(), nullptr, SW_SHOWNORMAL);
                return 0;
            }
        }
    }

    if (g.uninstallMode) {
        wchar_t self[MAX_PATH]{};
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        const fs::path root = localRoot();
        if (pathInDirectory(self, root)) {
            wchar_t temp[MAX_PATH]{};
            GetTempPathW(MAX_PATH, temp);
            const fs::path staged = fs::path(temp) / L"NuClip-uninstall.exe";
            std::error_code copyError;
            fs::copy_file(self, staged, fs::copy_options::overwrite_existing, copyError);
            if (!copyError) {
                ShellExecuteW(nullptr, L"open", staged.c_str(), L"--uninstall", nullptr, SW_SHOWNORMAL);
                return 0;
            }
        }
    }

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"NuClipSetup";
    windowClass.hbrBackground = nullptr;
    RegisterClassW(&windowClass);

    const int dpi = GetDpiForSystem();
    const int width = MulDiv(480, dpi, 96);
    const int height = MulDiv(560, dpi, 96);
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    const int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - width) / 2;
    const int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - height) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"NuClipSetup", L"NuClip Setup", WS_POPUP, x, y, width, height,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd)
        return 1;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (g.title)
        DeleteObject(g.title);
    if (g.body)
        DeleteObject(g.body);
    if (g.label)
        DeleteObject(g.label);
    if (g.button)
        DeleteObject(g.button);
    return static_cast<int>(message.wParam);
}
