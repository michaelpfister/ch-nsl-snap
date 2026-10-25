#include "editor/FrameItem.h"

#include "editor/Commands.h"
#include "editor/EditorScene.h"

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QPainterPathStroker>
#include <QUndoStack>

#include <algorithm>

namespace {

constexpr qreal kHandleSize = 8.0;  // screen pixels
constexpr qreal kGrabMargin = 6.0;  // screen pixels around the line that still grab it
constexpr qreal kMinSize = 4.0;     // image pixels

} // namespace

FrameItem::FrameItem(const QRectF &rect, const FrameStyle &style, double scale)
    : m_rect(rect.normalized())
    , m_style(style)
    , m_scale(scale)
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
}

void FrameItem::setRect(const QRectF &rect)
{
    if (rect == m_rect)
        return;
    prepareGeometryChange();
    m_rect = rect.normalized();
}

void FrameItem::setStyle(const FrameStyle &style)
{
    if (style == m_style)
        return;
    prepareGeometryChange();
    m_style = style;
}

qreal FrameItem::screenToItem(qreal pixels) const
{
    const QList<QGraphicsView *> views = scene() ? scene()->views() : QList<QGraphicsView *>();
    const qreal zoom = views.isEmpty() ? 1.0 : views.first()->transform().m11();
    return pixels / zoom;
}

QRectF FrameItem::boundingRect() const
{
    const qreal margin = std::max(penWidth(), screenToItem(kHandleSize + kGrabMargin));
    return m_rect.adjusted(-margin, -margin, margin, margin);
}

QPainterPath FrameItem::shape() const
{
    // Only the line (plus a margin) is clickable, so frames inside other
    // frames and the image underneath stay reachable.
    QPainterPath outline;
    outline.addRect(m_rect);
    QPainterPathStroker stroker;
    stroker.setWidth(penWidth() + 2 * screenToItem(kGrabMargin));
    QPainterPath path = stroker.createStroke(outline);
    if (showsHandles()) {
        const qreal h = screenToItem(kHandleSize + 4);
        for (int i = TopLeft; i <= Left; ++i)
            path.addRect(QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)));
    }
    return path;
}

void FrameItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    // The line lies inside the rectangle, like a CSS border.
    const qreal w = penWidth();
    const QRectF line = m_rect.adjusted(w / 2, w / 2, -w / 2, -w / 2);
    painter->setPen(QPen(m_style.color, w, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    if (m_style.rounded) {
        const qreal radius = std::min({FrameStyle::kRadius * m_scale, line.width() / 2, line.height() / 2});
        painter->drawRoundedRect(line, radius, radius);
    } else {
        painter->drawRect(line);
    }

    if (!showsHandles())
        return;
    const qreal h = screenToItem(kHandleSize);
    QPen handlePen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5));
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int i = TopLeft; i <= Left; ++i)
        painter->drawRect(QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)));
}

bool FrameItem::showsHandles() const
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    return isSelected() && editorScene && !editorScene->isExporting();
}

QPointF FrameItem::handlePoint(Handle handle) const
{
    const QRectF &r = m_rect;
    switch (handle) {
    case TopLeft: return r.topLeft();
    case Top: return QPointF(r.center().x(), r.top());
    case TopRight: return r.topRight();
    case Right: return QPointF(r.right(), r.center().y());
    case BottomRight: return r.bottomRight();
    case Bottom: return QPointF(r.center().x(), r.bottom());
    case BottomLeft: return r.bottomLeft();
    case Left: return QPointF(r.left(), r.center().y());
    case None: break;
    }
    return {};
}

FrameItem::Handle FrameItem::handleAt(const QPointF &pos) const
{
    if (!showsHandles())
        return None;
    const qreal h = screenToItem(kHandleSize + 4);
    for (int i = TopLeft; i <= Left; ++i) {
        if (QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)).contains(pos))
            return Handle(i);
    }
    return None;
}

void FrameItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    switch (handleAt(event->pos())) {
    case TopLeft:
    case BottomRight: setCursor(Qt::SizeFDiagCursor); break;
    case TopRight:
    case BottomLeft: setCursor(Qt::SizeBDiagCursor); break;
    case Top:
    case Bottom: setCursor(Qt::SizeVerCursor); break;
    case Left:
    case Right: setCursor(Qt::SizeHorCursor); break;
    case None: setCursor(Qt::SizeAllCursor); break;
    }
    QGraphicsItem::hoverMoveEvent(event);
}

void FrameItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragHandle = event->button() == Qt::LeftButton ? handleAt(event->pos()) : None;
    if (m_dragHandle == None) {
        QGraphicsItem::mousePressEvent(event); // select and move
        return;
    }
    m_rectAtPress = m_rect;
    event->accept();
}

void FrameItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragHandle == None) {
        QGraphicsItem::mouseMoveEvent(event);
        return;
    }
    QRectF r = m_rectAtPress;
    const QPointF p = event->pos();
    switch (m_dragHandle) {
    case TopLeft: r.setTopLeft(p); break;
    case Top: r.setTop(p.y()); break;
    case TopRight: r.setTopRight(p); break;
    case Right: r.setRight(p.x()); break;
    case BottomRight: r.setBottomRight(p); break;
    case Bottom: r.setBottom(p.y()); break;
    case BottomLeft: r.setBottomLeft(p); break;
    case Left: r.setLeft(p.x()); break;
    case None: break;
    }
    r = r.normalized();
    if (r.width() >= kMinSize && r.height() >= kMinSize)
        setRect(r);
}

void FrameItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragHandle == None) {
        QGraphicsItem::mouseReleaseEvent(event);
        return;
    }
    m_dragHandle = None;
    auto *editorScene = qobject_cast<EditorScene *>(scene());
    if (editorScene && m_rect != m_rectAtPress)
        editorScene->undoStack()->push(new SetFrameRectCommand(this, m_rectAtPress, m_rect));
}
