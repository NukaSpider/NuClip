#include "recording/FfmpegJob.h"

#include "core/Log.h"
#include "platform/WinUtil.h"

#include <QProcess>
#include <QRegularExpression>

namespace nuclip {

FfmpegJob::FfmpegJob(QObject *parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        consume(m_process->readAllStandardError());
    });
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_stdout += QString::fromLocal8Bit(m_process->readAllStandardOutput());
        if (m_stdout.size() > 200000)
            m_stdout = m_stdout.right(160000);
    });
    connect(m_process, &QProcess::finished, this, [this](int code) {
        m_stdout += QString::fromLocal8Bit(m_process->readAllStandardOutput());
        consume(m_process->readAllStandardError());
        emit finished(code);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            emit finished(-1);
    });
    connect(m_process, &QProcess::started, this, [this] {
        WinUtil::tieProcessLifetime(m_process->processId());
    });
}

bool FfmpegJob::wait(int timeoutMs)
{
    if (!running())
        return true;
    return m_process->waitForFinished(timeoutMs);
}

void FfmpegJob::start(const QString &program, const QStringList &args)
{
    if (running())
        kill();
    m_tail.clear();
    m_stdout.clear();
    m_seconds = 0;
    m_speed = 0;
    Log::info(program + QLatin1Char(' ') + args.join(QLatin1Char(' ')));
    m_process->start(program, args);
}

void FfmpegJob::requestQuit()
{
    if (!running())
        return;
    m_process->write("q\n");
    m_process->closeWriteChannel();
}

void FfmpegJob::kill()
{
    if (running())
        m_process->kill();
}

bool FfmpegJob::running() const
{
    return m_process->state() != QProcess::NotRunning;
}

void FfmpegJob::consume(const QByteArray &chunk)
{
    m_tail += QString::fromLocal8Bit(chunk);
    if (m_tail.size() > 12000)
        m_tail = m_tail.right(8000);
    const QString recent = m_tail.right(240);
    static const QRegularExpression timeRe(QStringLiteral("time=(\\d+):(\\d+):(\\d+(?:\\.\\d+)?)"));
    static const QRegularExpression speedRe(QStringLiteral("speed=\\s*(\\d+(?:\\.\\d+)?)x"));
    auto timeMatch = timeRe.match(recent);
    QRegularExpressionMatch lastTime;
    while (timeMatch.hasMatch()) {
        lastTime = timeMatch;
        timeMatch = timeRe.match(recent, timeMatch.capturedEnd());
    }
    if (lastTime.hasMatch()) {
        m_seconds = lastTime.captured(1).toInt() * 3600.0 + lastTime.captured(2).toInt() * 60.0
            + lastTime.captured(3).toDouble();
    }
    auto speedMatch = speedRe.match(recent);
    QRegularExpressionMatch lastSpeed;
    while (speedMatch.hasMatch()) {
        lastSpeed = speedMatch;
        speedMatch = speedRe.match(recent, speedMatch.capturedEnd());
    }
    if (lastSpeed.hasMatch())
        m_speed = lastSpeed.captured(1).toDouble();
    if (lastTime.hasMatch() || lastSpeed.hasMatch())
        emit progress(m_seconds, m_speed);
    emit tailChanged();
}

} // namespace nuclip
