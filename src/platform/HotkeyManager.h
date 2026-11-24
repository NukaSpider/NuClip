#pragma once

#include <QAbstractNativeEventFilter>
#include <QHash>
#include <QObject>
#include <QVariantMap>

class QWindow;

namespace nuclip {

class Settings;

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(QVariantMap errors READ errors NOTIFY errorsChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listenChanged)
    Q_PROPERTY(QString listeningAction READ listeningAction NOTIFY listenChanged)
    Q_PROPERTY(QString listeningText READ listeningText NOTIFY listenChanged)
    Q_PROPERTY(QString pendingSequence READ pendingSequence NOTIFY listenChanged)
    Q_PROPERTY(bool pendingConflict READ pendingConflict NOTIFY listenChanged)

public:
    explicit HotkeyManager(Settings *settings, QObject *parent = nullptr);
    ~HotkeyManager() override;

    QVariantMap errors() const { return m_errors; }
    bool listening() const { return m_listening; }
    QString listeningAction() const { return m_listeningAction; }
    QString listeningText() const { return m_listeningText; }
    QString pendingSequence() const { return m_pending; }
    bool pendingConflict() const { return m_pendingConflict; }

    void setWindow(QWindow *window);

    Q_INVOKABLE void beginListen(const QString &action);
    Q_INVOKABLE void cancelListen();
    Q_INVOKABLE void commitPending(bool replace);
    Q_INVOKABLE void commitModifiers();
    Q_INVOKABLE void assignFromEvent(const QString &action, int key, int modifiers);
    Q_INVOKABLE void clearAction(const QString &action);
    Q_INVOKABLE QString display(const QString &sequence) const;

    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void triggered(const QString &action);
    void errorsChanged();
    void listenChanged();

private:
    struct Chord {
        unsigned mods = 0;
        unsigned vk = 0;
        bool modifierOnly = false;
    };

    void apply();
    void writeAction(const QString &action, const QString &sequence);
    QString sequenceFor(const QString &action) const;
    QString ownerOf(const QString &sequence) const;
    void finishCapture(unsigned mods, unsigned vk, bool modifierOnly);
    void setListeningText(const QString &text);
    void clearListen();
    bool onHook(unsigned message, const void *data);
    friend struct HookBridge;

    Settings *m_settings = nullptr;
    void *m_hwnd = nullptr;
    void *m_hook = nullptr;
    QVariantMap m_errors;
    QHash<int, QString> m_registered;
    QHash<QString, QString> m_hookActions;
    bool m_listening = false;
    QString m_listeningAction;
    QString m_listeningText;
    QString m_pending;
    bool m_pendingConflict = false;
    unsigned m_liveMods = 0;
    bool m_sawPrimary = false;
    bool m_keyDown[256] = {};
};

} // namespace nuclip
