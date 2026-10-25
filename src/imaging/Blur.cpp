#include "imaging/Blur.h"

#include <QList>

#include <algorithm>

namespace Blur {

namespace {

// One box-blur pass along a line of `count` pixels, `step` apart (1 for a
// row, the image width for a column). Works on premultiplied ARGB, so
// transparent pixels do not darken their neighbours. Edges are clamped.
//
// This runs millions of times per blur, so it avoids per-pixel bounds
// checks: the window sum is updated with the pixel entering and the pixel
// leaving, both looked up with plain index arithmetic.
void blurLine(const quint32 *src, quint32 *dst, int count, int step, int radius)
{
    const int window = 2 * radius + 1;
    // Multiply instead of divide: (sum * inv) >> 16 ≈ sum / window.
    const quint32 inv = (1u << 16) / quint32(window) + 1;
    quint32 a = 0, r = 0, g = 0, b = 0;

    const quint32 first = src[0];
    const quint32 last = src[(count - 1) * step];
    // Initial window: `radius + 1` copies of the first pixel (clamped edge)
    // plus the next `radius` pixels.
    for (int i = -radius; i <= radius; ++i) {
        const quint32 px = src[std::clamp(i, 0, count - 1) * step];
        a += px >> 24;
        r += (px >> 16) & 0xff;
        g += (px >> 8) & 0xff;
        b += px & 0xff;
    }
    for (int i = 0; i < count; ++i) {
        dst[i * step] = (((a * inv) >> 16) << 24) | (((r * inv) >> 16) << 16)
                        | (((g * inv) >> 16) << 8) | ((b * inv) >> 16);
        const int in = i + radius + 1;
        const int out = i - radius;
        const quint32 pin = in < count ? src[in * step] : last;
        const quint32 pout = out >= 0 ? src[out * step] : first;
        a += (pin >> 24) - (pout >> 24);
        r += ((pin >> 16) & 0xff) - ((pout >> 16) & 0xff);
        g += ((pin >> 8) & 0xff) - ((pout >> 8) & 0xff);
        b += (pin & 0xff) - (pout & 0xff);
    }
}

} // namespace

QImage boxBlur(const QImage &image, int radius)
{
    if (radius <= 0 || image.isNull())
        return image;

    QImage a = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QImage b(a.size(), QImage::Format_ARGB32_Premultiplied);
    const int w = a.width();
    const int h = a.height();
    // bytesPerLine is w * 4 for this format, so the pixels are contiguous.
    Q_ASSERT(a.bytesPerLine() == w * 4);

    // Columns are blurred on a transposed copy, so both directions read
    // memory in order (much faster than striding down a column).
    QImage t(h, w, QImage::Format_ARGB32_Premultiplied);
    QImage u(h, w, QImage::Format_ARGB32_Premultiplied);
    const auto transpose = [](const QImage &from, QImage &to) {
        const int fw = from.width();
        const int fh = from.height();
        auto *src = reinterpret_cast<const quint32 *>(from.constBits());
        auto *dst = reinterpret_cast<quint32 *>(to.bits());
        for (int y = 0; y < fh; ++y)
            for (int x = 0; x < fw; ++x)
                dst[x * fh + y] = src[y * fw + x];
    };

    for (int pass = 0; pass < 3; ++pass) {
        auto *src = reinterpret_cast<const quint32 *>(a.constBits());
        auto *dst = reinterpret_cast<quint32 *>(b.bits());
        for (int y = 0; y < h; ++y)
            blurLine(src + y * w, dst + y * w, w, 1, radius);
        transpose(b, t);
        src = reinterpret_cast<const quint32 *>(t.constBits());
        dst = reinterpret_cast<quint32 *>(u.bits());
        for (int x = 0; x < w; ++x)
            blurLine(src + x * h, dst + x * h, h, 1, radius);
        transpose(u, a);
    }
    return a;
}

} // namespace Blur
