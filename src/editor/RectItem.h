#pragma once

#include <QGraphicsItem>

// Base for rectangular annotations (frames, pixelated areas): the
// rectangle, eight resize handles, moving, and staying inside the image.
// Coordinates are image pixels. Subclasses paint the content.
class RectItem : public QGraphicsItem
{
public:
    // `scale` is image pixels per logical pixel (D-015). Items that replace
    // image pixels keep their rectangle on whole pixels (`snapToPixels`),
    // so no original pixel shows at the edges (D-018).
    RectItem(const QRectF &rect, double scale, bool snapToPixels = false);

    QRectF rect() const { return m_rect; }
    void setRect(const QRectF &rect);
    double imageScale() const { return m_scale; }

    // Creation order: the order in the object list, and the numbering of
    // numbered frames (F-24). Kept through undo/redo.
    qint64 order() const { return m_order; }
    void setOrder(qint64 order) { m_order = order; }

    // Moves the item back into the image if it reaches beyond it, e.g. a
    // text that grew while typing. Moves by dragging are kept inside anyway.
    void keepInsideScene();

    // Name in the object list, e.g. "Rahmen".
    virtual QString label() const = 0;

    // Handles keep their screen size, so the bounding rect depends on the
    // zoom; the view calls this after zooming.
    void viewZoomChanged() { prepareGeometryChange(); }

protected:
    enum Handle { None = -1, TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left };

    // Converts screen pixels to image pixels for the current zoom.
    qreal screenToItem(qreal pixels) const;
    bool showsHandles() const;
    // Margin around the rectangle that the handles need.
    qreal handleMargin() const;
    // Adds the handles to a shape, so they can be grabbed.
    void addHandlesToShape(QPainterPath &path) const;
    void paintHandles(QPainter *painter) const;
    // A subclass can hide a handle, e.g. the one under a number badge.
    virtual bool isHiddenHandle(Handle) const { return false; }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QPointF handlePoint(Handle handle) const;
    Handle handleAt(const QPointF &pos) const;

    QRectF m_rect;
    double m_scale = 1.0;
    bool m_snap = false;
    qint64 m_order = 0;

    Handle m_dragHandle = None;
    QRectF m_rectAtPress;
};
