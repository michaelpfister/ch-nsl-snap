#pragma once

#include "capture/CaptureBackend.h"
#include "capture/CaptureOverlay.h"
#include "capture/wayland/HyprlandIpc.h"

#include <QMargins>

class Countdown;

// Capture on Wayland desktops other than GNOME that offer wlr-screencopy
// (Hyprland, Sway and other wlroots-based ones; D-031). Freezes every
// monitor, and on Hyprland every visible window, then lets the user pick
// in the app's own overlay, as on Windows (D-022). Window capture needs
// Hyprland: only it offers a window list (its control socket) and window
// contents (hyprland-toplevel-export) to normal apps.
class WaylandBackend : public CaptureBackend
{
    Q_OBJECT

public:
    explicit WaylandBackend(QObject *parent = nullptr);
    ~WaylandBackend() override;

    // Whether the compositor offers what this backend needs.
    static bool isSupported();

    bool isAvailable() const override;
    QString unavailableReason() const override;
    void captureWindow() override;
    void captureRegion() override;
    void captureScreen() override;

    // What the user picked for a video (F-50, D-039): a window, or one or
    // all monitors.
    struct VideoPick
    {
        bool window = false;
        uint32_t handle = 0;     // window: for hyprland-toplevel-export
        QMargins margins;        // window: shadow around it in the picture, in pixels
        QString title;
        QList<QScreen *> screens; // monitors, with their rectangles in
        QList<QRect> native;      // physical desktop pixels
        QSize size;              // physical pixels of the picked window or monitors
    };

    // The same freeze and overlay as for an image, in video mode (no
    // region, no countdown before it). Answers with videoPicked(),
    // cancelled() or failed().
    void pickForVideo(bool window);
    bool canPickWindowsForVideo() const;

    // The window inside a toplevel-export picture, in logical pixels of the
    // picture; public for the tests. The picture is the window's surface:
    // usually exactly the window, but a client that draws its own shadow
    // (GTK when floating) has transparent margins around it. Then the
    // window is the rectangle of fully opaque pixels.
    static QRect frameInPicture(const QImage &picture, const QSize &windowSize, double scale);

private:
    void start(CaptureOverlay::Mode mode);
    void freezeAndPick(CaptureOverlay::Mode mode);
    void onWindowPicked(int index);
    void deliverPart(const QRect &native, CaptureResult::Kind kind);
    void finish();
    void deliverVideoScreens(const QRect &native);

    QList<CaptureOverlay::Screen> m_screens;
    // The pickable windows (the overlay's frames) and their pictures.
    QList<HyprlandIpc::Window> m_windows;
    QList<QImage> m_windowPictures;
    QList<double> m_windowScales;
    CaptureOverlay *m_overlay = nullptr;
    Countdown *m_countdown = nullptr;
    bool m_forVideo = false;

signals:
    void videoPicked(const WaylandBackend::VideoPick &pick);
};
