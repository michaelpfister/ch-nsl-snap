#include "editor/RectItem.h"

#include "editor/Commands.h"
#include "editor/EditorScene.h"

#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QUndoStack>

#include <algorithm>

namespace {

constexpr qreal kHandleSize = 8.0; // screen pixels
constexpr qreal kMinSize = 4.0;    // image pixels

qint64 nextOrder()
{
    static qint64 counter = 0;
    return ++counter;
}

} // namespace

RectItem::RectItem(const QRectF &rect, double scale)
    : m_rect(rect.normalized())
    , m_scale(scale)
    , m_order(nextOrder())
{
    setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
}

void RectItem::setRect(const QRectF &rect)
{
    if (rect.normalized() == m_rect)
        return;
    prepareGeometryChange();
    m_rect = rect.normalized();
}

qreal RectItem::screenToItem(qreal pixels) const
{
    const QList<QGraphicsView *> views = scene() ? scene()->views() : QList<QGraphicsView *>();
    const qreal zoom = views.isEmpty() ? 1.0 : views.first()->transform().m11();
    return pixels / zoom;
}

bool RectItem::showsHandles() const
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    return isSelected() && editorScene && !editorScene->isExporting();
}

qreal RectItem::handleMargin() const
{
    return screenToItem(kHandleSize + 6);
}

QPointF RectItem::handlePoint(Handle handle) const
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

RectItem::Handle RectItem::handleAt(const QPointF &pos) const
{
    if (!showsHandles())
        return None;
    const qreal h = screenToItem(kHandleSize + 4);
    for (int i = TopLeft; i <= Left; ++i) {
        if (!isHiddenHandle(Handle(i))
            && QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)).contains(pos))
            return Handle(i);
    }
    return None;
}

void RectItem::addHandlesToShape(QPainterPath &path) const
{
    if (!showsHandles())
        return;
    const qreal h = screenToItem(kHandleSize + 4);
    for (int i = TopLeft; i <= Left; ++i) {
        if (!isHiddenHandle(Handle(i)))
            path.addRect(QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)));
    }
}

void RectItem::paintHandles(QPainter *painter) const
{
    if (!showsHandles())
        return;
    const qreal h = screenToItem(kHandleSize);
    painter->setPen(QPen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5)));
    painter->setBrush(Qt::white);
    for (int i = TopLeft; i <= Left; ++i) {
        if (!isHiddenHandle(Handle(i)))
            painter->drawRect(QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)));
    }
}

QVariant RectItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // Shift the new position so the rectangle stays inside the image.
        const QRectF bounds = scene()->sceneRect();
        QPointF pos = value.toPointF();
        const QRectF r = m_rect.translated(pos);
        if (r.width() <= bounds.width())
            pos.rx() += std::clamp(r.left(), bounds.left(), bounds.right() - r.width()) - r.left();
        if (r.height() <= bounds.height())
            pos.ry() += std::clamp(r.top(), bounds.top(), bounds.bottom() - r.height()) - r.top();
        return pos;
    }
    return QGraphicsItem::itemChange(change, value);
}

void RectItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
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

void RectItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragHandle = event->button() == Qt::LeftButton ? handleAt(event->pos()) : None;
    if (m_dragHandle == None) {
        QGraphicsItem::mousePressEvent(event); // select and move
        return;
    }
    m_rectAtPress = m_rect;
    event->accept();
}

void RectItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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
    // Stay inside the image: its rectangle in item coordinates.
    if (scene())
        r = r.normalized().intersected(scene()->sceneRect().translated(-pos()));
    r = r.normalized();
    if (r.width() >= kMinSize && r.height() >= kMinSize)
        setRect(r);
}

void RectItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragHandle == None) {
        QGraphicsItem::mouseReleaseEvent(event);
        return;
    }
    m_dragHandle = None;
    auto *editorScene = qobject_cast<EditorScene *>(scene());
    if (editorScene && m_rect != m_rectAtPress)
        editorScene->undoStack()->push(new SetRectCommand(this, m_rectAtPress, m_rect));
}
