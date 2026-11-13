#pragma once

#include "capture/CaptureBackend.h"
#include "capture/CaptureOverlay.h"

#include <memory>

class Countdown;

namespace MacCapture { class Freeze; }

// Capture on macOS (D-002, D-047): freezes every monitor and every window
// with ScreenCaptureKit, lets the user pick a window or select a region on
// the frozen screen in the app's own overlay (as on Windows, D-022, D-023),
// and returns it as it was at the freeze. Needs the "Screen Recording"
// permission.
class MacBackend : public CaptureBackend
{
    Q_OBJECT

public:
    explicit MacBackend(QObject *parent = nullptr);
    ~MacBackend() override;

    bool isAvailable() const override { return true; }
    QString unavailableReason() const override { return {}; }
    void captureWindow() override;
    void captureActiveWindow() override;
    void captureRegion() override;
    void captureScreen() override;

private:
    // After the countdown, if a delay is set (F-04), then freeze and pick.
    // `activeWindow`: take the topmost window instead of picking.
    void start(CaptureOverlay::Mode mode, bool activeWindow = false);
    void freezeAndPick(CaptureOverlay::Mode mode, bool activeWindow);
    void onWindowPicked(int index);
    // A region or a whole monitor (or all), from the frozen monitors.
    void deliverPart(const QRect &native, CaptureResult::Kind kind);
    void finish();

    std::unique_ptr<MacCapture::Freeze> m_freeze;
    QList<CaptureOverlay::Screen> m_screens;
    QList<QRect> m_frames; // the windows' frames, native pixels
    CaptureOverlay *m_overlay = nullptr;
    Countdown *m_countdown = nullptr;
};
