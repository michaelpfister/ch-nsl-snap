#include "editor/ArrowItem.h"

#include "editor/Commands.h"
#include "editor/EditorScene.h"

#include <QCoreApplication>
#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPalette>
#include <QUndoStack>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kHandleSize = 8.0;  // screen pixels, like RectItem
constexpr qreal kGrabMargin = 6.0;  // screen pixels around the line that still grab it

QRectF boxAround(const QLineF &line)
{
    return QRectF(line.p1(), line.p2()).normalized();
}

} // namespace

ArrowItem::ArrowItem(const QLineF &line, const ArrowStyle &style, double scale)
    : RectItem(boxAround(line), scale)
    , m_line(line)
    , m_style(style)
{
    setZValue(0.25); // above the frames, below texts
}

QString ArrowItem::label() const
{
    return QCoreApplication::translate("ArrowItem", "Pfeil");
}

void ArrowItem::setLine(const QLineF &line)
{
    if (line == m_line)
        return;
    prepareGeometryChange();
    m_line = line;
    setRect(boxAround(line));
    update();
}

void ArrowItem::setStyle(const ArrowStyle &style)
{
    if (style == m_style)
        return;
    prepareGeometryChange();
    m_style = style;
}

QPolygonF ArrowItem::head(QPointF *shaftEnd) const
{
    const qreal length = m_line.length();
    // The head grows with the line width, but never takes more than 60 %
    // of a short arrow.
    const qreal headLength = std::min(penWidth() * 3.5 + 4 * imageScale(), length * 0.6);
    const qreal halfWidth = headLength * 0.55;
    if (length <= 0) {
        *shaftEnd = m_line.p2();
        return {};
    }
    const QPointF dir = (m_line.p2() - m_line.p1()) / length;
    const QPointF normal(-dir.y(), dir.x());
    const QPointF tip = m_line.p2();
    const QPointF base = tip - dir * headLength;
    // The shaft ends inside the head, so its round end does not show.
    *shaftEnd = tip - dir * (headLength * 0.7);
    return QPolygonF({tip, base + normal * halfWidth, base - normal * halfWidth});
}

QRectF ArrowItem::boundingRect() const
{
    QPointF shaftEnd;
    const QRectF headBox = head(&shaftEnd).boundingRect();
    const qreal m = std::max(handleMargin(), penWidth());
    return rect().united(headBox).adjusted(-m, -m, m, m);
}

ArrowItem::End ArrowItem::endAt(const QPointF &pos) const
{
    if (!showsHandles())
        return End::None;
    const qreal reach = screenToItem(kHandleSize + 4);
    if (QLineF(pos, m_line.p2()).length() <= reach)
        return End::Tip;
    if (QLineF(pos, m_line.p1()).length() <= reach)
        return End::Start;
    return End::None;
}

QPainterPath ArrowItem::shape() const
{
    // Only the line, the head and the handles grab, not the box around them.
    QPainterPath line;
    line.moveTo(m_line.p1());
    line.lineTo(m_line.p2());
    QPainterPathStroker stroker;
    stroker.setWidth(penWidth() + 2 * screenToItem(kGrabMargin));
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath path = stroker.createStroke(line);
    QPointF shaftEnd;
    path.addPolygon(head(&shaftEnd));
    if (showsHandles()) {
        const qreal h = screenToItem(kHandleSize + 4);
        for (const QPointF &p : {m_line.p1(), m_line.p2()})
            path.addEllipse(p, h / 2, h / 2);
    }
    return path;
}

void ArrowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPointF shaftEnd;
    const QPolygonF tipShape = head(&shaftEnd);
    painter->setPen(QPen(m_style.color, penWidth(), Qt::SolidLine, Qt::RoundCap));
    painter->drawLine(m_line.p1(), shaftEnd);
    // The head, with slightly rounded corners.
    painter->setPen(QPen(m_style.color, penWidth() * 0.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(m_style.color);
    painter->drawPolygon(tipShape);
    painter->restore();

    if (showsHandles()) {
        const qreal h = screenToItem(kHandleSize);
        painter->setPen(QPen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5)));
        painter->setBrush(Qt::white);
        for (const QPointF &p : {m_line.p1(), m_line.p2()})
            painter->drawEllipse(p, h / 2, h / 2);
    }
}

void ArrowItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(QCursor(endAt(event->pos()) != End::None ? Qt::CrossCursor : Qt::SizeAllCursor));
    QGraphicsItem::hoverMoveEvent(event);
}

void ArrowItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_dragEnd = event->button() == Qt::LeftButton ? endAt(event->pos()) : End::None;
    if (m_dragEnd == End::None) {
        RectItem::mousePressEvent(event); // select and move
        return;
    }
    m_lineAtPress = m_line;
    event->accept();
}

void ArrowItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragEnd == End::None) {
        RectItem::mouseMoveEvent(event);
        return;
    }
    // The end stays on the image: its rectangle in item coordinates.
    QPointF p = event->pos();
    if (scene()) {
        const QRectF bounds = scene()->sceneRect().translated(-pos());
        p = QPointF(std::clamp(p.x(), bounds.left(), bounds.right()),
                    std::clamp(p.y(), bounds.top(), bounds.bottom()));
    }
    QLineF line = m_lineAtPress;
    if (m_dragEnd == End::Tip)
        line.setP2(p);
    else
        line.setP1(p);
    setLine(line);
}

void ArrowItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_dragEnd == End::None) {
        RectItem::mouseReleaseEvent(event);
        return;
    }
    m_dragEnd = End::None;
    auto *editorScene = qobject_cast<EditorScene *>(scene());
    if (editorScene && m_line != m_lineAtPress)
        editorScene->undoStack()->push(new SetArrowLineCommand(this, m_lineAtPress, m_line));
}
