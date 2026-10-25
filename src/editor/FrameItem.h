#pragma once

#include "editor/FrameStyle.h"
#include "editor/RectItem.h"

// A highlight frame (F-20–F-24): a hollow rectangle, optionally with a
// number badge on a corner. Moving and resizing come from RectItem.
class FrameItem : public RectItem
{
public:
    enum { Type = UserType + 1 };

    FrameItem(const QRectF &rect, const FrameStyle &style, double scale);

    int type() const override { return Type; }
    QString label() const override;

    FrameStyle style() const { return m_style; }
    void setStyle(const FrameStyle &style);
    bool isNumbered() const { return m_style.badge != BadgePosition::None; }

    // Where the number is actually drawn: the position from the style if
    // the circle fits on the image, otherwise the nearest corner that does.
    BadgePosition effectiveBadge() const;
    // Corners where the number would be fully visible; the panel offers only these.
    QList<BadgePosition> fittingBadgePositions() const;

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    bool isHiddenHandle(Handle handle) const override;

private:
    qreal penWidth() const { return m_style.width * imageScale(); }
    QRectF badgeRect() const;
    QRectF badgeRectAt(BadgePosition position) const;
    bool badgeFits(BadgePosition position) const;
    int number() const;

    FrameStyle m_style;
};
