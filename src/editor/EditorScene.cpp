#include "editor/EditorScene.h"

#include "editor/FrameItem.h"

#include <QGraphicsPixmapItem>
#include <QPainter>

#include <algorithm>
#include <cmath>

EditorScene::EditorScene(QUndoStack *undoStack, QObject *parent)
    : QGraphicsScene(parent)
    , m_undoStack(undoStack)
{
    m_frameStyle.badge = BadgePosition::TopRight;
    // Numbers depend on the other frames: repaint all when one comes or goes.
    connect(this, &EditorScene::annotationsChanged, this, [this] { update(); });
}

void EditorScene::setBaseImage(const QImage &image, double scale)
{
    clearAll();
    m_baseImage = image;
    m_scale = scale;
    m_base = addPixmap(QPixmap::fromImage(image));
    m_base->setZValue(-1); // always below the annotations
    setSceneRect(m_base->boundingRect());
}

void EditorScene::clearAll()
{
    clear(); // deletes all items, including m_base
    m_base = nullptr;
    m_baseImage = QImage();
    m_scale = 1.0;
    setSceneRect(QRectF());
    emit annotationsChanged();
}

QImage EditorScene::renderImage()
{
    if (!hasImage())
        return {};
    QImage out(m_baseImage.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    m_exporting = true; // FrameItem hides its handles
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    render(&painter, QRectF(out.rect()), sceneRect());
    painter.end();
    m_exporting = false;

    QImage result = out.convertToFormat(QImage::Format_ARGB32);
    setScaleMetadata(result, m_scale);
    return result;
}

namespace {
constexpr double kDotsPerMeterAt96Dpi = 96.0 / 0.0254;
}

void EditorScene::setScaleMetadata(QImage &image, double scale)
{
    const int dpm = int(std::lround(kDotsPerMeterAt96Dpi * scale));
    image.setDotsPerMeterX(dpm);
    image.setDotsPerMeterY(dpm);
}

double EditorScene::scaleFromMetadata(const QImage &image)
{
    // Round to quarter steps (1.25, 1.5, 2 …); anything odd counts as 1.
    const double scale = std::round(image.dotsPerMeterX() / kDotsPerMeterAt96Dpi * 4.0) / 4.0;
    return (scale >= 1.0 && scale <= 4.0) ? scale : 1.0;
}

QList<FrameItem *> EditorScene::frames() const
{
    QList<FrameItem *> result;
    for (QGraphicsItem *item : items()) {
        if (item->type() == FrameItem::Type)
            result << static_cast<FrameItem *>(item);
    }
    std::sort(result.begin(), result.end(),
              [](const FrameItem *a, const FrameItem *b) { return a->order() < b->order(); });
    return result;
}

int EditorScene::numberOf(const FrameItem *frame) const
{
    if (!frame->isNumbered())
        return 0;
    int number = 0;
    for (const FrameItem *other : frames()) {
        if (other->isNumbered())
            ++number;
        if (other == frame)
            return number;
    }
    return 0;
}

QList<FrameItem *> EditorScene::selectedFrames() const
{
    QList<FrameItem *> result = frames();
    result.removeIf([](const FrameItem *frame) { return !frame->isSelected(); });
    return result;
}
