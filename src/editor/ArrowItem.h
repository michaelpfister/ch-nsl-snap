#pragma once

#include "editor/ArrowStyle.h"
#include "editor/RectItem.h"

#include <QLineF>

// An arrow (F-44) from a start point to its tip. Its rectangle (RectItem)
// is the box around the line, which keeps it inside the image when moved;
// instead of the eight handles it has one at each end.
class ArrowItem : public RectItem
{
public:
    enum { Type = UserType + 6 };

    ArrowItem(const QLineF &line, const ArrowStyle &style, double scale);

    int type() const override { return Type; }
    QString label() const override;

    // Start (p1) and tip (p2), in item coordinates.
    QLineF line() const { return m_line; }
    void setLine(const QLineF &line);

    ArrowStyle style() const { return m_style; }
    void setStyle(const ArrowStyle &style);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    bool isHiddenHandle(Handle) const override { return true; }
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum class End { None, Start, Tip };

    qreal penWidth() const { return m_style.width * imageScale(); }
    // The filled head and where the shaft ends under it.
    QPolygonF head(QPointF *shaftEnd) const;
    End endAt(const QPointF &pos) const;

    QLineF m_line;
    ArrowStyle m_style;
    End m_dragEnd = End::None;
    QLineF m_lineAtPress;
};
