#include "editor/FrameItem.h"

#include "editor/EditorScene.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>

namespace {

constexpr qreal kGrabMargin = 6.0; // screen pixels around the line that still grab it

// Dark text on light badge colours (yellow), white otherwise.
QColor badgeTextColor(const QColor &background)
{
    return background.lightnessF() > 0.6 ? QColor(0x1F2328) : QColor(Qt::white);
}

} // namespace

FrameItem::FrameItem(const QRectF &rect, const FrameStyle &style, double scale)
    : RectItem(rect, scale)
    , m_style(style)
{
}

QString FrameItem::label() const
{
    return isNumbered() ? QCoreApplication::translate("FrameItem", "Nummer %1").arg(number())
                        : QCoreApplication::translate("FrameItem", "Rahmen");
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
    const QRectF line = rect().adjusted(inset, inset, -inset, -inset);
    QPointF corner;
    switch (position) {
    case BadgePosition::TopRight: corner = line.topRight(); break;
    case BadgePosition::TopLeft: corner = line.topLeft(); break;
    case BadgePosition::BottomRight: corner = line.bottomRight(); break;
    case BadgePosition::BottomLeft: corner = line.bottomLeft(); break;
    case BadgePosition::None: break;
    }
    const qreal d = FrameStyle::kBadgeDiameter * imageScale();
    return QRectF(corner - QPointF(d / 2, d / 2), QSizeF(d, d));
}

int FrameItem::number() const
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    return editorScene ? editorScene->numberOf(this) : 0;
}

QRectF FrameItem::boundingRect() const
{
    const qreal margin = std::max(penWidth(), handleMargin());
    QRectF bounds = rect().adjusted(-margin, -margin, margin, margin);
    // All four corners, because the badge can switch corners while the
    // frame moves (effectiveBadge), without a geometry change.
    if (isNumbered()) {
        const qreal r = FrameStyle::kBadgeDiameter * imageScale() / 2 + 2;
        bounds = bounds.united(rect().adjusted(-r, -r, r, r));
    }
    return bounds;
}

QPainterPath FrameItem::shape() const
{
    // Only the line (plus a margin) is clickable, so frames inside other
    // frames and the image underneath stay reachable.
    QPainterPath outline;
    outline.addRect(rect());
    QPainterPathStroker stroker;
    stroker.setWidth(penWidth() + 2 * screenToItem(kGrabMargin));
    QPainterPath path = stroker.createStroke(outline);
    if (isNumbered())
        path.addEllipse(badgeRect());
    addHandlesToShape(path);
    return path;
}

void FrameItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setRenderHint(QPainter::Antialiasing);

    // The line lies inside the rectangle, like a CSS border.
    const qreal w = penWidth();
    const QRectF line = rect().adjusted(w / 2, w / 2, -w / 2, -w / 2);
    painter->setPen(QPen(m_style.color, w, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
    painter->setBrush(Qt::NoBrush);
    if (m_style.rounded) {
        const qreal radius = std::min({FrameStyle::kRadius * imageScale(), line.width() / 2, line.height() / 2});
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
        font.setPixelSize(qRound(FrameStyle::kBadgeFontSize * imageScale()));
        font.setWeight(QFont::Bold);
        painter->setFont(font);
        painter->setPen(badgeTextColor(m_style.color));
        painter->drawText(badge, Qt::AlignCenter, QString::number(number()));
    }

    paintHandles(painter);
}
