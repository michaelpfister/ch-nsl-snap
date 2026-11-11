#pragma once

#include <QImage>
#include <QRect>
#include <QSize>

// Picture helpers for videos (F-50, D-039), kept apart so the tests can
// check them without a recording.
namespace VideoFrames {

// The video size for a picked window or monitor: its physical size, each
// side rounded down to an even number (H.264 with 4:2:0 colour needs
// that), at least 2 x 2.
QSize evenSize(const QSize &size);

// Fits a frame into the video size `target`. A frame of the same size is
// returned as it is; one that is at most a pixel larger (the odd row or
// column evenSize() dropped) is cut. Any other size (a window resized
// during the recording, Q-30) is scaled into `target` keeping its aspect
// ratio and centred on black. The result is always RGB32, without alpha.
QImage fit(const QImage &frame, const QSize &target);

// The part of a window picture that is the window itself, in its pixels:
// `frame` in logical pixels (as WaylandBackend::frameInPicture() gives
// it) times `scale`, kept inside the picture.
QRect toPixels(const QRect &frame, double scale, const QSize &picture);

} // namespace VideoFrames
