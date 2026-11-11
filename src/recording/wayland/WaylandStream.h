#pragma once

#include <QImage>
#include <QList>
#include <QMargins>
#include <QObject>
#include <QRect>
#include <QTimer>

#include <cstdint>
#include <memory>
#include <vector>

class QScreen;
struct StreamSource;
struct wl_shm;
struct zwlr_screencopy_manager_v1;
struct hyprland_toplevel_export_manager_v1;

// The video stream on wlroots-style compositors (F-50, D-039): the same
// protocols as the freeze (WaylandFreeze, D-031), but one copy per frame
// and with the pointer drawn in (overlay_cursor). wlr-screencopy copies
// monitors, hyprland-toplevel-export one window (Hyprland only).
//
// Each requestFrame() asks the compositor for a copy of every source; when
// all are in, frameReady() delivers them as one picture. The shared-memory
// buffers are kept from frame to frame. The answers are read from a timer
// every few milliseconds (WaylandConnection::dispatchPending), so the app
// never waits for the compositor.
class WaylandStream : public QObject
{
    Q_OBJECT

public:
    explicit WaylandStream(QObject *parent = nullptr);
    ~WaylandStream() override;

    // Monitors: `native` are their rectangles in the overlay's physical
    // desktop pixels (PickGeometry::nativeLayout); with several, the
    // picture is the rectangle around all of them.
    bool startScreens(const QList<QScreen *> &screens, const QList<QRect> &native);
    // A window by its Hyprland handle; `margins` (pixels) are cut off each
    // frame: the shadow a client draws around its window (D-004).
    bool startWindow(uint32_t handle, const QMargins &margins);
    void stop();
    bool isRunning() const { return m_running; }

    // Asks for the next frame; ignored while one is still on its way.
    void requestFrame();

signals:
    void frameReady(const QImage &frame);
    // The copies kept failing, e.g. because the window was closed.
    void ended();

private:
    bool bindManagers(bool window);
    void poll();
    void onSourceDone();
    void release();

    std::vector<std::unique_ptr<StreamSource>> m_sources;
    QRect m_area;          // screens: the rectangle around all of them
    QMargins m_margins;    // window: cut off each frame
    bool m_window = false;
    bool m_running = false;
    bool m_waiting = false;
    int m_failures = 0;    // frames in a row that failed
    QTimer m_poll;

    wl_shm *m_shm = nullptr;
    zwlr_screencopy_manager_v1 *m_screencopy = nullptr;
    uint32_t m_screencopyVersion = 0;
    hyprland_toplevel_export_manager_v1 *m_toplevelExport = nullptr;
};
