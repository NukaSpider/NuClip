#include "platform/TrayController.h"

#include "core/Settings.h"
#include "recording/RecordingEngine.h"

#include <algorithm>

#include <QApplication>
#include <QAudioOutput>
#include <QVariantMap>
#include <QEvent>
#include <QMediaPlayer>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QWindow>

namespace nuclip {

TrayController::TrayController(Settings *settings, RecordingEngine *recording, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_recording(recording)
{
    m_savedAudio = new QAudioOutput(this);
    m_savedAudio->setVolume(1.0);
    m_savedSound = new QMediaPlayer(this);
    m_savedSound->setAudioOutput(m_savedAudio);
    m_savedSound->setSource(QUrl(QStringLiteral("qrc:/assets/resources/saved.mp3")));
    connect(m_savedSound, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
            primeSavedSound();
    });
    connect(m_savedSound, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        if (!m_priming || state != QMediaPlayer::PlayingState)
            return;
        m_savedSound->pause();
        m_savedSound->setPosition(0);
        m_savedAudio->setVolume(float(std::clamp(m_settings->sfxVolume(), 0.0, 1.0)));
        m_priming = false;
    });
    m_lastState = m_recording->state();
    connect(m_recording, &RecordingEngine::clipAccepted, this, [this](const QString &, double) { playEvent(QStringLiteral("clip")); });
    connect(m_recording, &RecordingEngine::screenshotAccepted, this, [this] { playEvent(QStringLiteral("shot")); });
    connect(m_recording, &RecordingEngine::errorChanged, this, [this] {
        const QString text = m_recording->error().simplified();
        if (text.isEmpty()) {
            m_lastError.clear();
            return;
        }
        if (text == m_lastError)
            return;
        m_lastError = text;
        playEvent(QStringLiteral("error"));
    });
    connect(m_recording, &RecordingEngine::stateChanged, this, [this] { onRecordingState(); });
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    m_tray = new QSystemTrayIcon(this);
    QIcon icon = QApplication::windowIcon();
    if (icon.isNull()) {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(QStringLiteral("#3DDC97")));
        painter.drawRoundedRect(QRectF(1, 1, 30, 30), 8, 8);
        painter.setBrush(QColor(QStringLiteral("#07140F")));
        QPolygonF mark;
        mark << QPointF(12, 9) << QPointF(12, 23) << QPointF(23, 16);
        painter.drawPolygon(mark);
        icon = QIcon(pixmap);
    }
    m_tray->setIcon(icon);
    m_tray->setToolTip(QStringLiteral("NuClip"));
    rebuildMenu();
    m_tray->show();
    QApplication::setQuitOnLastWindowClosed(false);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });
    connect(m_recording, &RecordingEngine::stateChanged, this, [this] {
        m_tray->setToolTip(QStringLiteral("NuClip — %1").arg(m_recording->stateText()));
        rebuildMenu();
    });
}

void TrayController::primeSavedSound()
{
    if (m_primed || !m_savedSound)
        return;
    m_primed = true;
    m_priming = true;
    m_savedAudio->setVolume(0);
    m_savedSound->play();
}

QVariantList TrayController::sounds() const
{
    QVariantList list;
    QVariantMap item;
    item.insert(QStringLiteral("id"), QStringLiteral("nuclip"));
    item.insert(QStringLiteral("label"), QStringLiteral("NuClip"));
    list.append(item);
    return list;
}

void TrayController::previewSound()
{
    startPlayback();
}

void TrayController::playEvent(const QString &event)
{
    if (!m_settings->sfxEnabled())
        return;
    const bool allowed = (event == QLatin1String("clip") && m_settings->sfxClip())
        || (event == QLatin1String("shot") && m_settings->sfxScreenshot())
        || (event == QLatin1String("start") && m_settings->sfxRecordStart())
        || (event == QLatin1String("stop") && m_settings->sfxRecordStop())
        || (event == QLatin1String("error") && m_settings->sfxError());
    if (!allowed)
        return;
    startPlayback();
}

void TrayController::startPlayback()
{
    if (!m_savedSound)
        return;
    m_priming = false;
    m_savedAudio->setVolume(float(std::clamp(m_settings->sfxVolume(), 0.0, 1.0)));
    if (m_savedSound->playbackState() != QMediaPlayer::StoppedState)
        m_savedSound->setPosition(0);
    m_savedSound->play();
}

void TrayController::onRecordingState()
{
    const int state = m_recording->state();
    const bool session = m_recording->mode() == QLatin1String("session");
    if (session && state == RecordingEngine::Recording && m_lastState == RecordingEngine::Starting) {
        playEvent(QStringLiteral("start"));
        m_sessionLive = true;
    } else if (m_sessionLive && session && state == RecordingEngine::Stopping) {
        playEvent(QStringLiteral("stop"));
        m_sessionLive = false;
    }
    if (state == RecordingEngine::Idle || state == RecordingEngine::Error)
        m_sessionLive = false;
    m_lastState = state;
}

void TrayController::attach(QWindow *window)
{
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (m_window) {
        m_window->installEventFilter(this);
        if (m_tray && !QApplication::windowIcon().isNull())
            m_tray->setIcon(QApplication::windowIcon());
    }
}

void TrayController::quit()
{
    m_quitting = true;
    emit quittingChanged();
    m_recording->shutdown();
    QApplication::quit();
}

bool TrayController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::Close && m_tray && !m_quitting) {
        event->ignore();
        m_window->hide();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

void TrayController::rebuildMenu()
{
    if (!m_tray)
        return;
    QMenu *menu = new QMenu();
    QAction *open = menu->addAction(QStringLiteral("Open NuClip"));
    connect(open, &QAction::triggered, this, &TrayController::showRequested);
    const bool active = m_recording->state() == RecordingEngine::Recording
        || m_recording->state() == RecordingEngine::Paused
        || m_recording->state() == RecordingEngine::Starting;
    QAction *toggle = menu->addAction(active ? QStringLiteral("Stop replay") : QStringLiteral("Start replay"));
    connect(toggle, &QAction::triggered, m_recording, &RecordingEngine::toggle);
    QAction *save = menu->addAction(QStringLiteral("Save clip"));
    connect(save, &QAction::triggered, this, [this] { m_recording->saveClip(0); });
    menu->addSeparator();
    QAction *quit = menu->addAction(QStringLiteral("Quit"));
    connect(quit, &QAction::triggered, this, &TrayController::quit);
    QMenu *previous = m_tray->contextMenu();
    m_tray->setContextMenu(menu);
    if (previous)
        previous->deleteLater();
}

} // namespace nuclip
