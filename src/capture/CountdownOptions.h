#pragma once

#include <QRect>
#include <QSize>
#include <QString>

// Where the countdown of a delayed capture appears, and whether a click on
// it cancels (F-04, D-025). Set in the settings, used by the app's own
// Countdown and passed to the GNOME extension.
enum class CountdownPosition {
    TopLeft,
    TopRight,
    Center,
    BottomLeft,
    BottomRight,
};

struct CountdownOptions
{
    CountdownPosition position = CountdownPosition::Center;
    // False: clicks go through the countdown to what is below it, and only
    // Escape cancels.
    bool clickCancels = true;

    // Distance of a corner position from the edges of the work area
    // (logical pixels).
    static constexpr int kMargin = 24;

    // The name the GNOME extension expects ("top-left", "center", …) and
    // the one stored in the settings.
    static QString positionKey(CountdownPosition position);
    // The position for a key; Center for an unknown one.
    static CountdownPosition positionFromKey(const QString &key);

    // Where a countdown of `size` goes: in a corner of `workArea` (the
    // monitor without panels), kMargin from its edges, or in the middle of
    // `screen` (the whole monitor). Returns the top left corner.
    static QPoint place(CountdownPosition position, const QRect &screen, const QRect &workArea, const QSize &size);
};
