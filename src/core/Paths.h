#pragma once

#include <QString>

namespace nuclip::Paths {

QString dataDir();
QString logsDir();
QString bufferDir();
QString thumbnailDir();
QString clipMetaDir();
QString settingsFile();
QString defaultClipsDir();
QString screenshotsDirForClips(const QString &clipsDir);
QString sanitizeName(const QString &name);

} // namespace nuclip::Paths
