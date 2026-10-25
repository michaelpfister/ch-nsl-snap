#pragma once

#include <QColor>

// Where a numbered frame shows its number (F-24); None = plain frame.
enum class BadgePosition { None, TopRight, TopLeft, BottomRight, BottomLeft };

// Look of a highlight frame (F-22–F-24). Sizes are in logical pixels, so a
// 3 px frame looks the same on a scale-1 and a scale-2 capture (D-015).
struct FrameStyle
{
    QColor color = QColor(0xE5484D);
    int width = 3;        // line width
    bool rounded = true;  // rounded or square corners
    BadgePosition badge = BadgePosition::None;

    // Corner radius of rounded frames, like the mockup.
    static constexpr int kRadius = 10;
    // Number badge: a circle centred on the frame's corner.
    static constexpr int kBadgeDiameter = 26;
    static constexpr int kBadgeFontSize = 14;

    bool operator==(const FrameStyle &) const = default;
};
