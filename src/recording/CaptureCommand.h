#pragma once

#include <QString>
#include <QStringList>

namespace nuclip {

struct CaptureRequest {
    int monitor = 0;
    int fps = 60;
    int width = 0;
    int height = 0;
    bool cursor = true;
    QString encoder;
    QString codec = QStringLiteral("h264");
    int videoBitrateKbps = 15000;
    int audioBitrateKbps = 192;
    QStringList pipes;
    QString pattern;
    int segmentStart = 0;
};

QStringList captureArguments(const CaptureRequest &request);
QStringList concatArguments(const QString &listFile, const QString &output);
QStringList screenshotArguments(int monitor, const QString &output);
QStringList frameArguments(const QString &media, const QString &output);
QStringList thumbnailArguments(const QString &media, const QString &output);

} // namespace nuclip
