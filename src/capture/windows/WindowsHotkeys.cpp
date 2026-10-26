#include "capture/windows/WindowsHotkeys.h"

#include <QCoreApplication>
#include <QOperatingSystemVersion>
#include <QSettings>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

QString actionName(HotkeyAction action)
{
    switch (action) {
    case HotkeyAction::Last: return QCoreApplication::translate("WindowsHotkeys", "Zuletzt verwendete Art");
    case HotkeyAction::Screen: return QCoreApplication::translate("WindowsHotkeys", "Bildschirm");
    case HotkeyAction::Window: return QCoreApplication::translate("WindowsHotkeys", "Aktives Fenster");
    case HotkeyAction::Region: return QCoreApplication::translate("WindowsHotkeys", "Bereich");
    }
    return {};
}

} // namespace

WindowsHotkeys::WindowsHotkeys(QObject *parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WindowsHotkeys::~WindowsHotkeys()
{
    clear();
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
}

void WindowsHotkeys::clear()
{
    for (qsizetype i = 0; i < m_registered.size(); ++i)
        UnregisterHotKey(nullptr, int(i + 1));
    m_registered.clear();
}

QStringList WindowsHotkeys::setHotkeys(const GlobalHotkeys &hotkeys)
{
    clear();
    QStringList failed;
    if (!hotkeys.enabled)
        return failed;
    for (HotkeyAction action : {HotkeyAction::Last, HotkeyAction::Screen, HotkeyAction::Window, HotkeyAction::Region}) {
        unsigned modifiers = 0;
        unsigned key = 0;
        if (!HotkeyKeys::windowsHotkey(hotkeys.key(action), &modifiers, &key))
            continue; // off
        m_registered.append(action);
        const int id = int(m_registered.size());
        if (!RegisterHotKey(nullptr, id, modifiers | MOD_NOREPEAT, key)) {
            failed << QCoreApplication::translate("WindowsHotkeys", "%1 (%2) ist schon von einem anderen Programm belegt.")
                          .arg(actionName(action), hotkeys.key(action).toString(QKeySequence::NativeText));
        }
    }
    return failed;
}

bool WindowsHotkeys::printOpensSnippingTool()
{
    // "Use the Print screen key to open screen capture"; on by default
    // since Windows 11 22H2, so a missing value counts as on there.
    const QSettings keyboard(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Keyboard"), QSettings::NativeFormat);
    const QVariant value = keyboard.value(QStringLiteral("PrintScreenKeyForSnippingEnabled"));
    if (value.isValid())
        return value.toInt() != 0;
    return QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows11_22H2;
}

bool WindowsHotkeys::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "windows_generic_MSG")
        return false;
    const auto *msg = static_cast<const MSG *>(message);
    if (msg->message != WM_HOTKEY || msg->hwnd != nullptr)
        return false;
    const int index = int(msg->wParam) - 1;
    if (index < 0 || index >= m_registered.size())
        return false;
    emit pressed(m_registered[index]);
    if (result)
        *result = 0;
    return true;
}
