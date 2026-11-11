#pragma once

// Window capture with Windows.Graphics.Capture (D-022). This file and its
// .cpp use no Qt: C++/WinRT and Qt's keyword macros do not mix well, so the
// WinRT code stays in its own translation unit and hands out plain data.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The Win32 window handle, without including <windows.h> in a header.
struct HWND__;
typedef HWND__ *HWND;

namespace WgcCapture {

// A rectangle in physical desktop pixels, like Win32's RECT.
struct Rect
{
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    int width() const { return right - left; }
    int height() const { return bottom - top; }
};

struct Window
{
    HWND hwnd = nullptr;
    std::wstring title;
    // The visible frame (DWM extended frame bounds), without the invisible
    // resize borders and without the shadow.
    Rect frame;
    // GetWindowRect: the frame plus the invisible resize borders.
    Rect windowRect;
    unsigned dpi = 96; // of the monitor the window is on
    bool maximized = false;
    // DWMWA_WINDOW_CORNER_PREFERENCE: 0 default, 1 square, 2 round,
    // 3 round small. Windows 11 rounds the corners only when composing.
    int cornerPreference = 0;
};

// An image as BGRA bytes with premultiplied alpha, rows without padding.
struct Pixels
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

// False before Windows 10 version 1903, or without graphics capture.
bool isSupported();

// The windows a user can pick, topmost first, like the Alt+Tab list:
// visible, not minimised, not cloaked (other virtual desktops), with a
// title, no tool windows, no desktop or taskbar.
std::vector<Window> listWindows();

// The freeze: one frame of every window, taken in parallel, so each
// window can later be read as it was at this moment, even if it was
// covered by others.
class Freeze
{
public:
    // Captures the windows of listWindows(). Windows that deliver no frame
    // within a second are left out.
    static std::unique_ptr<Freeze> capture();
    ~Freeze();

    const std::vector<Window> &windows() const;
    // The frozen image of windows()[index]: the whole captured surface, of
    // which the frame is a part (PickGeometry::frameInCapture).
    bool pixels(std::size_t index, Pixels &out) const;

private:
    Freeze();
    struct Impl;
    std::unique_ptr<Impl> d;
};

// The window's rectangles right now (they change when it is moved or
// resized); false if the window is gone.
bool windowRects(HWND hwnd, Rect &windowRect, Rect &frame);

// A continuous capture for videos (F-50, D-039): a window or a monitor,
// with the pointer, without the yellow border where Windows allows that.
// Windows sends a frame whenever the content changes; takeLatest() reads
// the newest one back. A window's buffers are as large as the desktop, so
// it may grow during the recording.
class Stream
{
public:
    // nullptr if the capture cannot start.
    static std::unique_ptr<Stream> forWindow(HWND hwnd);
    // The monitor that holds the point (physical desktop pixels).
    static std::unique_ptr<Stream> forMonitorAt(int x, int y);
    ~Stream();

    // The newest frame since the last call, as BGRA with premultiplied
    // alpha; false if none came since. A window frame is its whole
    // captured surface, as with Freeze::pixels().
    bool takeLatest(Pixels &out);
    // The window was closed (or the capture ended otherwise).
    bool isClosed() const;
    // "frames arrived 12, pool recreated 1", for the tests' log.
    std::string statistics() const;

    struct Impl;

private:
    Stream();
    std::unique_ptr<Impl> d;
};

} // namespace WgcCapture
