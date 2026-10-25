#include "imaging/WindowImage.h"

#include "capture/CaptureResult.h"

#include <QList>
#include <QPoint>

#include <algorithm>
#include <cmath>

namespace WindowImage {

namespace {

int alphaAt(const QImage &image, int x, int y)
{
    // Format_ARGB32: one QRgb per pixel, alpha not premultiplied.
    return qAlpha(reinterpret_cast<const QRgb *>(image.constScanLine(y))[x]);
}

bool columnOpaque(const QImage &image, int x, const QRect &frame)
{
    for (int y = frame.top() + frame.height() / 4; y < frame.top() + 3 * frame.height() / 4; ++y) {
        if (alphaAt(image, x, y) != 255)
            return false;
    }
    return true;
}

bool rowOpaque(const QImage &image, int y, const QRect &frame)
{
    for (int x = frame.left() + frame.width() / 4; x < frame.left() + 3 * frame.width() / 4; ++x) {
        if (alphaAt(image, x, y) != 255)
            return false;
    }
    return true;
}

} // namespace

QRect frameInImage(const QRect &frameRect, const QRect &bufferRect, double scale)
{
    return QRect(int(std::lround((frameRect.x() - bufferRect.x()) * scale)),
                 int(std::lround((frameRect.y() - bufferRect.y()) * scale)),
                 int(std::lround(frameRect.width() * scale)),
                 int(std::lround(frameRect.height() * scale)));
}

std::optional<QRect> opaqueCore(const QImage &source, const QRect &frame, double scale)
{
    if (!source.hasAlphaChannel())
        return std::nullopt;
    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    const int w = image.width();
    const int h = image.height();
    const int margin = std::max(1, int(std::lround(kEdgeSearch * scale)));
    const QRect search = frame.intersected(image.rect());
    if (search.isEmpty())
        return std::nullopt;

    // Each side searches from outside the frame inwards and takes the first
    // fully opaque line.
    std::optional<int> left, right, top, bottom;
    for (int x = std::max(0, frame.left() - margin); x <= std::min(w - 1, frame.left() + margin) && !left; ++x) {
        if (columnOpaque(image, x, search))
            left = x;
    }
    for (int x = std::min(w - 1, frame.right() + margin); x >= std::max(0, frame.right() - margin) && !right; --x) {
        if (columnOpaque(image, x, search))
            right = x;
    }
    for (int y = std::max(0, frame.top() - margin); y <= std::min(h - 1, frame.top() + margin) && !top; ++y) {
        if (rowOpaque(image, y, search))
            top = y;
    }
    for (int y = std::min(h - 1, frame.bottom() + margin); y >= std::max(0, frame.bottom() - margin) && !bottom; --y) {
        if (rowOpaque(image, y, search))
            bottom = y;
    }
    if (!left || !right || !top || !bottom)
        return std::nullopt;
    return QRect(QPoint(*left, *top), QPoint(*right, *bottom));
}

int cleanCorners(QImage &image, double scale)
{
    if (!image.hasAlphaChannel())
        return 0;
    if (image.format() != QImage::Format_ARGB32)
        image = image.convertToFormat(QImage::Format_ARGB32);

    const int w = image.width();
    const int h = image.height();
    const int size = std::min({int(std::lround(kCornerMaxRadius * scale)), w / 2, h / 2});
    int cleared = 0;

    struct Corner { int x0, y0, dx, dy; };
    const Corner corners[] = {{0, 0, 1, 1}, {w - 1, 0, -1, 1}, {0, h - 1, 1, -1}, {w - 1, h - 1, -1, -1}};

    for (const Corner &c : corners) {
        // Flood fill from the corner pixel, in coordinates relative to the
        // corner (i to the inside horizontally, j vertically).
        QList<bool> seen(size * size, false);
        QList<QPoint> stack{QPoint(0, 0)};
        while (!stack.isEmpty()) {
            const QPoint p = stack.takeLast();
            const int i = p.x();
            const int j = p.y();
            if (i < 0 || j < 0 || i >= size || j >= size || seen[j * size + i])
                continue;
            seen[j * size + i] = true;

            QRgb *pixel = reinterpret_cast<QRgb *>(image.scanLine(c.y0 + c.dy * j)) + (c.x0 + c.dx * i);
            const int alpha = qAlpha(*pixel);
            if (alpha >= kCornerAlphaThreshold)
                continue;
            if (alpha > 0) {
                *pixel = qRgba(qRed(*pixel), qGreen(*pixel), qBlue(*pixel), 0);
                ++cleared;
            }
            stack << QPoint(i + 1, j) << QPoint(i, j + 1) << QPoint(i - 1, j) << QPoint(i, j - 1);
        }
    }
    return cleared;
}

QImage prepare(const CaptureResult &capture)
{
    const QRect frame = frameInImage(capture.frameRect, capture.bufferRect, capture.scale)
                            .intersected(capture.image.rect());
    const QRect core = opaqueCore(capture.image, frame, capture.scale).value_or(frame);

    QImage image = capture.image.copy(core).convertToFormat(QImage::Format_ARGB32);
    cleanCorners(image, capture.scale);
    return image;
}

} // namespace WindowImage
