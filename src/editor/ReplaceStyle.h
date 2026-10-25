#pragma once

#include <QColor>
#include <QString>

// Content and look of a replaced text (F-29). Sizes are in logical pixels
// (D-015); offsets place the text inside the area like the original.
struct ReplaceStyle
{
    QString text;
    QColor background = Qt::white;
    QColor color = QColor(0x1F2328);
    QString fontFamily;   // empty = the system UI font
    int fontSize = 13;    // pixel size, logical px
    bool bold = false;
    qreal textLeft = 4;     // from the left edge of the area, logical px
    qreal textBaseline = 0; // baseline from the top of the area, logical px; 0 = centred

    bool operator==(const ReplaceStyle &) const = default;
};
