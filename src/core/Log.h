#pragma once

#include <QString>

namespace nuclip::Log {

void init();
void info(const QString &message);
void warn(const QString &message);
void error(const QString &message);

} // namespace nuclip::Log
