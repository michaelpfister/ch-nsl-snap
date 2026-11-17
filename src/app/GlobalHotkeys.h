#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QObject>

// Global hotkeys (F-42, D-035): keys that start a capture while the app is
// hidden or another program has the focus. On Windows they are registered
// with RegisterHotKey, on macOS with Carbon's RegisterEventHotKey (D-048);
// elsewhere isSupported() is false (on GNOME the Shell extension will own
// them, D-011).
class GlobalHotkeys : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    enum class Action { LastMode, Screen, ActiveWindow, Region };

    explicit GlobalHotkeys(QObject *parent = nullptr);
    ~GlobalHotkeys() override;

    static bool isSupported();
    // All actions, in the order of the settings.
    static QList<Action> actions();
    // WinSnap's keys: Print, Shift + Print, Alt + Print, Ctrl + Alt + Print.
    // Macs have no Print key: Cmd + Shift + 6, 7, 8 and 9 (Q-16; Qt calls
    // the Command key Ctrl on macOS).
    static QKeySequence defaultKey(Action action);
    // For the settings, e.g. "Bildschirm".
    static QString actionName(Action action);

    // Assigns `key` (one key with modifiers; empty = none) and registers it.
    // False if another program already uses the key; the key stays assigned
    // and shows as taken (isTaken).
    bool setKey(Action action, const QKeySequence &key);
    QKeySequence key(Action action) const { return m_keys.value(action); }
    // The key is assigned but another program had registered it first.
    bool isTaken(Action action) const;

    // Unregisters all keys for a while, e.g. while the user types a new key
    // in the settings (a registered key never reaches the app's windows).
    void setSuspended(bool suspended);

    // Windows 11 opens the Snipping Tool on Print by default. While that
    // setting is on, Print never reaches the app (D-035).
    static bool printKeyOpensSnippingTool();
    // Opens the page of the Windows settings that has that switch.
    static void openPrintKeySetting();

    // Converts a key to Win32's modifiers (MOD_*) and virtual key code, on
    // macOS to Carbon's modifiers (cmdKey, ...) and key code of the current
    // keyboard layout. False if it has no such key (e.g. more than one key).
    static bool toNative(const QKeySequence &key, unsigned &modifiers, unsigned &virtualKey);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void triggered(GlobalHotkeys::Action action);
    // Keys were assigned, or became free or taken.
    void changed();

private:
    bool registerKey(Action action);
    void unregisterKey(Action action);

    QMap<Action, QKeySequence> m_keys;
    QMap<Action, bool> m_registered; // RegisterHotKey succeeded
    QMap<Action, quint32> m_macIds;  // macOS: the id the key is registered with
    bool m_suspended = false;
};
