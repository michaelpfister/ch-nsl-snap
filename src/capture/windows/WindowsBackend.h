#pragma once

#include "capture/CaptureBackend.h"
#include "capture/CaptureOverlay.h"

#include <memory>

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
    void captureRegion() override;

    // The radius Windows 11 gives the window's corners on the screen, in
    // logical px; the capture itself is square (D-022). 0 before Windows 11,
    // for maximised windows and when the app asks for square corners.
    static double cornerRadius(const WgcCapture::Window &window);

private:
    void start(CaptureOverlay::Mode mode);
    void onWindowPicked(int index);
    void onRegionSelected(const QRect &native);
    void finish();

    std::unique_ptr<WgcCapture::Freeze> m_freeze;
    QList<CaptureOverlay::Screen> m_screens;
    CaptureOverlay *m_overlay = nullptr;
};
