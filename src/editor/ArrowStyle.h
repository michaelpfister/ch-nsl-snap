#pragma once

#include <QColor>

// Look of an arrow (F-44). Sizes are in logical pixels (D-015); the head
// grows with the line width.
struct ArrowStyle
{
    QColor color = QColor(0xE5484D); // like the frames
    int width = 4;                   // line width

    bool operator==(const ArrowStyle &) const = default;
};
