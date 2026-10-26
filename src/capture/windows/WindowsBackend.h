#pragma once

#include "capture/CaptureBackend.h"

#include <memory>

class WindowPicker;
namespace WgcCapture { class Freeze; struct Window; }

// Window capture on Windows (D-022): freezes every window with
// Windows.Graphics.Capture, lets the user pick one on the frozen screen in
// the app's own overlay, and returns that window as it was at the freeze.
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
    bool supportsRegion() const override { return false; }

    // The radius Windows 11 gives the window's corners on the screen, in
    // logical px; the capture itself is square (D-022). 0 before Windows 11,
    // for maximised windows and when the app asks for square corners.
    static double cornerRadius(const WgcCapture::Window &window);

private:
    void onPicked(int index);
    void finish();

    std::unique_ptr<WgcCapture::Freeze> m_freeze;
    WindowPicker *m_picker = nullptr;
};
