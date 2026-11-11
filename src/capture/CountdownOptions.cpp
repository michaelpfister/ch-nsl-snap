#include "capture/CountdownOptions.h"

QString CountdownOptions::positionKey(CountdownPosition position)
{
    switch (position) {
    case CountdownPosition::TopLeft: return QStringLiteral("top-left");
    case CountdownPosition::TopRight: return QStringLiteral("top-right");
    case CountdownPosition::BottomLeft: return QStringLiteral("bottom-left");
    case CountdownPosition::BottomRight: return QStringLiteral("bottom-right");
    case CountdownPosition::Center: break;
    }
    return QStringLiteral("center");
}

CountdownPosition CountdownOptions::positionFromKey(const QString &key)
{
    for (CountdownPosition position : {CountdownPosition::TopLeft, CountdownPosition::TopRight,
                                       CountdownPosition::BottomLeft, CountdownPosition::BottomRight}) {
        if (key == positionKey(position))
            return position;
    }
    return CountdownPosition::Center;
}

QPoint CountdownOptions::place(CountdownPosition position, const QRect &screen, const QRect &workArea, const QSize &size)
{
    const int left = workArea.left() + kMargin;
    const int top = workArea.top() + kMargin;
    // QRect::right() is the last pixel inside, hence the + 1.
    const int right = workArea.right() + 1 - kMargin - size.width();
    const int bottom = workArea.bottom() + 1 - kMargin - size.height();
    switch (position) {
    case CountdownPosition::TopLeft: return QPoint(left, top);
    case CountdownPosition::TopRight: return QPoint(right, top);
    case CountdownPosition::BottomLeft: return QPoint(left, bottom);
    case CountdownPosition::BottomRight: return QPoint(right, bottom);
    case CountdownPosition::Center: break;
    }
    return QPoint(screen.x() + (screen.width() - size.width()) / 2, screen.y() + (screen.height() - size.height()) / 2);
}
