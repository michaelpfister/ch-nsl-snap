#pragma once

#include <QColor>

// Look of a highlight frame (F-22, F-23). Sizes are in logical pixels, so a
// 3 px frame looks the same on a scale-1 and a scale-2 capture.
struct FrameStyle
{
    QColor color = QColor(0xE5484D);
    int width = 3;        // line width
    bool rounded = true;  // rounded or square corners

    // Corner radius of rounded frames, like the mockup.
    static constexpr int kRadius = 10;

    bool operator==(const FrameStyle &) const = default;
};
