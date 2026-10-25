#include "imaging/TextSample.h"

#include <QHash>

#include <algorithm>
#include <cmath>

namespace {

// Colours closer than this (RGB distance) to the background count as
// background; anti-aliased text edges are mostly further away.
constexpr int kInkDistance = 60;

int distance(QRgb a, QRgb b)
{
    const int dr = qRed(a) - qRed(b);
    const int dg = qGreen(a) - qGreen(b);
    const int db = qBlue(a) - qBlue(b);
    return int(std::sqrt(double(dr * dr + dg * dg + db * db)));
}

// A plain background: at least this share of the edge pixels has the
// dominant colour. Photos and gradients stay far below.
constexpr double kPlainShare = 0.6;
// Text covers well under half of its box; more "ink" means a busy picture.
constexpr double kMaxInkShare = 0.45;

// Groups similar colours (4 bits per channel) and returns the average of
// the most frequent group; `share` receives its part of all pixels.
QColor dominant(const QList<QRgb> &pixels, double *share = nullptr)
{
    QHash<int, int> counts;
    for (QRgb p : pixels)
        ++counts[((qRed(p) >> 4) << 8) | ((qGreen(p) >> 4) << 4) | (qBlue(p) >> 4)];
    int best = -1;
    int bestCount = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        if (it.value() > bestCount) {
            best = it.key();
            bestCount = it.value();
        }
    }
    qint64 r = 0, g = 0, b = 0, n = 0;
    for (QRgb p : pixels) {
        if ((((qRed(p) >> 4) << 8) | ((qGreen(p) >> 4) << 4) | (qBlue(p) >> 4)) == best) {
            r += qRed(p);
            g += qGreen(p);
            b += qBlue(p);
            ++n;
        }
    }
    if (share)
        *share = pixels.isEmpty() ? 0.0 : double(n) / pixels.size();
    return n ? QColor(int(r / n), int(g / n), int(b / n)) : QColor(Qt::white);
}

} // namespace

TextSample TextSample::analyze(const QImage &source, const QRect &region)
{
    TextSample sample;
    const QRect area = region.intersected(source.rect());
    if (area.width() < 3 || area.height() < 3)
        return sample;
    const QImage image = source.convertToFormat(QImage::Format_RGB32);

    // Background: the most common colour along the edge of the selection,
    // where there is usually no text.
    QList<QRgb> edge;
    for (int x = area.left(); x <= area.right(); ++x)
        edge << image.pixel(x, area.top()) << image.pixel(x, area.bottom());
    for (int y = area.top() + 1; y < area.bottom(); ++y)
        edge << image.pixel(area.left(), y) << image.pixel(area.right(), y);
    double plainShare = 0;
    sample.background = dominant(edge, &plainShare);
    const QRgb bg = sample.background.rgb();

    // Ink: everything clearly different from the background.
    QList<QRgb> ink;
    int maxDistance = 0;
    qint64 inkCount = 0;
    int left = area.right(), right = area.left(), top = area.bottom(), bottom = area.top();
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const QRgb p = image.pixel(x, y);
            const int d = distance(p, bg);
            if (d < kInkDistance)
                continue;
            ++inkCount;
            maxDistance = std::max(maxDistance, d);
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = std::max(bottom, y);
        }
    }
    if (maxDistance == 0) {
        sample.text = sample.background.lightness() > 128 ? QColor(0x1F2328) : QColor(Qt::white);
        return sample;
    }
    sample.ink = QRect(QPoint(left, top), QPoint(right, bottom));

    // Text colour: the most common colour among the strongest ink pixels
    // (the cores of the letters, not their anti-aliased edges).
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const QRgb p = image.pixel(x, y);
            if (distance(p, bg) >= maxDistance * 3 / 4)
                ink << p;
        }
    }
    sample.text = dominant(ink);

    // Cap top and baseline from the number of text pixels per row.
    // Capitals and ascenders are fewer than the lower-case letters, so the
    // top uses a low threshold (10 % of the densest row). The baseline is
    // where the count drops most in the lower half: below it only the
    // descenders (g, p, y) continue.
    QList<int> rowCounts;
    int maxRow = 0;
    for (int y = top; y <= bottom; ++y) {
        int count = 0;
        for (int x = left; x <= right; ++x)
            count += distance(image.pixel(x, y), bg) >= kInkDistance;
        rowCounts << count;
        maxRow = std::max(maxRow, count);
    }
    const int rows = int(rowCounts.size());
    int first = 0;
    while (first < rows - 1 && rowCounts[first] < maxRow / 10)
        ++first;
    int baselineRow = rows; // just below the last row: no descenders
    int biggestDrop = rowCounts[rows - 1];
    for (int i = (first + rows) / 2; i < rows - 1; ++i) {
        const int drop = rowCounts[i] - rowCounts[i + 1];
        if (drop > biggestDrop) {
            biggestDrop = drop;
            baselineRow = i + 1;
        }
    }
    sample.capTop = top + first;
    sample.baseline = top + baselineRow;

    // Cap height is about 0.7 of the font's pixel size for common UI fonts
    // (Segoe UI, Cantarell, Inter, SF).
    sample.fontPixelSize = std::max(4, int(std::lround((sample.baseline - sample.capTop) / 0.70)));
    sample.touchesEdge = left == area.left() || right == area.right() || top == area.top() || bottom == area.bottom();

    // Trust the result only on a plain background with text that stands
    // out from it; on a photo everything counts as "ink".
    const double inkShare = double(inkCount) / std::max<qint64>(1, qint64(sample.ink.width()) * sample.ink.height());
    sample.reliable = plainShare >= kPlainShare && inkShare <= kMaxInkShare && sample.baseline > sample.capTop + 2;
    return sample;
}
