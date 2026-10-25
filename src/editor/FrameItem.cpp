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
    switch (effectiveBadge()) {
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
    const BadgePosition position = effectiveBadge();
    QRectF badge = badgeRectAt(position);
    // If no corner fits (frame about as large as the image), the circle
    // moves inwards until it is on the image.
    if (scene() && !badgeFits(position)) {
        const QRectF bounds = mapRectFromScene(scene()->sceneRect());
        badge.moveLeft(std::clamp(badge.left(), bounds.left(), std::max(bounds.left(), bounds.right() - badge.width())));
        badge.moveTop(std::clamp(badge.top(), bounds.top(), std::max(bounds.top(), bounds.bottom() - badge.height())));
    }
    return badge;
}

bool FrameItem::badgeFits(BadgePosition position) const
{
    // Fits if the whole circle is on the image, so the export shows it.
    return !scene() || scene()->sceneRect().contains(mapRectToScene(badgeRectAt(position)));
}

QList<BadgePosition> FrameItem::fittingBadgePositions() const
{
    QList<BadgePosition> result;
    for (BadgePosition position : {BadgePosition::TopRight, BadgePosition::TopLeft,
                                   BadgePosition::BottomRight, BadgePosition::BottomLeft}) {
        if (badgeFits(position))
            result << position;
    }
    return result;
}

BadgePosition FrameItem::effectiveBadge() const
{
    const BadgePosition wanted = m_style.badge;
    if (wanted == BadgePosition::None || badgeFits(wanted))
        return wanted;
    // Nearest corner that fits: same edge mirrored first, then the other
    // edge, then the opposite corner. The wish is kept in the style, so the
    // number returns when the frame moves away from the edge.
    const auto mirrorX = [](BadgePosition p) {
        switch (p) {
        case BadgePosition::TopRight: return BadgePosition::TopLeft;
        case BadgePosition::TopLeft: return BadgePosition::TopRight;
        case BadgePosition::BottomRight: return BadgePosition::BottomLeft;
        case BadgePosition::BottomLeft: return BadgePosition::BottomRight;
        case BadgePosition::None: break;
        }
        return p;
    };
    const auto mirrorY = [](BadgePosition p) {
        switch (p) {
        case BadgePosition::TopRight: return BadgePosition::BottomRight;
        case BadgePosition::TopLeft: return BadgePosition::BottomLeft;
        case BadgePosition::BottomRight: return BadgePosition::TopRight;
        case BadgePosition::BottomLeft: return BadgePosition::TopLeft;
        case BadgePosition::None: break;
        }
        return p;
    };
    for (BadgePosition candidate : {mirrorX(wanted), mirrorY(wanted), mirrorX(mirrorY(wanted))}) {
        if (badgeFits(candidate))
            return candidate;
    }
    return wanted; // nothing fits (frame as large as the image): see badgeRect
}

QRectF FrameItem::badgeRectAt(BadgePosition position) const
{
    // Centred on the corner of the line, like the mockup.
    const qreal inset = penWidth() / 2;
    const QRectF line = m_rect.adjusted(inset, inset, -inset, -inset);
    QPointF corner;
    switch (position) {
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
    QRectF bounds = m_rect.adjusted(-margin, -margin, margin, margin);
    // All four corners, because the badge can switch corners while the
    // frame moves (effectiveBadge), without a geometry change.
    if (isNumbered()) {
        const qreal r = FrameStyle::kBadgeDiameter * m_scale / 2 + 2;
        bounds = bounds.united(m_rect.adjusted(-r, -r, r, r));
    }
    return bounds;
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

QVariant FrameItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene()) {
        // Shift the new position so the frame stays inside the image.
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
    // Stay inside the image: its rectangle in item coordinates.
    if (scene())
        r = r.normalized().intersected(scene()->sceneRect().translated(-pos()));
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
