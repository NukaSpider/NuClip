#include "storage/StorageMonitor.h"

#include "core/Format.h"
#include "core/Settings.h"

#include <QDir>
#include <algorithm>
#include <QFileInfo>
#include <QStorageInfo>
#include <QtConcurrent>

namespace nuclip {
namespace {

qint64 folderBytes(const QString &root)
{
    qint64 total = 0;
    QList<QString> pending{root};
    while (!pending.isEmpty()) {
        const QString directory = pending.takeFirst();
        const QFileInfoList entries = QDir(directory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &info : entries) {
            if (info.isDir())
                pending.append(info.absoluteFilePath());
            else
                total += info.size();
        }
    }
    return total;
}

} // namespace

StorageMonitor::StorageMonitor(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    connect(m_settings, &Settings::clipsDirectoryChanged, this, &StorageMonitor::refresh);
}

void StorageMonitor::refresh()
{
    const int generation = ++m_generation;
    const QString path = m_settings->clipsDirectory();
    (void)QtConcurrent::run([this, generation, path] {
        const qint64 used = folderBytes(path);
        const QStorageInfo info(path);
        const qint64 free = info.bytesAvailable();
        QMetaObject::invokeMethod(this, [this, generation, path, used, free] {
            if (generation != m_generation)
                return;
            m_path = path;
            m_usedText = formatBytes(used);
            m_freeText = free >= 0 ? formatBytes(free) : QStringLiteral("—");
            const double span = double(used) + double(std::max<qint64>(0, free));
            m_ratio = span > 0 ? qBound(0.0, double(used) / span, 1.0) : 0;
            emit changed();
        }, Qt::QueuedConnection);
    });
}

} // namespace nuclip
