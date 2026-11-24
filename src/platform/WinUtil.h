#pragma once

#include <QColor>
#include <QString>

class QWindow;

namespace nuclip::WinUtil {

void attachParentConsole();
void setLaunchOnStartup(bool enabled);
void setTitleBarColors(QWindow *window, const QColor &caption, const QColor &text, const QColor &border, bool dark);
void revealInExplorer(const QString &path);
void closeProcessesFrom(const QString &executablePath);
void tieProcessLifetime(qint64 pid);

} // namespace nuclip::WinUtil
