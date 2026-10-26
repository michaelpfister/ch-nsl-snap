#pragma once

#include "capture/CaptureResult.h"
#include "capture/GlobalHotkeys.h"

#include <QObject>

#include <memory>

// Interface for the per-platform capture code (docs/01-architecture.md).
// Every capture is interactive: the backend freezes the screen, lets the
// user select (D-011), then emits exactly one of captured(), cancelled() or
// failed().
class CaptureBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // False if this platform has no backend, or it is not ready (e.g. the
    // GNOME Shell extension is not running). unavailableReason() says why.
    virtual bool isAvailable() const = 0;
    virtual QString unavailableReason() const = 0;

    // Lets the user pick a window and captures it (F-02).
    virtual void captureWindow() = 0;
    // Lets the user drag a rectangle on the frozen screen (F-03).
    virtual void captureRegion() = 0;
    // Lets the user pick a monitor, or all, on the frozen screen (F-01).
    virtual void captureScreen() = 0;
    // For the global hotkeys (F-42): without the overlay, the window that
    // has the focus, or the monitor under the pointer. The default shows
    // the overlay.
    virtual void captureActiveWindow() { captureWindow(); }
    virtual void captureScreenAtPointer() { captureScreen(); }

    // Global hotkeys and the preferences for captures they start (F-42).
    // Windows registers the keys and emits hotkeyPressed(); on GNOME the
    // extension has the keys and captures itself, see takeHotkeyCapture().
    virtual void setHotkeyOptions(const HotkeyOptions &options) { Q_UNUSED(options); }
    // Why some hotkeys do not work, for the settings; empty if they do.
    virtual QString hotkeyProblem() const { return {}; }
    // GNOME: the extension has a hotkey capture ready; fetch it and emit
    // captured() (or failed()).
    virtual void takeHotkeyCapture() {}

    // False where region and screen capture are not written yet.
    virtual bool supportsRegion() const { return true; }

    // How a region is selected (D-023): two clicks, or dragging.
    void setRegionByDragging(bool enabled) { m_regionByDragging = enabled; }
    bool regionByDragging() const { return m_regionByDragging; }

    // Seconds of countdown before the freeze (F-04, D-025); 0 = at once.
    void setDelay(int seconds) { m_delay = seconds; }
    int delay() const { return m_delay; }

signals:
    void captured(const CaptureResult &result);
    void cancelled();
    void failed(const QString &message);
    // A global hotkey was pressed (Windows); the app starts the capture.
    void hotkeyPressed(HotkeyAction action);

private:
    bool m_regionByDragging = false;
    int m_delay = 0;
};

// Returns the backend for the platform the app runs on.
std::unique_ptr<CaptureBackend> createCaptureBackend(QObject *parent = nullptr);
