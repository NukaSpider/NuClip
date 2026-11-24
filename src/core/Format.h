#pragma once

#include <QtGlobal>
#include <QString>

namespace nuclip {

inline QString formatDuration(double seconds)
{
    if (seconds < 0)
        seconds = 0;
    const int total = int(seconds + 0.5);
    const int h = total / 3600;
    const int m = (total % 3600) / 60;
    const int s = total % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

inline QString formatBytes(qint64 bytes)
{
    if (bytes < 0)
        bytes = 0;
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;
    if (bytes >= qint64(gb))
        return QString::number(bytes / gb, 'f', bytes >= qint64(10 * gb) ? 1 : 2) + QStringLiteral(" GB");
    if (bytes >= qint64(mb))
        return QString::number(bytes / mb, 'f', 1) + QStringLiteral(" MB");
    if (bytes >= qint64(kb))
        return QString::number(bytes / kb, 'f', 0) + QStringLiteral(" KB");
    return QString::number(bytes) + QStringLiteral(" B");
}

inline QString formatClock(double seconds)
{
    return formatDuration(seconds);
}

} // namespace nuclip
