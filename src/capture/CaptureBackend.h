#pragma once

#include "capture/CaptureResult.h"

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
    // Captures the window that is active at the freeze, without picking
    // (F-42, Alt + Print): a global hotkey is genuine user interaction
    // (D-011). Falls back to picking where there is no active window.
    virtual void captureActiveWindow() { captureWindow(); }
    // Lets the user drag a rectangle on the frozen screen (F-03).
    virtual void captureRegion() = 0;
    // Lets the user pick a monitor, or all, on the frozen screen (F-01).
    virtual void captureScreen() = 0;
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

private:
    bool m_regionByDragging = false;
    int m_delay = 0;
};

// Returns the backend for the platform the app runs on.
std::unique_ptr<CaptureBackend> createCaptureBackend(QObject *parent = nullptr);
