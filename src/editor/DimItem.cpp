#include "editor/DimItem.h"

#include "editor/EditorScene.h"
#include "editor/FrameItem.h"

#include <QPainter>

#include <algorithm>

DimItem::DimItem(EditorScene *scene)
    : m_scene(scene)
{
    setZValue(-0.5); // above the base image (-1), below the annotations (0)
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
}

QRectF DimItem::boundingRect() const
{
    return m_scene->sceneRect();
}

QPainterPath DimItem::dimmedArea() const
{
    QPainterPath area;
    area.addRect(m_scene->sceneRect());

    // Each frame cuts out its outer outline, rounded like the frame.
    QPainterPath cutouts;
    for (const FrameItem *frame : m_scene->frames()) {
        const QRectF rect = frame->mapRectToScene(frame->rect());
        QPainterPath cut;
        if (frame->style().rounded) {
            const qreal r = std::min({FrameStyle::kRadius * m_scene->imageScale(), rect.width() / 2, rect.height() / 2});
            cut.addRoundedRect(rect, r, r);
        } else {
            cut.addRect(rect);
        }
        cutouts = cutouts.united(cut);
    }
    return area.subtracted(cutouts);
}

void DimItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const DimSettings dim = m_scene->dimSettings();
    if (!dim.enabled)
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setClipPath(dimmedArea());
    // The layer is the whole image already blurred and darkened (cached in
    // the scene); here it is only drawn outside the frames. SourceAtop paints
    // only where the image already is: the blur spreads into the transparent
    // window corners, which must stay transparent (D-009).
    painter->setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter->drawPixmap(QPointF(0, 0), m_scene->dimLayer());
    painter->restore();
}
