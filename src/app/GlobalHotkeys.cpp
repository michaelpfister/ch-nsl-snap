#include "app/GlobalHotkeys.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QOperatingSystemVersion>
#include <QSettings>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef Q_OS_WIN
namespace {

// RegisterHotKey ids of the app; any value below 0xC000 that no other
// hotkey of this thread uses (Countdown registers on its own window).
constexpr int kFirstId = 0x4E10;

int hotkeyId(GlobalHotkeys::Action action)
{
    return kFirstId + int(action);
}

} // namespace
#endif

GlobalHotkeys::GlobalHotkeys(QObject *parent)
    : QObject(parent)
{
    // WM_HOTKEY of keys registered without a window goes to the thread's
    // message queue; Qt hands every message to the native event filters.
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalHotkeys::~GlobalHotkeys()
{
    for (Action action : actions())
        unregisterKey(action);
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool GlobalHotkeys::isSupported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QList<GlobalHotkeys::Action> GlobalHotkeys::actions()
{
    return {Action::LastMode, Action::Screen, Action::ActiveWindow, Action::Region};
}

QKeySequence GlobalHotkeys::defaultKey(Action action)
{
    switch (action) {
    case Action::LastMode: return QKeySequence(Qt::Key_Print);
    case Action::Screen: return QKeySequence(Qt::SHIFT | Qt::Key_Print);
    case Action::ActiveWindow: return QKeySequence(Qt::ALT | Qt::Key_Print);
    case Action::Region: return QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Print);
    }
    return {};
}

QString GlobalHotkeys::actionName(Action action)
{
    switch (action) {
    case Action::LastMode: return tr("Zuletzt verwendete Aufnahmeart");
    case Action::Screen: return tr("Bildschirm");
    case Action::ActiveWindow: return tr("Aktives Fenster, ohne Auswahl");
    case Action::Region: return tr("Bereich");
    }
    return {};
}

bool GlobalHotkeys::setKey(Action action, const QKeySequence &key)
{
    unregisterKey(action);
    m_keys[action] = key;
    const bool ok = key.isEmpty() || m_suspended || registerKey(action);
    emit changed();
    return ok;
}

bool GlobalHotkeys::isTaken(Action action) const
{
    return isSupported() && !m_suspended && !m_keys.value(action).isEmpty() && !m_registered.value(action);
}

void GlobalHotkeys::setSuspended(bool suspended)
{
    if (suspended == m_suspended)
        return;
    m_suspended = suspended;
    for (Action action : actions()) {
        if (suspended)
            unregisterKey(action);
        else if (!m_keys.value(action).isEmpty())
            registerKey(action);
    }
    emit changed();
}

bool GlobalHotkeys::printKeyOpensSnippingTool()
{
#ifdef Q_OS_WIN
    // The switch "Use the Print screen key to open screen capture". Without
    // the value Windows uses its default: on since Windows 11 22H2 (2023),
    // off on Windows 10.
    const QSettings keyboard(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Keyboard"), QSettings::NativeFormat);
    const QVariant value = keyboard.value(QStringLiteral("PrintScreenKeyForSnippingEnabled"));
    if (value.isValid())
        return value.toInt() != 0;
    return QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows11;
#else
    return false;
#endif
}

void GlobalHotkeys::openPrintKeySetting()
{
    // Settings > Accessibility > Keyboard.
    QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:easeofaccess-keyboard")));
}

bool GlobalHotkeys::toNative(const QKeySequence &key, unsigned &modifiers, unsigned &virtualKey)
{
#ifdef Q_OS_WIN
    if (key.count() != 1)
        return false;
    const QKeyCombination combination = key[0];
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    const int k = combination.key();

    modifiers = 0;
    if (mods & Qt::ShiftModifier)
        modifiers |= MOD_SHIFT;
    if (mods & Qt::ControlModifier)
        modifiers |= MOD_CONTROL;
    if (mods & Qt::AltModifier)
        modifiers |= MOD_ALT;
    if (mods & Qt::MetaModifier)
        modifiers |= MOD_WIN;

    // Letters and digits have the same codes as their ASCII characters.
    if ((k >= Qt::Key_A && k <= Qt::Key_Z) || (k >= Qt::Key_0 && k <= Qt::Key_9)) {
        virtualKey = unsigned(k);
        return true;
    }
    if (k >= Qt::Key_F1 && k <= Qt::Key_F24) {
        virtualKey = VK_F1 + unsigned(k - Qt::Key_F1);
        return true;
    }
    switch (k) {
    case Qt::Key_Print: virtualKey = VK_SNAPSHOT; return true;
    case Qt::Key_Pause: virtualKey = VK_PAUSE; return true;
    case Qt::Key_ScrollLock: virtualKey = VK_SCROLL; return true;
    case Qt::Key_Insert: virtualKey = VK_INSERT; return true;
    case Qt::Key_Delete: virtualKey = VK_DELETE; return true;
    case Qt::Key_Home: virtualKey = VK_HOME; return true;
    case Qt::Key_End: virtualKey = VK_END; return true;
    case Qt::Key_PageUp: virtualKey = VK_PRIOR; return true;
    case Qt::Key_PageDown: virtualKey = VK_NEXT; return true;
    case Qt::Key_Space: virtualKey = VK_SPACE; return true;
    case Qt::Key_Left: virtualKey = VK_LEFT; return true;
    case Qt::Key_Right: virtualKey = VK_RIGHT; return true;
    case Qt::Key_Up: virtualKey = VK_UP; return true;
    case Qt::Key_Down: virtualKey = VK_DOWN; return true;
    default: return false;
    }
#else
    Q_UNUSED(key)
    Q_UNUSED(modifiers)
    Q_UNUSED(virtualKey)
    return false;
#endif
}

bool GlobalHotkeys::registerKey(Action action)
{
    m_registered[action] = false;
#ifdef Q_OS_WIN
    unsigned modifiers = 0;
    unsigned virtualKey = 0;
    if (!toNative(m_keys.value(action), modifiers, virtualKey))
        return false;
    // Without a window: WM_HOTKEY goes to this thread's queue. MOD_NOREPEAT:
    // holding the key down does not start a capture after the other.
    m_registered[action] = RegisterHotKey(nullptr, hotkeyId(action), modifiers | MOD_NOREPEAT, virtualKey) != 0;
#endif
    return m_registered[action];
}

void GlobalHotkeys::unregisterKey(Action action)
{
#ifdef Q_OS_WIN
    if (m_registered.value(action))
        UnregisterHotKey(nullptr, hotkeyId(action));
#endif
    m_registered[action] = false;
}

bool GlobalHotkeys::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<const MSG *>(message);
    if (eventType != "windows_generic_MSG" || msg->message != WM_HOTKEY || msg->hwnd != nullptr)
        return false;
    for (Action action : actions()) {
        if (msg->wParam == WPARAM(hotkeyId(action)) && m_registered.value(action)) {
            // Qt passes no result for messages without a window.
            if (result)
                *result = 0;
            emit triggered(action);
            return true;
        }
    }
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}
