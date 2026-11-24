#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class QQuickView;
class QTimer;
class QVariantAnimation;

namespace nuclip {

class DeviceCatalog;
class GameDetector;
class RecordingEngine;
class Settings;
class Theme;

class OverlayService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY contentChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY contentChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY contentChanged)
    Q_PROPERTY(qreal fade READ fade NOTIFY motionChanged)
    Q_PROPERTY(qreal travel READ travel NOTIFY motionChanged)
    Q_PROPERTY(int shiftX READ shiftX NOTIFY layoutChanged)
    Q_PROPERTY(int shiftY READ shiftY NOTIFY layoutChanged)
    Q_PROPERTY(int anchorX READ anchorX NOTIFY layoutChanged)
    Q_PROPERTY(int anchorY READ anchorY NOTIFY layoutChanged)
    Q_PROPERTY(bool compact READ compact NOTIFY layoutChanged)
    Q_PROPERTY(bool large READ large NOTIFY layoutChanged)
    Q_PROPERTY(int cardWidth READ cardWidth NOTIFY layoutChanged)
    Q_PROPERTY(int cardHeight READ cardHeight NOTIFY layoutChanged)
    Q_PROPERTY(qreal panelOpacity READ panelOpacity NOTIFY layoutChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY layoutChanged)

public:
    explicit OverlayService(Settings *settings, Theme *theme, DeviceCatalog *devices, GameDetector *detector,
                            RecordingEngine *recording, QObject *parent = nullptr);

    QString title() const { return m_title; }
    QString detail() const { return m_detail; }
    QString kind() const { return m_kind; }
    qreal fade() const { return m_fade; }
    qreal travel() const { return m_travel; }
    int shiftX() const { return m_shiftX; }
    int shiftY() const { return m_shiftY; }
    int anchorX() const { return m_anchorX; }
    int anchorY() const { return m_anchorY; }
    bool compact() const;
    bool large() const;
    int cardWidth() const;
    int cardHeight() const;
    qreal panelOpacity() const;
    QColor accent() const;

    Q_INVOKABLE void preview();

signals:
    void contentChanged();
    void motionChanged();
    void layoutChanged();

private:
    struct Note {
        QString kind;
        QString event;
        QString title;
        QString detail;
        int count = 1;
        bool coalesce = false;
    };

    enum class Phase { Idle, Enter, Hold, Exit };

    void onClip(const QString &game, double seconds);
    void onScreenshot();
    void onError();
    void onRecordingState();
    void post(Note note);
    void trimQueue();
    void presentNext();
    void startEnter();
    void beginExit();
    void ensureView();
    void place();
    void applyNative();
    void readAnchor();
    void publish(const Note &note);
    bool allowed(const Note &note) const;
    int targetMonitor() const;
    QString appName() const;
    static void applyCount(Note *note);
    static QString cleanError(const QString &error);

    Settings *m_settings = nullptr;
    Theme *m_theme = nullptr;
    DeviceCatalog *m_devices = nullptr;
    GameDetector *m_detector = nullptr;
    RecordingEngine *m_recording = nullptr;
    QQuickView *m_view = nullptr;
    QVariantAnimation *m_motion = nullptr;
    QTimer *m_hold = nullptr;
    QList<Note> m_queue;
    QString m_title;
    QString m_detail;
    QString m_kind;
    qreal m_fade = 0;
    qreal m_travel = 1;
    int m_shiftX = 10;
    int m_shiftY = -8;
    int m_anchorX = 2;
    int m_anchorY = 0;
    Phase m_phase = Phase::Idle;
    bool m_showing = false;
    bool m_stoppingMotion = false;
    bool m_sessionLive = false;
    int m_lastState = 0;
};

} // namespace nuclip
