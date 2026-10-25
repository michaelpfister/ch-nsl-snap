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

qint64 nextOrder()
{
    static qint64 counter = 0;
    return ++counter;
}

// Dark text on light badge colours (yellow), white otherwise.
QColor badgeTextColor(const QColor &background)
{
    return background.lightnessF() > 0.6 ? QColor(0x1F2328) : QColor(Qt::white);
}

} // namespace

FrameItem::FrameItem(const QRectF &rect, const FrameStyle &style, double scale)
    : m_rect(rect.normalized())
    , m_style(style)
    , m_scale(scale)
    , m_order(nextOrder())
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
    const bool numberingChanged = style.badge == BadgePosition::None
                                  || m_style.badge == BadgePosition::None;
    prepareGeometryChange();
    m_style = style;
    // Turning a number on or off renumbers the other frames.
    if (numberingChanged && scene())
        scene()->update();
}

bool FrameItem::isHiddenHandle(Handle handle) const
{
    // The corner handle under the number badge would hide the number.
    switch (m_style.badge) {
    case BadgePosition::TopRight: return handle == TopRight;
    case BadgePosition::TopLeft: return handle == TopLeft;
    case BadgePosition::BottomRight: return handle == BottomRight;
    case BadgePosition::BottomLeft: return handle == BottomLeft;
    case BadgePosition::None: break;
    }
    return false;
}

QRectF FrameItem::badgeRect() const
{
    if (!isNumbered())
        return {};
    // Centred on the corner of the line, like the mockup.
    const qreal inset = penWidth() / 2;
    const QRectF line = m_rect.adjusted(inset, inset, -inset, -inset);
    QPointF corner;
    switch (m_style.badge) {
    case BadgePosition::TopRight: corner = line.topRight(); break;
    case BadgePosition::TopLeft: corner = line.topLeft(); break;
    case BadgePosition::BottomRight: corner = line.bottomRight(); break;
    case BadgePosition::BottomLeft: corner = line.bottomLeft(); break;
    case BadgePosition::None: break;
    }
    const qreal d = FrameStyle::kBadgeDiameter * m_scale;
    return QRectF(corner - QPointF(d / 2, d / 2), QSizeF(d, d));
}

int FrameItem::number() const
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    return editorScene ? editorScene->numberOf(this) : 0;
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
    return m_rect.adjusted(-margin, -margin, margin, margin).united(badgeRect().adjusted(-2, -2, 2, 2));
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
    if (isNumbered())
        path.addEllipse(badgeRect());
    if (showsHandles()) {
        const qreal h = screenToItem(kHandleSize + 4);
        for (int i = TopLeft; i <= Left; ++i) {
            if (!isHiddenHandle(Handle(i)))
                path.addRect(QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)));
        }
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

    if (isNumbered()) {
        const QRectF badge = badgeRect();
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_style.color);
        painter->drawEllipse(badge);
        QFont font = painter->font();
        font.setPixelSize(qRound(FrameStyle::kBadgeFontSize * m_scale));
        font.setWeight(QFont::Bold);
        painter->setFont(font);
        painter->setPen(badgeTextColor(m_style.color));
        painter->drawText(badge, Qt::AlignCenter, QString::number(number()));
    }

    if (!showsHandles())
        return;
    const qreal h = screenToItem(kHandleSize);
    QPen handlePen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5));
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    for (int i = TopLeft; i <= Left; ++i) {
        if (!isHiddenHandle(Handle(i)))
            painter->drawRect(QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)));
    }
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
        if (!isHiddenHandle(Handle(i))
            && QRectF(handlePoint(Handle(i)) - QPointF(h / 2, h / 2), QSizeF(h, h)).contains(pos))
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
