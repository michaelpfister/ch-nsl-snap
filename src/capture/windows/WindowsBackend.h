#pragma once

#include "capture/CaptureBackend.h"
#include "capture/CaptureOverlay.h"

#include <memory>

class Countdown;

namespace WgcCapture { class Freeze; struct Window; }

// Capture on Windows (D-022, D-023): freezes every window with
// Windows.Graphics.Capture and the monitors with GDI, lets the user pick a
// window or select a region on the frozen screen in the app's own overlay,
// and returns it as it was at the freeze.
class WindowsBackend : public CaptureBackend
{
    Q_OBJECT

public:
    explicit WindowsBackend(QObject *parent = nullptr);
    ~WindowsBackend() override;

    bool isAvailable() const override;
    QString unavailableReason() const override;
    void captureWindow() override;
    void captureActiveWindow() override;
    void captureRegion() override;
    void captureScreen() override;

    // The radius Windows 11 gives the window's corners on the screen, in
    // logical px; the capture itself is square (D-022). 0 before Windows 11,
    // for maximised windows and when the app asks for square corners.
    static double cornerRadius(const WgcCapture::Window &window);

private:
    // After the countdown, if a delay is set (F-04), then freeze and pick.
    // `activeWindow`: take the foreground window instead of picking.
    void start(CaptureOverlay::Mode mode, bool activeWindow = false);
    void freezeAndPick(CaptureOverlay::Mode mode, bool activeWindow);
    void onWindowPicked(int index);
    // A region or a whole monitor (or all), from the frozen monitors.
    void deliverPart(const QRect &native, CaptureResult::Kind kind);
    void finish();

    std::unique_ptr<WgcCapture::Freeze> m_freeze;
    QList<CaptureOverlay::Screen> m_screens;
    CaptureOverlay *m_overlay = nullptr;
    Countdown *m_countdown = nullptr;
};
