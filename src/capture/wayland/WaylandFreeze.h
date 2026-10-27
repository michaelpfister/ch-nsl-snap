#pragma once

#include <QImage>
#include <QList>
#include <QString>

#include <cstdint>

class QScreen;

// Freezes the screen on wlroots-style compositors (Hyprland, Sway, ...;
// D-031): one picture per monitor through wlr-screencopy, and on Hyprland
// one picture per window through hyprland-toplevel-export. All requests go
// out together, so the pictures show the same moment.
//
// The compositor copies into shared memory that the app provides (wl_shm);
// GPU buffers (dmabuf) are not used.
namespace WaylandFreeze {

// Whether the compositor offers the protocols.
bool canCaptureScreens();
bool canCaptureWindows();

struct Pictures
{
    QList<QImage> screens;  // one per requested screen, null where it failed
    QList<QImage> windows;  // one per requested window, null where it failed
};

// Captures `screens` and the windows with the Hyprland handles `windows`
// (the lower 32 bits of the address in `hyprctl clients`). Screens come
// back as RGB32, windows as ARGB32 with their alpha channel. Waits at most
// `timeoutMs`; a compositor that asks the user for permission first
// (Hyprland's "ecosystem:enforce_permissions") may need that long.
Pictures capture(const QList<QScreen *> &screens, const QList<uint32_t> &windows, int timeoutMs);

// The QImage format with the same memory layout as a wl_shm pixel format,
// or Format_Invalid if Qt has none. Wayland's formats are little-endian
// words: ARGB8888 is the bytes B, G, R, A, as Qt's ARGB32.
QImage::Format imageFormat(uint32_t shmFormat);

// Turns a copied buffer into a picture: alpha formats to ARGB32 (from
// premultiplied), the others to RGB32; flipped if the compositor says the
// rows are upside down (y_invert).
QImage toImage(const uchar *data, int width, int height, int stride, uint32_t shmFormat, bool yInvert,
               bool keepAlpha);

} // namespace WaylandFreeze
