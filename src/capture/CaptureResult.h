#pragma once

#include <QImage>
#include <QRect>
#include <QString>

// What a capture backend delivers (docs/01-architecture.md): the raw image
// plus enough metadata for the shared core to crop and name it.
struct CaptureResult
{
    // For window captures: the whole window buffer, including transparent
    // shadow margins. The core crops it (imaging/WindowImage.h).
    QImage image;

    QString title;  // window title, used for the file name
    QString appId;  // e.g. "org.gnome.Nautilus.desktop"

    // Geometry in logical pixels (desktop coordinates). The buffer contains
    // the frame plus the client-side shadow margins.
    QRect frameRect;
    QRect bufferRect;

    // Image pixels per logical pixel, e.g. 2.0 on a HiDPI screen.
    double scale = 1.0;
};
