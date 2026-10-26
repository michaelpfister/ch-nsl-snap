#pragma once

#include "capture/CaptureResult.h"

#include <QKeySequence>
#include <QString>

// The global capture hotkeys (F-42, D-031), modelled on WinSnap.
enum class HotkeyAction {
    Last,   // Print: the mode used last, with the overlay
    Screen, // Shift + Print: the monitor under the pointer, directly
    Window, // Alt + Print: the active window, directly
    Region, // Ctrl + Alt + Print: a region, with the overlay
};

struct GlobalHotkeys
{
    bool enabled = true;
    // GNOME: take the same keys away from GNOME's own screenshot UI while
    // the extension runs (Q-14).
    bool takeOverSystemKeys = true;
    QKeySequence last = QKeySequence(Qt::Key_Print);
    QKeySequence screen = QKeySequence(Qt::SHIFT | Qt::Key_Print);
    QKeySequence window = QKeySequence(Qt::ALT | Qt::Key_Print);
    QKeySequence region = QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Print);

    QKeySequence key(HotkeyAction action) const;
    void setKey(HotkeyAction action, const QKeySequence &key);

    bool operator==(const GlobalHotkeys &) const = default;
};

// What a backend needs for captures started by a hotkey, also while the
// app's window is hidden or (GNOME) the app is not running.
struct HotkeyOptions
{
    GlobalHotkeys hotkeys;
    int delay = 0;                 // F-04
    bool regionByDragging = false; // D-023
    CaptureResult::Kind lastMode = CaptureResult::Kind::Window;
};

namespace HotkeyKeys {

// A GNOME accelerator, e.g. "<Shift>Print"; empty for an empty sequence.
QString gnomeAccelerator(const QKeySequence &key);

// Windows RegisterHotKey values (MOD_* and VK_*). False for keys that
// have no virtual key code here.
bool windowsHotkey(const QKeySequence &key, unsigned *modifiers, unsigned *virtualKey);

// The keys offered in the settings: Print, Pause, F1–F12, A–Z, 0–9.
QList<Qt::Key> offeredKeys();

} // namespace HotkeyKeys
