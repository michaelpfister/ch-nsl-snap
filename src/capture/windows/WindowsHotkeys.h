#pragma once

#include "capture/GlobalHotkeys.h"

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QStringList>

// Global hotkeys on Windows (F-42): RegisterHotKey without a window, so
// Windows posts WM_HOTKEY to the app's thread, where this native event
// filter picks it up.
class WindowsHotkeys : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WindowsHotkeys(QObject *parent = nullptr);
    ~WindowsHotkeys() override;

    // Replaces the registered keys. Returns a text for every key that
    // could not be registered (another program has it), empty if all work.
    QStringList setHotkeys(const GlobalHotkeys &hotkeys);
    void clear();

    // True if Windows opens the Snipping Tool on Print (Q-15).
    static bool printOpensSnippingTool();

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void pressed(HotkeyAction action);

private:
    QList<HotkeyAction> m_registered; // the hotkey id is the index + 1
};
