#include "recording/VideoFrames.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace VideoFrames {

QSize evenSize(const QSize &size)
{
    // `& ~1` clears the lowest bit: 1921 becomes 1920.
    return QSize(std::max(2, size.width() & ~1), std::max(2, size.height() & ~1));
}

QImage fit(const QImage &frame, const QSize &target)
{
    if (frame.isNull() || target.isEmpty())
        return {};
    const QImage rgb = frame.format() == QImage::Format_RGB32 ? frame : frame.convertToFormat(QImage::Format_RGB32);
    if (rgb.size() == target)
        return rgb;
    const QSize extra = rgb.size() - target;
    if (extra.width() >= 0 && extra.width() <= 1 && extra.height() >= 0 && extra.height() <= 1)
        return rgb.copy(QRect(QPoint(0, 0), target));

    // Scaled into the video, keeping the aspect ratio; the rest stays black.
    QImage result(target, QImage::Format_RGB32);
    result.fill(Qt::black);
    const QSize scaled = rgb.size().scaled(target, Qt::KeepAspectRatio);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QPoint offset((target.width() - scaled.width()) / 2, (target.height() - scaled.height()) / 2);
    painter.drawImage(QRect(offset, scaled), rgb);
    return result;
}

QRect toPixels(const QRect &frame, double scale, const QSize &picture)
{
    const QRect pixels(QPoint(int(std::lround(frame.left() * scale)), int(std::lround(frame.top() * scale))),
                       QSize(int(std::lround(frame.width() * scale)), int(std::lround(frame.height() * scale))));
    return pixels.intersected(QRect(QPoint(0, 0), picture));
}

} // namespace VideoFrames
