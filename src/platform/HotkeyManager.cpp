#include "platform/HotkeyManager.h"

#include "core/Settings.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QWindow>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace nuclip {
namespace {

struct ActionDef {
    const char *id;
    int hotkeyId;
};

const ActionDef kActions[] = {
    {"clip", 1},
    {"clip15", 2},
    {"clip30", 3},
    {"clip60", 4},
    {"clip120", 5},
    {"screenshot", 6},
    {"toggle", 7},
    {"pause", 8},
};

struct NamedKey {
    const char *token;
    UINT vk;
};

const NamedKey kNamed[] = {
    {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4}, {"F5", VK_F5}, {"F6", VK_F6},
    {"F7", VK_F7}, {"F8", VK_F8}, {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
    {"F13", VK_F13}, {"F14", VK_F14}, {"F15", VK_F15}, {"F16", VK_F16}, {"F17", VK_F17}, {"F18", VK_F18},
    {"F19", VK_F19}, {"F20", VK_F20}, {"F21", VK_F21}, {"F22", VK_F22}, {"F23", VK_F23}, {"F24", VK_F24},
    {"Space", VK_SPACE}, {"Enter", VK_RETURN}, {"Tab", VK_TAB}, {"Backspace", VK_BACK}, {"Delete", VK_DELETE},
    {"Insert", VK_INSERT}, {"Home", VK_HOME}, {"End", VK_END}, {"PageUp", VK_PRIOR}, {"PageDown", VK_NEXT},
    {"Left", VK_LEFT}, {"Right", VK_RIGHT}, {"Up", VK_UP}, {"Down", VK_DOWN}, {"Escape", VK_ESCAPE},
    {"CapsLock", VK_CAPITAL}, {"NumLock", VK_NUMLOCK}, {"ScrollLock", VK_SCROLL},
    {"Num0", VK_NUMPAD0}, {"Num1", VK_NUMPAD1}, {"Num2", VK_NUMPAD2}, {"Num3", VK_NUMPAD3}, {"Num4", VK_NUMPAD4},
    {"Num5", VK_NUMPAD5}, {"Num6", VK_NUMPAD6}, {"Num7", VK_NUMPAD7}, {"Num8", VK_NUMPAD8}, {"Num9", VK_NUMPAD9},
    {"NumAdd", VK_ADD}, {"NumSubtract", VK_SUBTRACT}, {"NumMultiply", VK_MULTIPLY}, {"NumDivide", VK_DIVIDE},
    {"NumDecimal", VK_DECIMAL}, {"Minus", VK_OEM_MINUS}, {"Equal", VK_OEM_PLUS}, {"Comma", VK_OEM_COMMA},
    {"Period", VK_OEM_PERIOD}, {"Slash", VK_OEM_2}, {"Backslash", VK_OEM_5}, {"Semicolon", VK_OEM_1},
    {"Quote", VK_OEM_7}, {"BracketLeft", VK_OEM_4}, {"BracketRight", VK_OEM_6}, {"Backtick", VK_OEM_3},
};

bool isModifierVk(UINT vk)
{
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU
        || vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LWIN || vk == VK_RWIN;
}

UINT modifierBit(UINT vk)
{
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)
        return MOD_CONTROL;
    if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)
        return MOD_ALT;
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
        return MOD_SHIFT;
    if (vk == VK_LWIN || vk == VK_RWIN)
        return MOD_WIN;
    return 0;
}

QString actionTitle(const QString &action)
{
    if (action == QLatin1String("clip"))
        return QStringLiteral("Save clip");
    if (action == QLatin1String("clip15"))
        return QStringLiteral("Save 15 seconds");
    if (action == QLatin1String("clip30"))
        return QStringLiteral("Save 30 seconds");
    if (action == QLatin1String("clip60"))
        return QStringLiteral("Save 60 seconds");
    if (action == QLatin1String("clip120"))
        return QStringLiteral("Save 120 seconds");
    if (action == QLatin1String("screenshot"))
        return QStringLiteral("Screenshot");
    if (action == QLatin1String("toggle"))
        return QStringLiteral("Start or stop recording");
    if (action == QLatin1String("pause"))
        return QStringLiteral("Pause or resume");
    return action;
}

QString tokenForVk(UINT vk)
{
    for (const NamedKey &key : kNamed) {
        if (key.vk == vk)
            return QString::fromLatin1(key.token);
    }
    if (vk >= 'A' && vk <= 'Z')
        return QString(QChar(char(vk)));
    if (vk >= '0' && vk <= '9')
        return QString(QChar(char(vk)));
    return {};
}

UINT vkForToken(const QString &token)
{
    const QString key = token.trimmed();
    for (const NamedKey &named : kNamed) {
        if (key.compare(QLatin1String(named.token), Qt::CaseInsensitive) == 0)
            return named.vk;
    }
    if (key.compare(QLatin1String("Esc"), Qt::CaseInsensitive) == 0)
        return VK_ESCAPE;
    if (key.compare(QLatin1String("PgUp"), Qt::CaseInsensitive) == 0)
        return VK_PRIOR;
    if (key.compare(QLatin1String("PgDn"), Qt::CaseInsensitive) == 0)
        return VK_NEXT;
    if (key.compare(QLatin1String("Del"), Qt::CaseInsensitive) == 0)
        return VK_DELETE;
    if (key.size() == 1) {
        const SHORT scanned = VkKeyScanW(key.at(0).unicode());
        if (scanned != -1)
            return UINT(LOBYTE(scanned));
    }
    return 0;
}

UINT modsForToken(const QString &token)
{
    const QString key = token.trimmed().toLower();
    if (key == QLatin1String("ctrl") || key == QLatin1String("control"))
        return MOD_CONTROL;
    if (key == QLatin1String("alt"))
        return MOD_ALT;
    if (key == QLatin1String("shift"))
        return MOD_SHIFT;
    if (key == QLatin1String("win") || key == QLatin1String("meta"))
        return MOD_WIN;
    return 0;
}

QString modsPrefix(UINT mods)
{
    QStringList parts;
    if (mods & MOD_CONTROL)
        parts << QStringLiteral("Ctrl");
    if (mods & MOD_ALT)
        parts << QStringLiteral("Alt");
    if (mods & MOD_SHIFT)
        parts << QStringLiteral("Shift");
    if (mods & MOD_WIN)
        parts << QStringLiteral("Win");
    return parts.join(QLatin1Char('+'));
}

QString canonical(UINT mods, UINT vk, bool modifierOnly)
{
    const QString prefix = modsPrefix(mods);
    if (modifierOnly || vk == 0)
        return prefix;
    const QString key = tokenForVk(vk);
    if (key.isEmpty())
        return {};
    return prefix.isEmpty() ? key : prefix + QLatin1Char('+') + key;
}

bool parseSequence(const QString &sequence, UINT &mods, UINT &vk, bool &modifierOnly, QString *error)
{
    mods = 0;
    vk = 0;
    modifierOnly = false;
    const QStringList parts = sequence.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (error)
            *error = QStringLiteral("Shortcut is empty.");
        return false;
    }
    QString keyToken;
    for (const QString &part : parts) {
        const UINT mod = modsForToken(part);
        if (mod)
            mods |= mod;
        else if (keyToken.isEmpty())
            keyToken = part.trimmed();
        else {
            if (error)
                *error = QStringLiteral("This shortcut has more than one key.");
            return false;
        }
    }
    if (keyToken.isEmpty()) {
        if (!mods) {
            if (error)
                *error = QStringLiteral("Shortcut is empty.");
            return false;
        }
        modifierOnly = true;
        return true;
    }
    vk = vkForToken(keyToken);
    if (!vk) {
        if (error)
            *error = QStringLiteral("NuClip does not recognize “%1”.").arg(keyToken);
        return false;
    }
    if (keyToken.size() == 1 && !keyToken.at(0).isLetterOrNumber()) {
        const SHORT scanned = VkKeyScanW(keyToken.at(0).unicode());
        if (scanned != -1 && (HIBYTE(scanned) & 1))
            mods |= MOD_SHIFT;
    }
    return true;
}

QString producedChar(UINT vk, UINT mods)
{
    BYTE state[256] = {};
    if (mods & MOD_SHIFT)
        state[VK_SHIFT] = 0x80;
    wchar_t text[8] = {};
    const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    const int count = ToUnicode(vk, scan, state, text, 8, 1 << 2);
    if (count != 1)
        return {};
    const QString value = QString::fromWCharArray(text, 1);
    if (value.at(0).isPrint() && !value.at(0).isSpace())
        return value;
    return {};
}

QString keyFace(UINT vk)
{
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return QString::number(int(vk - VK_NUMPAD0));
    switch (vk) {
    case VK_ADD: return QStringLiteral("+");
    case VK_SUBTRACT: return QStringLiteral("-");
    case VK_MULTIPLY: return QStringLiteral("*");
    case VK_DIVIDE: return QStringLiteral("/");
    case VK_DECIMAL: return QStringLiteral(".");
    default: break;
    }
    return producedChar(vk, 0);
}

QString pretty(const QString &sequence)
{
    UINT mods = 0;
    UINT vk = 0;
    bool modifierOnly = false;
    if (!parseSequence(sequence, mods, vk, modifierOnly, nullptr))
        return sequence;
    const QString prefix = modsPrefix(mods).replace(QLatin1Char('+'), QStringLiteral(" + "));
    if (modifierOnly || vk == 0)
        return prefix;
    if (vk >= 'A' && vk <= 'Z' && (mods & ~UINT(MOD_SHIFT)) == 0)
        return mods & MOD_SHIFT ? QStringLiteral("Shift + %1").arg(QChar(char(vk))) : QString(QChar(char(vk)));
    const QString face = keyFace(vk);
    if (!face.isEmpty() && (mods & ~UINT(MOD_SHIFT)) == 0) {
        const QString glyph = producedChar(vk, mods);
        return glyph.isEmpty() ? face : glyph;
    }
    const QString key = tokenForVk(vk);
    const QString shown = !face.isEmpty() ? face : (key == QLatin1String("Escape") ? QStringLiteral("Esc") : key);
    return prefix.isEmpty() ? shown : prefix + QStringLiteral(" + ") + shown;
}

UINT qtKeyToVk(int key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return VK_F1 + UINT(key - Qt::Key_F1);
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return UINT(key);
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return UINT(key);
    switch (key) {
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_CapsLock: return VK_CAPITAL;
    case Qt::Key_NumLock: return VK_NUMLOCK;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Plus: return VK_OEM_PLUS;
    case Qt::Key_Minus: return VK_OEM_MINUS;
    case Qt::Key_Equal: return VK_OEM_PLUS;
    case Qt::Key_Comma: return VK_OEM_COMMA;
    case Qt::Key_Period: return VK_OEM_PERIOD;
    case Qt::Key_Slash: return VK_OEM_2;
    case Qt::Key_Backslash: return VK_OEM_5;
    case Qt::Key_BracketLeft: return VK_OEM_4;
    case Qt::Key_BracketRight: return VK_OEM_6;
    case Qt::Key_Semicolon: return VK_OEM_1;
    case Qt::Key_Apostrophe: return VK_OEM_7;
    case Qt::Key_QuoteLeft: return VK_OEM_3;
    case Qt::Key_Control: return VK_CONTROL;
    case Qt::Key_Shift: return VK_SHIFT;
    case Qt::Key_Alt: return VK_MENU;
    case Qt::Key_Meta: return VK_LWIN;
    default: return 0;
    }
}

HotkeyManager *g_hookOwner = nullptr;

} // namespace

struct HookBridge {
    static LRESULT CALLBACK proc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && g_hookOwner) {
            if (g_hookOwner->onHook(UINT(wParam), reinterpret_cast<const void *>(lParam)))
                return 1;
        }
        return CallNextHookEx(nullptr, code, wParam, lParam);
    }
};

HotkeyManager::HotkeyManager(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    qApp->installNativeEventFilter(this);
    qApp->installEventFilter(this);
    connect(m_settings, &Settings::hotkeysChanged, this, &HotkeyManager::apply);
}

HotkeyManager::~HotkeyManager()
{
    qApp->removeNativeEventFilter(this);
    qApp->removeEventFilter(this);
    if (m_hook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_hook));
        m_hook = nullptr;
        g_hookOwner = nullptr;
    }
    if (m_hwnd) {
        for (const ActionDef &action : kActions)
            UnregisterHotKey(static_cast<HWND>(m_hwnd), action.hotkeyId);
    }
}

void HotkeyManager::setWindow(QWindow *window)
{
    m_hwnd = window ? reinterpret_cast<void *>(window->winId()) : nullptr;
    apply();
}

QString HotkeyManager::sequenceFor(const QString &action) const
{
    if (action == QLatin1String("clip"))
        return m_settings->hotkeyClip();
    if (action == QLatin1String("clip15"))
        return m_settings->hotkeyClip15();
    if (action == QLatin1String("clip30"))
        return m_settings->hotkeyClip30();
    if (action == QLatin1String("clip60"))
        return m_settings->hotkeyClip60();
    if (action == QLatin1String("clip120"))
        return m_settings->hotkeyClip120();
    if (action == QLatin1String("screenshot"))
        return m_settings->hotkeyScreenshot();
    if (action == QLatin1String("toggle"))
        return m_settings->hotkeyToggle();
    if (action == QLatin1String("pause"))
        return m_settings->hotkeyPause();
    return {};
}

void HotkeyManager::writeAction(const QString &action, const QString &sequence)
{
    if (action == QLatin1String("clip"))
        m_settings->setHotkeyClip(sequence);
    else if (action == QLatin1String("clip15"))
        m_settings->setHotkeyClip15(sequence);
    else if (action == QLatin1String("clip30"))
        m_settings->setHotkeyClip30(sequence);
    else if (action == QLatin1String("clip60"))
        m_settings->setHotkeyClip60(sequence);
    else if (action == QLatin1String("clip120"))
        m_settings->setHotkeyClip120(sequence);
    else if (action == QLatin1String("screenshot"))
        m_settings->setHotkeyScreenshot(sequence);
    else if (action == QLatin1String("toggle"))
        m_settings->setHotkeyToggle(sequence);
    else if (action == QLatin1String("pause"))
        m_settings->setHotkeyPause(sequence);
}

QString HotkeyManager::display(const QString &sequence) const
{
    return sequence.trimmed().isEmpty() ? QStringLiteral("Not set") : pretty(sequence);
}

QString HotkeyManager::ownerOf(const QString &sequence) const
{
    UINT mods = 0;
    UINT vk = 0;
    bool modifierOnly = false;
    if (!parseSequence(sequence, mods, vk, modifierOnly, nullptr))
        return {};
    const QString canon = canonical(mods, vk, modifierOnly);
    for (const ActionDef &action : kActions) {
        const QString id = QString::fromLatin1(action.id);
        UINT otherMods = 0;
        UINT otherVk = 0;
        bool otherOnly = false;
        const QString stored = sequenceFor(id).trimmed();
        if (stored.isEmpty() || !parseSequence(stored, otherMods, otherVk, otherOnly, nullptr))
            continue;
        if (canonical(otherMods, otherVk, otherOnly) == canon)
            return id;
    }
    return {};
}

void HotkeyManager::beginListen(const QString &action)
{
    m_listening = true;
    m_listeningAction = action;
    m_pending.clear();
    m_pendingConflict = false;
    m_liveMods = 0;
    m_sawPrimary = false;
    m_listeningText = QStringLiteral("Press any key combination...");
    m_errors.remove(action);
    emit errorsChanged();
    emit listenChanged();
}

void HotkeyManager::cancelListen()
{
    clearListen();
}

void HotkeyManager::clearListen()
{
    m_listening = false;
    m_listeningAction.clear();
    m_listeningText.clear();
    m_pending.clear();
    m_pendingConflict = false;
    m_liveMods = 0;
    emit listenChanged();
}

void HotkeyManager::setListeningText(const QString &text)
{
    if (m_listeningText == text)
        return;
    m_listeningText = text;
    emit listenChanged();
}

void HotkeyManager::finishCapture(unsigned mods, unsigned vk, bool modifierOnly)
{
    const QString sequence = canonical(mods, vk, modifierOnly);
    if (sequence.isEmpty()) {
        m_errors.insert(m_listeningAction, QStringLiteral("NuClip could not use that key."));
        emit errorsChanged();
        return;
    }
    m_pending = sequence;
    m_listeningText = pretty(sequence);
    const QString owner = ownerOf(sequence);
    m_pendingConflict = !owner.isEmpty() && owner != m_listeningAction;
    if (m_pendingConflict) {
        m_errors.insert(m_listeningAction, QStringLiteral("Already used by %1.").arg(actionTitle(owner)));
        emit errorsChanged();
        emit listenChanged();
        return;
    }
    m_errors.remove(m_listeningAction);
    const QString action = m_listeningAction;
    clearListen();
    writeAction(action, sequence);
    emit errorsChanged();
}

void HotkeyManager::commitPending(bool replace)
{
    if (!m_listening || m_pending.isEmpty())
        return;
    if (replace) {
        const QString owner = ownerOf(m_pending);
        if (!owner.isEmpty() && owner != m_listeningAction)
            writeAction(owner, QString());
    } else if (m_pendingConflict) {
        return;
    }
    m_errors.remove(m_listeningAction);
    const QString action = m_listeningAction;
    const QString sequence = m_pending;
    clearListen();
    writeAction(action, sequence);
    emit errorsChanged();
}

void HotkeyManager::commitModifiers()
{
    if (!m_listening || m_liveMods == 0)
        return;
    finishCapture(m_liveMods, 0, true);
}

void HotkeyManager::assignFromEvent(const QString &action, int key, int modifiers)
{
    beginListen(action);
    const UINT vk = qtKeyToVk(key);
    if (isModifierVk(vk))
        return;
    UINT mods = 0;
    if (modifiers & Qt::ControlModifier)
        mods |= MOD_CONTROL;
    if (modifiers & Qt::AltModifier)
        mods |= MOD_ALT;
    if (modifiers & Qt::ShiftModifier)
        mods |= MOD_SHIFT;
    if (modifiers & Qt::MetaModifier)
        mods |= MOD_WIN;
    finishCapture(mods, vk, false);
}

void HotkeyManager::clearAction(const QString &action)
{
    m_errors.remove(action);
    if (m_listeningAction == action)
        clearListen();
    writeAction(action, QString());
    emit errorsChanged();
}

bool HotkeyManager::eventFilter(QObject *, QEvent *event)
{
    if (!m_listening)
        return false;
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease && event->type() != QEvent::ShortcutOverride)
        return false;
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (event->type() == QEvent::ShortcutOverride)
        return true;
    if (keyEvent->isAutoRepeat())
        return true;
    UINT vk = UINT(keyEvent->nativeVirtualKey());
    if (!vk)
        vk = qtKeyToVk(keyEvent->key());
    const bool modifier = isModifierVk(vk);
    if (event->type() == QEvent::KeyRelease) {
        if (modifier)
            m_liveMods &= ~modifierBit(vk);
        if (m_pending.isEmpty()) {
            const QString prefix = modsPrefix(m_liveMods).replace(QLatin1Char('+'), QStringLiteral(" + "));
            setListeningText(prefix.isEmpty() ? QStringLiteral("Press any key combination...") : prefix + QStringLiteral(" + …"));
        }
        return true;
    }
    if (modifier) {
        m_liveMods |= modifierBit(vk);
        if (m_pending.isEmpty()) {
            const QString prefix = modsPrefix(m_liveMods).replace(QLatin1Char('+'), QStringLiteral(" + "));
            setListeningText(prefix + QStringLiteral(" + …"));
        }
        return true;
    }
    if (vk == VK_ESCAPE && m_liveMods == 0) {
        cancelListen();
        return true;
    }
    UINT mods = 0;
    if (keyEvent->modifiers() & Qt::ControlModifier)
        mods |= MOD_CONTROL;
    if (keyEvent->modifiers() & Qt::AltModifier)
        mods |= MOD_ALT;
    if (keyEvent->modifiers() & Qt::ShiftModifier)
        mods |= MOD_SHIFT;
    if (keyEvent->modifiers() & Qt::MetaModifier)
        mods |= MOD_WIN;
    finishCapture(mods, vk, false);
    return true;
}

void HotkeyManager::apply()
{
    if (m_hwnd) {
        for (const ActionDef &action : kActions)
            UnregisterHotKey(static_cast<HWND>(m_hwnd), action.hotkeyId);
    }
    if (m_hook) {
        UnhookWindowsHookEx(static_cast<HHOOK>(m_hook));
        m_hook = nullptr;
        g_hookOwner = nullptr;
    }
    m_registered.clear();
    m_hookActions.clear();
    QVariantMap nextErrors;
    QHash<QString, QString> owners;
    bool needHook = false;

    for (const ActionDef &action : kActions) {
        const QString id = QString::fromLatin1(action.id);
        const QString sequence = sequenceFor(id).trimmed();
        if (sequence.isEmpty())
            continue;
        UINT mods = 0;
        UINT vk = 0;
        bool modifierOnly = false;
        QString error;
        if (!parseSequence(sequence, mods, vk, modifierOnly, &error)) {
            nextErrors.insert(id, error);
            continue;
        }
        const QString canon = canonical(mods, vk, modifierOnly);
        if (owners.contains(canon)) {
            nextErrors.insert(id, QStringLiteral("Already used by %1.").arg(actionTitle(owners.value(canon))));
            continue;
        }
        owners.insert(canon, id);
        const bool reserved = vk == VK_DELETE && (mods & MOD_CONTROL) && (mods & MOD_ALT) && !(mods & MOD_SHIFT);
        if (reserved) {
            nextErrors.insert(id, QStringLiteral("Windows reserves Ctrl+Alt+Delete."));
            continue;
        }
        bool registered = false;
        if (!modifierOnly && m_hwnd) {
            registered = RegisterHotKey(static_cast<HWND>(m_hwnd), action.hotkeyId, mods | MOD_NOREPEAT, vk);
            if (registered)
                m_registered.insert(action.hotkeyId, id);
        }
        if (!registered) {
            m_hookActions.insert(canon, id);
            needHook = true;
        }
    }

    if (needHook) {
        g_hookOwner = this;
        m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookBridge::proc, GetModuleHandleW(nullptr), 0);
        if (!m_hook) {
            g_hookOwner = nullptr;
            for (auto it = m_hookActions.cbegin(); it != m_hookActions.cend(); ++it)
                nextErrors.insert(it.value(), QStringLiteral("Windows could not register this shortcut."));
            m_hookActions.clear();
        }
    }

    if (nextErrors != m_errors) {
        m_errors = nextErrors;
        emit errorsChanged();
    }
}

bool HotkeyManager::onHook(unsigned message, const void *data)
{
    if (m_listening || !data)
        return false;
    const auto *info = static_cast<const KBDLLHOOKSTRUCT *>(data);
    const UINT vk = info->vkCode;
    if (vk >= 256)
        return false;
    const bool up = message == WM_KEYUP || message == WM_SYSKEYUP || (info->flags & LLKHF_UP) != 0;
    if (up) {
        m_keyDown[vk] = false;
        if (!isModifierVk(vk)) {
            bool anyModifier = false;
            for (UINT other = 0; other < 256; ++other) {
                if (m_keyDown[other] && isModifierVk(other))
                    anyModifier = true;
            }
            if (!anyModifier)
                m_sawPrimary = false;
            return false;
        }
        if (!m_sawPrimary) {
            UINT mods = modifierBit(vk);
            for (UINT other = 0; other < 256; ++other) {
                if (m_keyDown[other])
                    mods |= modifierBit(other);
            }
            const QString action = m_hookActions.value(canonical(mods, 0, true));
            if (!action.isEmpty()) {
                QMetaObject::invokeMethod(this, [this, action] { emit triggered(action); }, Qt::QueuedConnection);
                return true;
            }
        }
        bool anyModifier = false;
        for (UINT other = 0; other < 256; ++other) {
            if (m_keyDown[other] && isModifierVk(other))
                anyModifier = true;
        }
        if (!anyModifier)
            m_sawPrimary = false;
        return false;
    }
    if (m_keyDown[vk])
        return false;
    m_keyDown[vk] = true;
    if (isModifierVk(vk))
        return false;
    m_sawPrimary = true;
    UINT mods = 0;
    for (UINT other = 0; other < 256; ++other) {
        if (m_keyDown[other])
            mods |= modifierBit(other);
    }
    const QString action = m_hookActions.value(canonical(mods, vk, false));
    if (action.isEmpty())
        return false;
    QMetaObject::invokeMethod(this, [this, action] { emit triggered(action); }, Qt::QueuedConnection);
    return true;
}

bool HotkeyManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (m_listening)
        return false;
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG")
        return false;
    const MSG *msg = static_cast<MSG *>(message);
    if (!msg || msg->message != WM_HOTKEY)
        return false;
    const QString action = m_registered.value(int(msg->wParam));
    if (action.isEmpty())
        return false;
    emit triggered(action);
    return true;
}

} // namespace nuclip
