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

} // namespace WgcCapture
