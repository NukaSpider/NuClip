#include "core/Paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

#define WIN32_LEAN_AND_MEAN
#include <ShlObj.h>
#include <combaseapi.h>

namespace nuclip::Paths {

QString dataDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/AppData/Local");
    const QString path = QDir(base).filePath(QStringLiteral("NuClip"));
    QDir().mkpath(path);
    return path;
}

QString logsDir()
{
    const QString path = QDir(dataDir()).filePath(QStringLiteral("logs"));
    QDir().mkpath(path);
    return path;
}

QString bufferDir()
{
    const QString path = QDir(dataDir()).filePath(QStringLiteral("buffer"));
    QDir().mkpath(path);
    return path;
}

QString thumbnailDir()
{
    const QString path = QDir(dataDir()).filePath(QStringLiteral("thumbnails"));
    QDir().mkpath(path);
    return path;
}

QString clipMetaDir()
{
    const QString path = QDir(dataDir()).filePath(QStringLiteral("clipmeta"));
    QDir().mkpath(path);
    return path;
}

QString settingsFile()
{
    return QDir(dataDir()).filePath(QStringLiteral("settings.json"));
}

QString defaultClipsDir()
{
    QString videos;
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, KF_FLAG_DEFAULT, nullptr, &raw)) && raw) {
        videos = QString::fromWCharArray(raw);
        CoTaskMemFree(raw);
    }
    if (videos.isEmpty())
        videos = QDir::homePath() + QStringLiteral("/Videos");
    const QString path = QDir(videos).filePath(QStringLiteral("NuClip/Clips"));
    QDir().mkpath(path);
    return QDir::cleanPath(path);
}

QString screenshotsDirForClips(const QString &clipsDir)
{
    QDir parent(clipsDir);
    parent.cdUp();
    const QString path = parent.filePath(QStringLiteral("Screenshots"));
    QDir().mkpath(path);
    return QDir::cleanPath(path);
}

QString sanitizeName(const QString &name)
{
    QString out;
    out.reserve(name.size());
    const QString invalid = QStringLiteral("<>:\"/\\|?*");
    for (const QChar ch : name) {
        if (ch.unicode() < 32 || invalid.contains(ch))
            out.append(QLatin1Char('_'));
        else
            out.append(ch);
    }
    out = out.trimmed();
    while (out.endsWith(QLatin1Char('.')) || out.endsWith(QLatin1Char(' ')))
        out.chop(1);
    if (out.isEmpty())
        out = QStringLiteral("Desktop");
    if (out.size() > 60)
        out.truncate(60);
    return out;
}

} // namespace nuclip::Paths
