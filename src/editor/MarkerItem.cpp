#include "editor/MarkerItem.h"

#include "editor/EditorScene.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPalette>

MarkerItem::MarkerItem(const QRectF &rect, const MarkerStyle &style, double scale)
    : RectItem(rect, scale)
    , m_style(style)
{
    // On the image, under the dim layer and the other annotations: like
    // ink on paper, dimmed together with the background (F-27).
    setZValue(-0.75);
}

QString MarkerItem::label() const
{
    return QCoreApplication::translate("MarkerItem", "Marker");
}

void MarkerItem::setStyle(const MarkerStyle &style)
{
    if (style == m_style)
        return;
    m_style = style;
    update();
}

QRectF MarkerItem::boundingRect() const
{
    const qreal m = handleMargin();
    return rect().adjusted(-m, -m, m, m);
}

QPainterPath MarkerItem::shape() const
{
    QPainterPath path;
    path.addRect(rect());
    addHandlesToShape(path);
    return path;
}

void MarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // Multiply: the result is image colour × marker colour.
    painter->setCompositionMode(QPainter::CompositionMode_Multiply);
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_style.color);
    const qreal radius = 2 * imageScale(); // slightly soft, like a pen tip
    painter->drawRoundedRect(rect(), radius, radius);
    painter->restore();

    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    if (isSelected() && editorScene && !editorScene->isExporting()) {
        painter->setPen(QPen(scene()->palette().color(QPalette::Highlight), screenToItem(1.5), Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect());
    }
    paintHandles(painter);
}
