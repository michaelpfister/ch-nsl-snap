#pragma once

#include "editor/FrameStyle.h"

#include <QGraphicsItem>

// A highlight frame (F-20–F-23): a hollow rectangle that can be moved,
// resized with eight handles and restyled. Coordinates are image pixels.
class FrameItem : public QGraphicsItem
{
public:
    enum { Type = UserType + 1 };

    // `scale` is image pixels per logical pixel; it converts the style's
    // logical sizes (line width, radius) into image pixels.
    FrameItem(const QRectF &rect, const FrameStyle &style, double scale);

    int type() const override { return Type; }

    QRectF rect() const { return m_rect; }
    void setRect(const QRectF &rect);

    FrameStyle style() const { return m_style; }
    void setStyle(const FrameStyle &style);

    // Handles keep their screen size, so the bounding rect depends on the
    // zoom; the view calls this after zooming.
    void viewZoomChanged() { prepareGeometryChange(); }

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    enum Handle { None = -1, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

    qreal penWidth() const { return m_style.width * m_scale; }
    // Converts screen pixels to image pixels for the current zoom, so
    // handles and the grab area keep their size when zooming.
    qreal screenToItem(qreal pixels) const;
    QPointF handlePoint(Handle handle) const;
    Handle handleAt(const QPointF &pos) const;
    bool showsHandles() const;

    QRectF m_rect;
    FrameStyle m_style;
    double m_scale = 1.0;

    Handle m_dragHandle = None;
    QRectF m_rectAtPress;
};
