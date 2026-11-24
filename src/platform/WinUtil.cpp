#include "platform/WinUtil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QWindow>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwmapi.h>
#include <tlhelp32.h>
#include <windows.h>

#include <cstdio>

namespace nuclip::WinUtil {
namespace {

COLORREF toColorRef(const QColor &color)
{
    return RGB(color.red(), color.green(), color.blue());
}

} // namespace

void attachParentConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE *out = nullptr;
    FILE *err = nullptr;
    freopen_s(&out, "CONOUT$", "w", stdout);
    freopen_s(&err, "CONOUT$", "w", stderr);
}

void setLaunchOnStartup(bool enabled)
{
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    if (enabled) {
        const QString path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        reg.setValue(QStringLiteral("NuClip"), QStringLiteral("\"%1\"").arg(path));
    } else {
        reg.remove(QStringLiteral("NuClip"));
    }
}

void setTitleBarColors(QWindow *window, const QColor &caption, const QColor &text, const QColor &border, bool dark)
{
    if (!window)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return;
    const BOOL useDark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
    const COLORREF captionRef = toColorRef(caption);
    const COLORREF textRef = toColorRef(text);
    const COLORREF borderRef = toColorRef(border);
    DwmSetWindowAttribute(hwnd, 35, &captionRef, sizeof(captionRef));
    DwmSetWindowAttribute(hwnd, 36, &textRef, sizeof(textRef));
    DwmSetWindowAttribute(hwnd, 34, &borderRef, sizeof(borderRef));
}

void revealInExplorer(const QString &path)
{
    const QString native = QDir::toNativeSeparators(path);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + native});
}

void closeProcessesFrom(const QString &executablePath)
{
    const QString mine = QFileInfo(executablePath).absoluteFilePath();
    if (mine.isEmpty())
        return;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"ffmpeg.exe") != 0 && _wcsicmp(entry.szExeFile, L"ffprobe.exe") != 0)
                continue;
            const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
            if (!process)
                continue;
            wchar_t image[1024];
            DWORD length = 1024;
            if (QueryFullProcessImageNameW(process, 0, image, &length)) {
                const QString path = QFileInfo(QString::fromWCharArray(image)).absoluteFilePath();
                if (path.compare(mine, Qt::CaseInsensitive) == 0)
                    TerminateProcess(process, 1);
            }
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

void tieProcessLifetime(qint64 pid)
{
    if (pid <= 0)
        return;
    static HANDLE job = nullptr;
    if (!job) {
        job = CreateJobObjectW(nullptr, nullptr);
        if (!job)
            return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    }
    const HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return;
    AssignProcessToJobObject(job, process);
    CloseHandle(process);
}

} // namespace nuclip::WinUtil
