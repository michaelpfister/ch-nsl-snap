#include "capture/PickGeometry.h"

#include <cmath>

namespace PickGeometry {

int topmostAt(const QList<QRect> &frames, const QPoint &point)
{
    for (qsizetype i = 0; i < frames.size(); ++i) {
        if (frames[i].contains(point))
            return int(i);
    }
    return -1;
}

QPoint localToNative(const QPointF &local, const QRect &screen, double dpr)
{
    return screen.topLeft() + QPoint(int(std::floor(local.x() * dpr)), int(std::floor(local.y() * dpr)));
}

QRectF nativeToLocal(const QRect &native, const QRect &screen, double dpr)
{
    const QRectF r(native.translated(-screen.topLeft()));
    return QRectF(r.x() / dpr, r.y() / dpr, r.width() / dpr, r.height() / dpr);
}

QRect frameInCapture(const QSize &content, const QRect &windowRect, const QRect &frame)
{
    const QRect image(QPoint(0, 0), content);
    if (content == windowRect.size()) {
        // The capture starts at the window rectangle: skip the invisible borders.
        return frame.translated(-windowRect.topLeft()).intersected(image);
    }
    // The capture is the frame itself (or a size we do not know): from the origin.
    return QRect(QPoint(0, 0), frame.size()).intersected(image);
}

} // namespace PickGeometry
