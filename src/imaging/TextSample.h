#pragma once

#include <QColor>
#include <QImage>
#include <QRect>

// Looks at a text in an image region, so a replacement can match it
// (F-29): background colour, text colour, size and position.
struct TextSample
{
    QColor background;
    QColor text;
    // Rows and columns with text pixels ("ink"), in image coordinates.
    // Empty if the region holds no text.
    QRect ink;
    // Top of the capital letters and the baseline (image y), from the rows
    // with many text pixels; ascenders and descenders are sparse rows.
    int capTop = 0;
    int baseline = 0;
    // Suggested font pixel size (image pixels).
    int fontPixelSize = 0;
    // True if text pixels touch the edge of the region: the selection cuts
    // the original text, and parts of it would stay visible.
    bool touchesEdge = false;
    // False if the region has no plain background (photo, gradient) or no
    // text that stands out: then only `background` is meaningful.
    bool reliable = false;

    static TextSample analyze(const QImage &image, const QRect &region);
};
