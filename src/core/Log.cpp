#include "core/Log.h"

#include "core/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>

namespace nuclip::Log {
namespace {

QMutex g_mutex;
QString g_path;
bool g_ready = false;

void writeLine(const QString &level, const QString &message)
{
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        + QStringLiteral(" ") + level + QStringLiteral(" ") + message;
    const QByteArray utf8 = line.toUtf8();
    std::fprintf(stderr, "%s\n", utf8.constData());
    std::fflush(stderr);

    if (!g_ready)
        return;
    QMutexLocker lock(&g_mutex);
    QFileInfo info(g_path);
    if (info.exists() && info.size() > 2 * 1024 * 1024) {
        QFile::remove(g_path + QStringLiteral(".1"));
        QFile::rename(g_path, g_path + QStringLiteral(".1"));
    }
    QFile file(g_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        file.write(utf8);
        file.write("\n");
    }
}

void qtHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    switch (type) {
    case QtDebugMsg:
    case QtInfoMsg:
        writeLine(QStringLiteral("INFO"), message);
        break;
    case QtWarningMsg:
        writeLine(QStringLiteral("WARN"), message);
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        writeLine(QStringLiteral("ERROR"), message);
        break;
    }
}

} // namespace

void init()
{
    QDir().mkpath(Paths::logsDir());
    g_path = QDir(Paths::logsDir()).filePath(QStringLiteral("nuclip.log"));
    g_ready = true;
    qInstallMessageHandler(qtHandler);
    info(QStringLiteral("NuClip log started"));
}

void info(const QString &message) { writeLine(QStringLiteral("INFO"), message); }
void warn(const QString &message) { writeLine(QStringLiteral("WARN"), message); }
void error(const QString &message) { writeLine(QStringLiteral("ERROR"), message); }

} // namespace nuclip::Log
