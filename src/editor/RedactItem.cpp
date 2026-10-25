#include "editor/RedactItem.h"

#include "editor/EditorScene.h"
#include "imaging/Blur.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <cmath>

RedactItem::RedactItem(const QRectF &rect, const RedactStyle &style, double scale)
    : RectItem(rect, scale, true)
    , m_style(style)
{
    // Above the dimmed background, so the dim layer never paints over it;
    // below frames and numbers.
    setZValue(-0.25);
}

QString RedactItem::label() const
{
    return m_style.mode == RedactMode::Pixelate ? QCoreApplication::translate("RedactItem", "Verpixelt")
                                                : QCoreApplication::translate("RedactItem", "Weichgezeichnet");
}

void RedactItem::setStyle(const RedactStyle &style)
{
    if (style == m_style)
        return;
    m_style = style;
    update();
}

QRectF RedactItem::boundingRect() const
{
    const qreal m = handleMargin();
    return rect().adjusted(-m, -m, m, m);
}

QPainterPath RedactItem::shape() const
{
    // The whole area is clickable, unlike a hollow frame.
    QPainterPath path;
    path.addRect(rect());
    addHandlesToShape(path);
    return path;
}

QImage RedactItem::obscure(const QImage &image, const QRect &region, const RedactStyle &style, double scale)
{
    const QRect area = region.intersected(image.rect());
    if (area.isEmpty())
        return {};
    const int size = std::max(1, int(std::lround(style.size() * scale)));

    if (style.mode == RedactMode::Pixelate) {
        // Average each block (smooth downscale), then blow it up again
        // without smoothing: hard-edged squares.
        const QImage source = image.copy(area);
        const QSize blocks((area.width() + size - 1) / size, (area.height() + size - 1) / size);
        const QImage small = source.scaled(blocks, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return small.scaled(blocks * size, Qt::IgnoreAspectRatio, Qt::FastTransformation)
            .copy(0, 0, area.width(), area.height());
    }

    // Blur with some surrounding image, so the edges are as soft as the middle.
    const int margin = 3 * size;
    const QRect padded = area.adjusted(-margin, -margin, margin, margin).intersected(image.rect());
    const QImage blurred = Blur::boxBlur(image.copy(padded), size);
    return blurred.copy(area.translated(-padded.topLeft()));
}

QPixmap RedactItem::obscured() const
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    if (!editorScene)
        return {};
    const QRect region = mapRectToScene(rect()).toAlignedRect();
    const QImage base = editorScene->baseImage();
    if (m_cache.isNull() || region != m_cacheRegion || !(m_style == m_cacheStyle) || base.cacheKey() != m_cacheImage) {
        m_cache = QPixmap::fromImage(obscure(base, region, m_style, imageScale()));
        m_cacheRegion = region;
        m_cacheStyle = m_style;
        m_cacheImage = base.cacheKey();
    }
    return m_cache;
}

void RedactItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    const auto *editorScene = qobject_cast<const EditorScene *>(scene());
    const QPixmap pixmap = obscured();
    if (!pixmap.isNull()) {
        painter->save();
        // Replace, not blend: nothing of the original may show through.
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, editorScene && editorScene->smoothBase());
        painter->drawPixmap(rect().topLeft(), pixmap);
        painter->restore();

        // Outside the frames the dimmed background darkens this area too.
        if (editorScene && editorScene->dimSettings().enabled) {
            painter->save();
            painter->setClipPath(mapFromScene(editorScene->dimmedArea()));
            painter->fillRect(rect(), editorScene->dimVeil());
            painter->restore();
        }
    }

    if (isSelected() && editorScene && !editorScene->isExporting()) {
        QPen outline(scene()->palette().color(QPalette::Highlight), screenToItem(1.5), Qt::DashLine);
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(rect());
    }
    paintHandles(painter);
}
