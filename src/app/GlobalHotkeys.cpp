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

#ifdef Q_OS_MACOS
// Carbon's hotkeys still are the way to get a key without the
// accessibility permission. AssertMacros.h would define check() and
// verify() as macros, which clash with Qt.
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#include <Carbon/Carbon.h>

#include <QHash>
#endif

#ifdef Q_OS_MACOS
namespace {

// Identifies the app's hotkeys in Carbon's events ("NslS").
constexpr OSType kSignature = 0x4E736C53;

struct MacHotkey
{
    GlobalHotkeys *owner = nullptr;
    GlobalHotkeys::Action action = GlobalHotkeys::Action::LastMode;
    EventHotKeyRef ref = nullptr;
};

// Every registered key of every GlobalHotkeys object, by its id.
QHash<UInt32, MacHotkey> &macHotkeys()
{
    static QHash<UInt32, MacHotkey> hotkeys;
    return hotkeys;
}

// Carbon calls this on the main thread when a registered key is pressed.
OSStatus onMacHotkey(EventHandlerCallRef, EventRef event, void *)
{
    EventHotKeyID id = {};
    if (GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(id), nullptr, &id) != noErr
        || id.signature != kSignature)
        return eventNotHandledErr;
    const auto found = macHotkeys().constFind(id.id);
    if (found == macHotkeys().constEnd())
        return eventNotHandledErr;
    GlobalHotkeys *owner = found->owner;
    const GlobalHotkeys::Action action = found->action;
    // Queued: the handler returns before the capture starts; dropped if the
    // object is gone by then.
    QMetaObject::invokeMethod(owner, [owner, action] { emit owner->triggered(action); }, Qt::QueuedConnection);
    return noErr;
}

void installMacHandler()
{
    static bool installed = false;
    if (installed)
        return;
    const EventTypeSpec pressed = {kEventClassKeyboard, kEventHotKeyPressed};
    installed = InstallEventHandler(GetApplicationEventTarget(), NewEventHandlerUPP(onMacHotkey), 1, &pressed,
                                    nullptr, nullptr)
                == noErr;
}

// The key code that types `character` on the current keyboard layout,
// with or without Shift (a Swiss keyboard has Z and Y swapped, a French
// one the digits on Shift).
bool macKeyCodeFor(QChar character, unsigned &keyCode)
{
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    CFDataRef data = source ? static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData))
                            : nullptr;
    if (!data) {
        // Input methods (Japanese, ...) have no layout of their own.
        if (source)
            CFRelease(source);
        source = TISCopyCurrentASCIICapableKeyboardLayoutInputSource();
        data = source ? static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData))
                      : nullptr;
    }
    bool found = false;
    if (data) {
        const auto *layout = reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(data));
        for (const UInt32 modifiers : {UInt32(0), UInt32(shiftKey >> 8)}) {
            for (UInt16 code = 0; code < 128 && !found; ++code) {
                UInt32 deadKeys = 0;
                UniChar typed[4] = {};
                UniCharCount length = 0;
                const OSStatus status = UCKeyTranslate(layout, code, kUCKeyActionDisplay, modifiers, LMGetKbdType(),
                                                       1 << kUCKeyTranslateNoDeadKeysBit, &deadKeys, 4, &length, typed);
                if (status == noErr && length == 1 && QChar(typed[0]).toUpper() == character.toUpper()) {
                    keyCode = code;
                    found = true;
                }
            }
            if (found)
                break;
        }
    }
    if (source)
        CFRelease(source);
    return found;
}

// Keys that type no character: the same code on every layout.
bool macFixedKeyCode(int key, unsigned &keyCode)
{
    static const QHash<int, unsigned> codes = {
        {Qt::Key_F1, kVK_F1}, {Qt::Key_F2, kVK_F2}, {Qt::Key_F3, kVK_F3}, {Qt::Key_F4, kVK_F4},
        {Qt::Key_F5, kVK_F5}, {Qt::Key_F6, kVK_F6}, {Qt::Key_F7, kVK_F7}, {Qt::Key_F8, kVK_F8},
        {Qt::Key_F9, kVK_F9}, {Qt::Key_F10, kVK_F10}, {Qt::Key_F11, kVK_F11}, {Qt::Key_F12, kVK_F12},
        {Qt::Key_F13, kVK_F13}, {Qt::Key_F14, kVK_F14}, {Qt::Key_F15, kVK_F15}, {Qt::Key_F16, kVK_F16},
        {Qt::Key_F17, kVK_F17}, {Qt::Key_F18, kVK_F18}, {Qt::Key_F19, kVK_F19}, {Qt::Key_F20, kVK_F20},
        {Qt::Key_Space, kVK_Space}, {Qt::Key_Delete, kVK_ForwardDelete}, {Qt::Key_Home, kVK_Home},
        {Qt::Key_End, kVK_End}, {Qt::Key_PageUp, kVK_PageUp}, {Qt::Key_PageDown, kVK_PageDown},
        {Qt::Key_Left, kVK_LeftArrow}, {Qt::Key_Right, kVK_RightArrow}, {Qt::Key_Up, kVK_UpArrow},
        {Qt::Key_Down, kVK_DownArrow}, {Qt::Key_Help, kVK_Help},
    };
    const auto found = codes.constFind(key);
    if (found == codes.constEnd())
        return false;
    keyCode = *found;
    return true;
}

} // namespace
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
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
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
#ifdef Q_OS_MACOS
    // Cmd + Shift + 3, 4 and 5 are the system's own screenshot keys.
    switch (action) {
    case Action::LastMode: return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_6);
    case Action::Screen: return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_7);
    case Action::ActiveWindow: return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_8);
    case Action::Region: return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_9);
    }
    return {};
#endif
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
#elif defined(Q_OS_MACOS)
    if (key.count() != 1)
        return false;
    const QKeyCombination combination = key[0];
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    const int k = combination.key();

    // Qt swaps the names on macOS: ControlModifier is the Command key,
    // MetaModifier the Control key.
    modifiers = 0;
    if (mods & Qt::ShiftModifier)
        modifiers |= shiftKey;
    if (mods & Qt::ControlModifier)
        modifiers |= cmdKey;
    if (mods & Qt::AltModifier)
        modifiers |= optionKey;
    if (mods & Qt::MetaModifier)
        modifiers |= controlKey;

    if (macFixedKeyCode(k, virtualKey))
        return true;
    // A key that types a character: its code on the current layout.
    if (k >= Qt::Key_Exclam && k <= Qt::Key_ydiaeresis && k != Qt::Key_Space)
        return macKeyCodeFor(QChar(k), virtualKey);
    return false;
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
#elif defined(Q_OS_MACOS)
    unsigned modifiers = 0;
    unsigned keyCode = 0;
    if (!toNative(m_keys.value(action), modifiers, keyCode))
        return false;
    installMacHandler();
    static UInt32 nextId = 1;
    const EventHotKeyID id = {kSignature, nextId++};
    EventHotKeyRef ref = nullptr;
    // Exclusive: fails if another app has the same key registered.
    if (RegisterEventHotKey(keyCode, modifiers, id, GetApplicationEventTarget(), kEventHotKeyExclusive, &ref) == noErr) {
        macHotkeys().insert(id.id, {this, action, ref});
        m_macIds[action] = id.id;
        m_registered[action] = true;
    }
#endif
    return m_registered[action];
}

void GlobalHotkeys::unregisterKey(Action action)
{
#ifdef Q_OS_WIN
    if (m_registered.value(action))
        UnregisterHotKey(nullptr, hotkeyId(action));
#elif defined(Q_OS_MACOS)
    if (m_registered.value(action)) {
        const MacHotkey hotkey = macHotkeys().take(m_macIds.take(action));
        if (hotkey.ref)
            UnregisterEventHotKey(hotkey.ref);
    }
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
