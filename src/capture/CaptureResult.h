#pragma once

#include <QImage>
#include <QRect>
#include <QString>

// What a capture backend delivers (docs/01-architecture.md): the raw image
// plus enough metadata for the shared core to crop and name it.
struct CaptureResult
{
    enum class Kind { Window, Region, Screen };
    Kind kind = Kind::Window;

    // For window captures: the whole window buffer, including transparent
    // shadow margins. The core crops it (imaging/WindowImage.h). For
    // region captures (F-03): exactly the selected part of the frozen screen;
    // screen captures (F-01): one monitor, or all monitors together.
    QImage image;

    QString title;  // window title, used for the file name
    QString appId;  // e.g. "org.gnome.Nautilus.desktop"

    // Geometry in logical pixels (desktop coordinates). The buffer contains
    // the frame plus the client-side shadow margins.
    QRect frameRect;
    QRect bufferRect;
    // Region captures: the selected rectangle.
    QRect regionRect;

    // Image pixels per logical pixel, e.g. 2.0 on a HiDPI screen.
    double scale = 1.0;

    // Corners the system rounds only when composing the screen, so the
    // capture shows them square (Windows 11, D-022): their radius in
    // logical px, 0 if the capture already has the window's real corners.
    double cornerRadius = 0;
};
