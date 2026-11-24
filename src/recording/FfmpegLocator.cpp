#include "recording/FfmpegLocator.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace nuclip {

QString ffmpegExecutable()
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/ffmpeg.exe");
    return QFileInfo::exists(path) ? path : QString();
}

QString ffprobeExecutable()
{
    const QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/ffprobe.exe");
    return QFileInfo::exists(path) ? path : QString();
}

} // namespace nuclip
