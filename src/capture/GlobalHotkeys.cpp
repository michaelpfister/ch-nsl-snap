#include "capture/GlobalHotkeys.h"

QKeySequence GlobalHotkeys::key(HotkeyAction action) const
{
    switch (action) {
    case HotkeyAction::Last: return last;
    case HotkeyAction::Screen: return screen;
    case HotkeyAction::Window: return window;
    case HotkeyAction::Region: return region;
    }
    return {};
}

void GlobalHotkeys::setKey(HotkeyAction action, const QKeySequence &key)
{
    switch (action) {
    case HotkeyAction::Last: last = key; break;
    case HotkeyAction::Screen: screen = key; break;
    case HotkeyAction::Window: window = key; break;
    case HotkeyAction::Region: region = key; break;
    }
}

namespace HotkeyKeys {

namespace {

// Windows constants (winuser.h), so this file builds and is tested on
// every platform.
constexpr unsigned kModAlt = 0x0001;
constexpr unsigned kModControl = 0x0002;
constexpr unsigned kModShift = 0x0004;
constexpr unsigned kModWin = 0x0008;
constexpr unsigned kVkSnapshot = 0x2C;
constexpr unsigned kVkPause = 0x13;
constexpr unsigned kVkF1 = 0x70;

} // namespace

QString gnomeAccelerator(const QKeySequence &key)
{
    if (key.isEmpty())
        return {};
    const QKeyCombination combination = key[0];
    const Qt::Key k = combination.key();
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    QString text;
    // GTK accelerator syntax, the one Mutter's keybindings read.
    if (modifiers & Qt::ShiftModifier)
        text += QStringLiteral("<Shift>");
    if (modifiers & Qt::ControlModifier)
        text += QStringLiteral("<Control>");
    if (modifiers & Qt::AltModifier)
        text += QStringLiteral("<Alt>");
    if (modifiers & Qt::MetaModifier)
        text += QStringLiteral("<Super>");
    if (k == Qt::Key_Print)
        text += QStringLiteral("Print");
    else if (k == Qt::Key_Pause)
        text += QStringLiteral("Pause");
    else if (k >= Qt::Key_F1 && k <= Qt::Key_F12)
        text += QStringLiteral("F%1").arg(int(k - Qt::Key_F1) + 1);
    else if (k >= Qt::Key_A && k <= Qt::Key_Z)
        text += QChar(u'a' + (k - Qt::Key_A)); // GTK names letters in lower case
    else if (k >= Qt::Key_0 && k <= Qt::Key_9)
        text += QChar(u'0' + (k - Qt::Key_0));
    else
        return {};
    return text;
}

bool windowsHotkey(const QKeySequence &key, unsigned *modifiers, unsigned *virtualKey)
{
    if (key.isEmpty())
        return false;
    const QKeyCombination combination = key[0];
    const Qt::Key k = combination.key();
    const Qt::KeyboardModifiers mods = combination.keyboardModifiers();
    unsigned vk = 0;
    if (k == Qt::Key_Print)
        vk = kVkSnapshot;
    else if (k == Qt::Key_Pause)
        vk = kVkPause;
    else if (k >= Qt::Key_F1 && k <= Qt::Key_F12)
        vk = kVkF1 + unsigned(k - Qt::Key_F1);
    else if ((k >= Qt::Key_A && k <= Qt::Key_Z) || (k >= Qt::Key_0 && k <= Qt::Key_9))
        vk = unsigned(k); // Qt's codes are the ASCII codes, like Windows'
    else
        return false;
    unsigned m = 0;
    if (mods & Qt::AltModifier)
        m |= kModAlt;
    if (mods & Qt::ControlModifier)
        m |= kModControl;
    if (mods & Qt::ShiftModifier)
        m |= kModShift;
    if (mods & Qt::MetaModifier)
        m |= kModWin;
    *modifiers = m;
    *virtualKey = vk;
    return true;
}

QList<Qt::Key> offeredKeys()
{
    QList<Qt::Key> keys = {Qt::Key_Print, Qt::Key_Pause};
    for (int k = Qt::Key_F1; k <= Qt::Key_F12; ++k)
        keys << Qt::Key(k);
    for (int k = Qt::Key_A; k <= Qt::Key_Z; ++k)
        keys << Qt::Key(k);
    for (int k = Qt::Key_0; k <= Qt::Key_9; ++k)
        keys << Qt::Key(k);
    return keys;
}

} // namespace HotkeyKeys
