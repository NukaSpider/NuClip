#include "recording/CaptureCommand.h"

#include <algorithm>

namespace nuclip {
namespace {

QString ffmpegCodecName(const QString &encoder, const QString &codec)
{
    const bool hevc = codec == QLatin1String("hevc");
    const bool av1 = codec == QLatin1String("av1");
    if (encoder == QLatin1String("nvenc"))
        return hevc ? QStringLiteral("hevc_nvenc") : av1 ? QStringLiteral("av1_nvenc") : QStringLiteral("h264_nvenc");
    if (encoder == QLatin1String("amf"))
        return hevc ? QStringLiteral("hevc_amf") : av1 ? QStringLiteral("av1_amf") : QStringLiteral("h264_amf");
    if (encoder == QLatin1String("qsv"))
        return hevc ? QStringLiteral("hevc_qsv") : av1 ? QStringLiteral("av1_qsv") : QStringLiteral("h264_qsv");
    if (hevc)
        return QStringLiteral("libx265");
    if (av1)
        return QStringLiteral("libsvtav1");
    return QStringLiteral("libx264");
}

QStringList videoEncoder(const QString &encoder, const QString &codec, int bitrateKbps, int fps)
{
    const QString rate = QString::number(std::max(500, bitrateKbps)) + QLatin1Char('k');
    const QString peak = QString::number(std::max(500, int(bitrateKbps * 1.25))) + QLatin1Char('k');
    const QString gop = QString::number(std::max(15, fps));
    const QString name = ffmpegCodecName(encoder, codec);
    if (encoder == QLatin1String("nvenc")) {
        QStringList args{QStringLiteral("-c:v"), name, QStringLiteral("-preset"), QStringLiteral("p5"),
                         QStringLiteral("-tune"), QStringLiteral("ll"), QStringLiteral("-rc"), QStringLiteral("vbr"),
                         QStringLiteral("-b:v"), rate, QStringLiteral("-maxrate"), peak, QStringLiteral("-bufsize"),
                         rate, QStringLiteral("-g"), gop};
        if (name != QLatin1String("av1_nvenc"))
            args << QStringLiteral("-bf") << QStringLiteral("0");
        if (name == QLatin1String("h264_nvenc"))
            args << QStringLiteral("-delay") << QStringLiteral("0");
        return args;
    }
    if (encoder == QLatin1String("amf")) {
        return {QStringLiteral("-c:v"), name, QStringLiteral("-quality"), QStringLiteral("balanced"),
                QStringLiteral("-rc"), QStringLiteral("vbr_peak"), QStringLiteral("-b:v"), rate,
                QStringLiteral("-maxrate"), peak, QStringLiteral("-g"), gop};
    }
    if (encoder == QLatin1String("qsv")) {
        return {QStringLiteral("-c:v"), name, QStringLiteral("-b:v"), rate, QStringLiteral("-maxrate"), peak,
                QStringLiteral("-g"), gop, QStringLiteral("-look_ahead"), QStringLiteral("0")};
    }
    if (name == QLatin1String("libx265")) {
        return {QStringLiteral("-c:v"), name, QStringLiteral("-preset"), QStringLiteral("veryfast"),
                QStringLiteral("-tune"), QStringLiteral("zerolatency"), QStringLiteral("-b:v"), rate,
                QStringLiteral("-maxrate"), peak, QStringLiteral("-bufsize"), rate, QStringLiteral("-g"), gop,
                QStringLiteral("-x265-params"),
                QStringLiteral("bframes=0:keyint=%1:min-keyint=%1").arg(gop), QStringLiteral("-pix_fmt"),
                QStringLiteral("yuv420p")};
    }
    if (name == QLatin1String("libsvtav1")) {
        return {QStringLiteral("-c:v"), name, QStringLiteral("-preset"), QStringLiteral("10"), QStringLiteral("-b:v"),
                rate, QStringLiteral("-g"), gop, QStringLiteral("-svtav1-params"),
                QStringLiteral("keyint=%1:lookahead=0").arg(gop), QStringLiteral("-pix_fmt"),
                QStringLiteral("yuv420p")};
    }
    return {QStringLiteral("-c:v"), QStringLiteral("libx264"), QStringLiteral("-preset"), QStringLiteral("veryfast"),
            QStringLiteral("-tune"), QStringLiteral("zerolatency"), QStringLiteral("-b:v"), rate,
            QStringLiteral("-maxrate"), peak, QStringLiteral("-bufsize"), rate, QStringLiteral("-g"), gop,
            QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p")};
}

QString captureGraph(const CaptureRequest &request)
{
    QString graph = QStringLiteral("ddagrab=output_idx=%1:framerate=%2:draw_mouse=%3,hwdownload,format=bgra,fps=%2")
                        .arg(std::max(0, request.monitor))
                        .arg(std::max(15, request.fps))
                        .arg(request.cursor ? 1 : 0);
    if (request.width >= 320 && request.height >= 240) {
        const int width = request.width - (request.width % 2);
        const int height = request.height - (request.height % 2);
        graph += QStringLiteral(",scale=%1:%2").arg(width).arg(height);
    } else {
        graph += QStringLiteral(",scale=trunc(iw/2)*2:trunc(ih/2)*2");
    }
    graph += QStringLiteral(",format=yuv420p");
    return graph;
}

} // namespace

QStringList captureArguments(const CaptureRequest &request)
{
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("info"),
                     QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                     captureGraph(request)};
    for (const QString &pipe : request.pipes) {
        args << QStringLiteral("-f")
             << QStringLiteral("f32le") << QStringLiteral("-sample_rate") << QStringLiteral("48000")
             << QStringLiteral("-ch_layout") << QStringLiteral("stereo") << QStringLiteral("-probesize")
             << QStringLiteral("32") << QStringLiteral("-analyzeduration") << QStringLiteral("0")
             << QStringLiteral("-i") << pipe;
    }
    args << QStringLiteral("-map") << QStringLiteral("0:v:0");
    for (int i = 0; i < request.pipes.size(); ++i)
        args << QStringLiteral("-map") << QStringLiteral("%1:a:0").arg(i + 1);
    args << videoEncoder(request.encoder, request.codec, request.videoBitrateKbps, request.fps);
    args << QStringLiteral("-force_key_frames") << QStringLiteral("expr:gte(t,n_forced*1)");
    if (!request.pipes.isEmpty()) {
        args << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-b:a")
             << (QString::number(std::max(64, request.audioBitrateKbps)) + QLatin1Char('k')) << QStringLiteral("-ar")
             << QStringLiteral("48000");
    }
    args << QStringLiteral("-fps_mode") << QStringLiteral("cfr") << QStringLiteral("-f") << QStringLiteral("segment")
         << QStringLiteral("-segment_time") << QStringLiteral("1") << QStringLiteral("-segment_format")
         << QStringLiteral("mpegts") << QStringLiteral("-segment_start_number")
         << QString::number(std::max(0, request.segmentStart)) << QStringLiteral("-reset_timestamps")
         << QStringLiteral("1") << request.pattern;
    return args;
}

QStringList concatArguments(const QString &listFile, const QString &output)
{
    return {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("warning"),
            QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("concat"), QStringLiteral("-safe"),
            QStringLiteral("0"), QStringLiteral("-i"), listFile, QStringLiteral("-c"), QStringLiteral("copy"),
            QStringLiteral("-avoid_negative_ts"), QStringLiteral("make_zero"), output};
}

QStringList screenshotArguments(int monitor, const QString &output)
{
    const QString graph = QStringLiteral("ddagrab=output_idx=%1:framerate=1:draw_mouse=1,hwdownload,format=bgra,format=yuv420p")
                              .arg(std::max(0, monitor));
    return {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("warning"),
            QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"), graph,
            QStringLiteral("-frames:v"), QStringLiteral("1"), output};
}

QStringList frameArguments(const QString &media, const QString &output)
{
    return {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("warning"),
            QStringLiteral("-y"), QStringLiteral("-i"), media, QStringLiteral("-frames:v"), QStringLiteral("1"),
            output};
}

QStringList thumbnailArguments(const QString &media, const QString &output)
{
    return {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
            QStringLiteral("-y"), QStringLiteral("-ss"), QStringLiteral("0.25"), QStringLiteral("-i"), media,
            QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-vf"), QStringLiteral("scale=640:-2"),
            output};
}

} // namespace nuclip
