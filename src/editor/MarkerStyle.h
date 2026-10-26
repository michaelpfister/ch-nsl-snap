#pragma once

#include <QColor>

// Look of a text marker (F-28): a felt-pen colour, multiplied with the
// image so dark text stays dark.
struct MarkerStyle
{
    QColor color = QColor(0xFFE629); // yellow, like a highlighter pen

    bool operator==(const MarkerStyle &) const = default;
};
