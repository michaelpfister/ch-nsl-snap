#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>

// Geometry for picking a window on a frozen screen when the app draws the
// overlay itself (Windows, D-022). Native coordinates are physical desktop
// pixels, as Win32 reports them to a per-monitor DPI aware app; local
// coordinates are logical pixels inside one overlay widget.
namespace PickGeometry {

// Index of the first frame (topmost first) that contains `point`, or -1.
int topmostAt(const QList<QRect> &frames, const QPoint &point);

// A native point or rectangle as seen by the overlay on one screen, whose
// native rectangle is `screen` and whose device pixel ratio is `dpr`.
QPoint localToNative(const QPointF &local, const QRect &screen, double dpr);
QRectF nativeToLocal(const QRect &native, const QRect &screen, double dpr);

// The part of a captured window image that shows its visible frame (DWM
// extended frame bounds), in image pixels. The capture covers either the
// whole window rectangle, including the invisible resize borders, or just
// the frame; `content` is its size.
QRect frameInCapture(const QSize &content, const QRect &windowRect, const QRect &frame);

} // namespace PickGeometry
