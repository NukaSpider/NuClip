#pragma once

#include <QObject>
#include <QStringList>

class QProcess;

namespace nuclip {

class FfmpegJob : public QObject {
    Q_OBJECT

public:
    explicit FfmpegJob(QObject *parent = nullptr);

    void start(const QString &program, const QStringList &args);
    void requestQuit();
    void kill();
    bool wait(int timeoutMs);
    bool running() const;
    QString tail() const { return m_tail; }
    QString stdoutText() const { return m_stdout; }
    double mediaSeconds() const { return m_seconds; }
    double speed() const { return m_speed; }

signals:
    void progress(double seconds, double speed);
    void finished(int exitCode);
    void tailChanged();

private:
    void consume(const QByteArray &chunk);

    QProcess *m_process = nullptr;
    QString m_tail;
    QString m_stdout;
    double m_seconds = 0;
    double m_speed = 0;
};

} // namespace nuclip
