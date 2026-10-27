#include "capture/PickGeometry.h"

#include <QMap>

#include <algorithm>
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

namespace {

// One axis of nativeLayout(): where each logical coordinate at a monitor
// edge lands. Between two neighbouring edges the native length is the
// logical length times the largest scale of the monitors covering that
// stretch, so a monitor never overlaps the next and monitors side by side
// touch.
QMap<int, int> axisPositions(const QList<int> &starts, const QList<int> &ends, const QList<double> &scales)
{
    QList<int> edges = starts + ends;
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    double largest = 1.0;
    for (double scale : scales)
        largest = std::max(largest, scale);

    QMap<int, int> positions;
    if (edges.isEmpty())
        return positions;
    double position = edges.first() * largest;
    positions.insert(edges.first(), int(std::lround(position)));
    for (qsizetype k = 0; k + 1 < edges.size(); ++k) {
        double factor = 0;
        for (qsizetype i = 0; i < starts.size(); ++i) {
            if (starts[i] <= edges[k] && ends[i] >= edges[k + 1])
                factor = std::max(factor, scales[i]);
        }
        // A stretch no monitor covers (monitors not touching) keeps its size.
        position += (edges[k + 1] - edges[k]) * (factor > 0 ? factor : largest);
        positions.insert(edges[k + 1], int(std::lround(position)));
    }
    return positions;
}

} // namespace

QList<QRect> nativeLayout(const QList<QRect> &logical, const QList<QSize> &physical)
{
    const qsizetype count = std::min(logical.size(), physical.size());
    QList<int> left, right, top, bottom;
    QList<double> scaleX, scaleY;
    for (qsizetype i = 0; i < count; ++i) {
        left << logical[i].x();
        right << logical[i].x() + logical[i].width();
        top << logical[i].y();
        bottom << logical[i].y() + logical[i].height();
        scaleX << double(physical[i].width()) / std::max(1, logical[i].width());
        scaleY << double(physical[i].height()) / std::max(1, logical[i].height());
    }
    const QMap<int, int> xs = axisPositions(left, right, scaleX);
    const QMap<int, int> ys = axisPositions(top, bottom, scaleY);
    QList<QRect> native;
    for (qsizetype i = 0; i < count; ++i)
        native.append(QRect(QPoint(xs.value(left[i]), ys.value(top[i])), physical[i]));
    return native;
}

QRect logicalToNative(const QRect &logical, const QRect &screen, const QRect &native)
{
    if (screen.width() <= 0 || screen.height() <= 0)
        return {};
    const double sx = double(native.width()) / screen.width();
    const double sy = double(native.height()) / screen.height();
    // Edges, not sizes, are rounded, so neighbouring rectangles still touch.
    const int left = int(std::lround((logical.x() - screen.x()) * sx));
    const int top = int(std::lround((logical.y() - screen.y()) * sy));
    const int right = int(std::lround((logical.x() + logical.width() - screen.x()) * sx));
    const int bottom = int(std::lround((logical.y() + logical.height() - screen.y()) * sy));
    return QRect(native.x() + left, native.y() + top, right - left, bottom - top);
}

} // namespace PickGeometry
