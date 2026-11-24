#include "platform/OverlayService.h"

#include "core/Log.h"
#include "core/Settings.h"
#include "core/Theme.h"
#include "platform/DeviceCatalog.h"
#include "platform/GameDetector.h"
#include "recording/RecordingEngine.h"

#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickView>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QWindow>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

#include <algorithm>
#include <cmath>

namespace nuclip {

OverlayService::OverlayService(Settings *settings, Theme *theme, DeviceCatalog *devices, GameDetector *detector,
                               RecordingEngine *recording, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_devices(devices)
    , m_detector(detector)
    , m_recording(recording)
{
    m_motion = new QVariantAnimation(this);
    m_motion->setDuration(220);
    connect(m_motion, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal t = value.toReal();
        const bool slide = m_settings->overlayAnimation() == QLatin1String("slide");
        if (m_phase == Phase::Exit) {
            m_fade = 1.0 - t;
            m_travel = slide ? t : 0;
        } else {
            m_fade = t;
            m_travel = slide ? 1.0 - t : 0;
        }
        emit motionChanged();
    });
    connect(m_motion, &QVariantAnimation::finished, this, [this] {
        if (m_stoppingMotion)
            return;
        if (m_phase == Phase::Enter) {
            m_phase = Phase::Hold;
            m_fade = 1;
            m_travel = 0;
            emit motionChanged();
            m_hold->start(m_settings->overlayDurationMs());
        } else if (m_phase == Phase::Exit) {
            m_phase = Phase::Idle;
            m_showing = false;
            if (m_view)
                m_view->hide();
            presentNext();
        }
    });
    m_hold = new QTimer(this);
    m_hold->setSingleShot(true);
    connect(m_hold, &QTimer::timeout, this, [this] { beginExit(); });

    connect(m_recording, &RecordingEngine::clipAccepted, this, &OverlayService::onClip);
    connect(m_recording, &RecordingEngine::screenshotAccepted, this, [this] { onScreenshot(); });
    connect(m_recording, &RecordingEngine::errorChanged, this, [this] { onError(); });
    m_lastState = m_recording->state();
    connect(m_recording, &RecordingEngine::stateChanged, this, [this] { onRecordingState(); });
    connect(m_settings, &Settings::overlayChanged, this, [this] {
        readAnchor();
        emit layoutChanged();
        if (!m_settings->overlayEnabled()) {
            m_queue.clear();
            m_hold->stop();
            m_phase = Phase::Idle;
            m_showing = false;
            if (m_view)
                m_view->hide();
            return;
        }
        if (m_showing)
            place();
    });
    connect(m_theme, &Theme::changed, this, [this] { emit layoutChanged(); });
    const auto reposition = [this] {
        if (m_showing)
            place();
    };
    connect(qApp, &QGuiApplication::screenAdded, this, reposition);
    connect(qApp, &QGuiApplication::screenRemoved, this, reposition);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, reposition);
    readAnchor();
}

bool OverlayService::compact() const
{
    return m_settings->overlaySize() == QLatin1String("compact");
}

bool OverlayService::large() const
{
    return m_settings->overlaySize() == QLatin1String("large");
}

int OverlayService::cardWidth() const
{
    if (compact())
        return 252;
    if (large())
        return 340;
    return 292;
}

int OverlayService::cardHeight() const
{
    if (compact())
        return 52;
    if (large())
        return 72;
    return 60;
}

qreal OverlayService::panelOpacity() const
{
    return m_settings->overlayOpacity();
}

QColor OverlayService::accent() const
{
    if (m_settings->overlayUseAccent())
        return m_theme->accent();
    return QColor(QStringLiteral("#3DDC97"));
}

void OverlayService::preview()
{
    Note note;
    note.kind = QStringLiteral("clip");
    note.coalesce = false;
    note.count = 1;
    note.detail = appName() + QStringLiteral(" · 30 seconds");
    applyCount(&note);
    m_queue.clear();
    if (m_showing) {
        publish(note);
        readAnchor();
        emit layoutChanged();
        place();
        if (m_view && !m_view->isVisible()) {
            m_view->show();
            applyNative();
        }
        startEnter();
        return;
    }
    m_queue.append(note);
    presentNext();
}

void OverlayService::onClip(const QString &game, double seconds)
{
    Note note;
    note.kind = QStringLiteral("clip");
    note.coalesce = true;
    note.count = 1;
    const QString name = game.trimmed().isEmpty() ? appName() : game.trimmed();
    const int span = std::max(0, int(std::lround(seconds)));
    if (span >= 1)
        note.detail = QStringLiteral("%1 · %2 second%3").arg(name).arg(span).arg(span == 1 ? QString() : QStringLiteral("s"));
    else
        note.detail = name;
    applyCount(&note);
    post(note);
}

void OverlayService::onScreenshot()
{
    Note note;
    note.kind = QStringLiteral("shot");
    note.coalesce = true;
    note.count = 1;
    note.detail = appName();
    applyCount(&note);
    post(note);
}

void OverlayService::onError()
{
    const QString text = cleanError(m_recording->error());
    Note gate;
    gate.kind = QStringLiteral("error");
    if (text.isEmpty() || !allowed(gate))
        return;
    if (m_showing && m_phase != Phase::Exit && m_kind == QLatin1String("error") && m_detail == text) {
        if (m_phase == Phase::Hold)
            m_hold->start(m_settings->overlayDurationMs());
        return;
    }
    if (m_showing && m_phase != Phase::Exit
        && (m_kind == QLatin1String("clip") || m_kind == QLatin1String("shot"))) {
        Note note;
        note.kind = QStringLiteral("error");
        note.title = QStringLiteral("Capture error");
        note.detail = text;
        publish(note);
        emit layoutChanged();
        if (m_phase == Phase::Hold)
            m_hold->start(m_settings->overlayDurationMs());
        return;
    }
    for (const Note &queued : m_queue) {
        if (queued.kind == QLatin1String("error") && queued.detail == text)
            return;
    }
    Note note;
    note.kind = QStringLiteral("error");
    note.title = QStringLiteral("Capture error");
    note.detail = text;
    note.coalesce = false;
    post(note);
}

void OverlayService::onRecordingState()
{
    const int state = m_recording->state();
    const bool session = m_recording->mode() == QLatin1String("session");
    if (session && state == RecordingEngine::Recording && m_lastState == RecordingEngine::Starting) {
        Note note;
        note.kind = QStringLiteral("record");
        note.event = QStringLiteral("start");
        note.title = QStringLiteral("Recording started");
        note.detail = appName();
        post(note);
        m_sessionLive = true;
    } else if (m_sessionLive && session && state == RecordingEngine::Stopping) {
        Note note;
        note.kind = QStringLiteral("record");
        note.event = QStringLiteral("stop");
        note.title = QStringLiteral("Recording stopped");
        note.detail = appName();
        post(note);
        m_sessionLive = false;
    }
    if (state == RecordingEngine::Idle || state == RecordingEngine::Error)
        m_sessionLive = false;
    m_lastState = state;
}

void OverlayService::post(Note note)
{
    if (!allowed(note))
        return;
    if (m_showing && m_phase != Phase::Exit && note.coalesce && m_kind == note.kind) {
        Note current;
        current.kind = m_kind;
        current.detail = note.detail;
        current.count = 1;
        const QString digits = m_title.section(QLatin1Char(' '), 0, 0);
        const int shown = digits.toInt();
        current.count = (shown > 1 ? shown : 1) + 1;
        applyCount(&current);
        publish(current);
        if (m_phase == Phase::Hold)
            m_hold->start(m_settings->overlayDurationMs());
        return;
    }
    if (note.coalesce) {
        for (Note &queued : m_queue) {
            if (queued.kind == note.kind) {
                queued.count += 1;
                queued.detail = note.detail;
                applyCount(&queued);
                return;
            }
        }
    }
    if (note.kind == QLatin1String("error"))
        m_queue.prepend(note);
    else
        m_queue.append(note);
    trimQueue();
    if (!m_showing)
        presentNext();
}

void OverlayService::trimQueue()
{
    int guard = 0;
    while (m_queue.size() > 4 && guard++ < 8) {
        int drop = -1;
        for (int i = m_queue.size() - 1; i >= 0; --i) {
            if (m_queue.at(i).kind != QLatin1String("error")) {
                drop = i;
                break;
            }
        }
        if (drop < 0)
            break;
        m_queue.removeAt(drop);
    }
    while (m_queue.size() > 6)
        m_queue.removeLast();
}

void OverlayService::presentNext()
{
    if (m_queue.isEmpty()) {
        m_showing = false;
        return;
    }
    ensureView();
    if (!m_view || m_view->status() == QQuickView::Error) {
        m_queue.clear();
        m_showing = false;
        return;
    }
    publish(m_queue.takeFirst());
    m_showing = true;
    readAnchor();
    emit layoutChanged();
    place();
    m_view->show();
    applyNative();
    QTimer::singleShot(0, this, [this] {
        if (m_showing)
            applyNative();
    });
    startEnter();
    Log::info(QStringLiteral("Overlay %1").arg(m_title));
}

void OverlayService::startEnter()
{
    m_hold->stop();
    m_stoppingMotion = true;
    m_motion->stop();
    m_stoppingMotion = false;
    if (m_settings->overlayAnimation() == QLatin1String("none")) {
        m_phase = Phase::Hold;
        m_fade = 1;
        m_travel = 0;
        emit motionChanged();
        m_hold->start(m_settings->overlayDurationMs());
        return;
    }
    m_phase = Phase::Enter;
    m_fade = 0;
    m_travel = m_settings->overlayAnimation() == QLatin1String("slide") ? 1 : 0;
    emit motionChanged();
    m_motion->setEasingCurve(QEasingCurve::OutCubic);
    m_motion->setStartValue(0.0);
    m_motion->setEndValue(1.0);
    m_motion->start();
}

void OverlayService::beginExit()
{
    if (!m_showing || m_phase == Phase::Exit)
        return;
    if (m_settings->overlayAnimation() == QLatin1String("none")) {
        m_phase = Phase::Idle;
        m_showing = false;
        if (m_view)
            m_view->hide();
        presentNext();
        return;
    }
    m_phase = Phase::Exit;
    m_stoppingMotion = true;
    m_motion->stop();
    m_stoppingMotion = false;
    m_motion->setEasingCurve(QEasingCurve::InCubic);
    m_motion->setStartValue(0.0);
    m_motion->setEndValue(1.0);
    m_motion->start();
}

void OverlayService::ensureView()
{
    if (m_view)
        return;
    QQuickWindow::setDefaultAlphaBuffer(true);
    m_view = new QQuickView;
    m_view->QObject::setParent(this);
    m_view->setFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput
                     | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint);
    m_view->setColor(Qt::transparent);
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    m_view->setTitle(QString());
    m_view->engine()->rootContext()->setContextProperty(QStringLiteral("overlay"), this);
    m_view->setSource(QUrl(QStringLiteral("qrc:/qt/qml/NuClip/qml/overlay/CaptureToast.qml")));
    if (m_view->status() == QQuickView::Error) {
        for (const QQmlError &error : m_view->errors())
            Log::error(error.toString());
    }
}

void OverlayService::place()
{
    if (!m_view)
        return;
    const QVariantList monitors = m_devices->monitors();
    const int wanted = targetMonitor();
    QRect physical;
    for (const QVariant &value : monitors) {
        const QVariantMap item = value.toMap();
        if (item.value(QStringLiteral("index")).toInt() != wanted)
            continue;
        physical = QRect(item.value(QStringLiteral("x")).toInt(), item.value(QStringLiteral("y")).toInt(),
                         item.value(QStringLiteral("width")).toInt(), item.value(QStringLiteral("height")).toInt());
        break;
    }
    QScreen *screen = QGuiApplication::primaryScreen();
    int bestArea = 0;
    if (!physical.isEmpty()) {
        for (QScreen *candidate : QGuiApplication::screens()) {
            const qreal dpr = candidate->devicePixelRatio() > 0 ? candidate->devicePixelRatio() : 1;
            const QRect logical = candidate->geometry();
            const QRect pixels(qRound(logical.x() * dpr), qRound(logical.y() * dpr), qRound(logical.width() * dpr),
                               qRound(logical.height() * dpr));
            const QRect hit = pixels.intersected(physical);
            const int area = std::max(0, hit.width()) * std::max(0, hit.height());
            if (area > bestArea) {
                bestArea = area;
                screen = candidate;
            }
        }
    }
    if (!screen)
        return;
    const bool fullscreen = m_detector && m_detector->live() && m_detector->fullscreen();
    const QRect area = fullscreen ? screen->geometry() : screen->availableGeometry();
    const int width = cardWidth() + 28;
    const int height = cardHeight() + 28;
    const int margin = 18;
    int x = area.x() + margin;
    int y = area.y() + margin;
    if (m_anchorX == 1)
        x = area.x() + (area.width() - width) / 2;
    else if (m_anchorX == 2)
        x = area.x() + area.width() - width - margin;
    if (m_anchorY == 2)
        y = area.y() + area.height() - height - margin;
    else if (m_anchorY == 1)
        y = area.y() + (area.height() - height) / 2;
    m_view->setScreen(screen);
    m_view->setGeometry(x, y, width, height);
}

void OverlayService::applyNative()
{
    if (!m_view)
        return;
    const HWND hwnd = reinterpret_cast<HWND>(m_view->winId());
    if (!hwnd)
        return;
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    style |= WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    style &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, style);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
}

void OverlayService::readAnchor()
{
    const QString position = m_settings->overlayPosition();
    if (position.startsWith(QLatin1String("bottom")))
        m_anchorY = 2;
    else if (position.startsWith(QLatin1String("top")))
        m_anchorY = 0;
    else
        m_anchorY = 1;
    if (position.endsWith(QLatin1String("Left")))
        m_anchorX = 0;
    else if (position.endsWith(QLatin1String("Center")))
        m_anchorX = 1;
    else
        m_anchorX = 2;
    if (m_anchorY == 1)
        m_shiftX = m_anchorX == 0 ? -12 : 12;
    else if (m_anchorX == 0)
        m_shiftX = -10;
    else if (m_anchorX == 2)
        m_shiftX = 10;
    else
        m_shiftX = 0;
    if (m_anchorY == 1)
        m_shiftY = 0;
    else if (m_anchorX == 1)
        m_shiftY = m_anchorY == 0 ? -10 : 10;
    else
        m_shiftY = m_anchorY == 0 ? -8 : 8;
}

void OverlayService::publish(const Note &note)
{
    m_title = note.title;
    m_detail = note.detail;
    m_kind = note.kind;
    emit contentChanged();
}

bool OverlayService::allowed(const Note &note) const
{
    if (!m_settings->overlayEnabled())
        return false;
    if (note.kind == QLatin1String("clip"))
        return m_settings->overlayClip();
    if (note.kind == QLatin1String("shot"))
        return m_settings->overlayScreenshot();
    if (note.kind == QLatin1String("record"))
        return note.event == QLatin1String("stop") ? m_settings->overlayRecordStop() : m_settings->overlayRecordStart();
    if (note.kind == QLatin1String("error"))
        return m_settings->overlayError();
    return false;
}

int OverlayService::targetMonitor() const
{
    if (m_detector && m_detector->live())
        return std::max(0, m_detector->monitorIndex());
    return std::max(0, m_settings->monitorIndex());
}

QString OverlayService::appName() const
{
    if (m_detector && m_detector->live() && !m_detector->name().isEmpty())
        return m_detector->name();
    return QStringLiteral("Desktop");
}

void OverlayService::applyCount(Note *note)
{
    if (note->kind == QLatin1String("clip"))
        note->title = note->count <= 1 ? QStringLiteral("Clip saved") : QStringLiteral("%1 clips saved").arg(note->count);
    else if (note->kind == QLatin1String("shot"))
        note->title = note->count <= 1 ? QStringLiteral("Screenshot saved")
                                       : QStringLiteral("%1 screenshots saved").arg(note->count);
}

QString OverlayService::cleanError(const QString &error)
{
    QString text = error;
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text = text.simplified();
    if (text.size() > 84)
        text = text.left(81).trimmed() + QStringLiteral("...");
    return text;
}

} // namespace nuclip
