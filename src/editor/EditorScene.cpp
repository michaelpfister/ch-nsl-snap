#include "editor/EditorScene.h"

#include "editor/DimItem.h"
#include "editor/FrameItem.h"
#include "editor/RectItem.h"
#include "imaging/Blur.h"

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
    m_base->setTransformationMode(m_smoothBase ? Qt::SmoothTransformation : Qt::FastTransformation);
    setSceneRect(m_base->boundingRect());
    m_dimItem = new DimItem(this);
    addItem(m_dimItem);
}

void EditorScene::setDimSettings(const DimSettings &settings)
{
    if (settings == m_dim)
        return;
    m_dim = settings;
    update();
    emit dimSettingsChanged();
}

QPixmap EditorScene::dimLayer() const
{
    if (!m_dimLayer.isNull() && m_dimLayerSettings == m_dim)
        return m_dimLayer;

    const int radius = int(std::lround(m_dim.blur * m_scale));
    if (!m_blurred.contains(radius))
        m_blurred.insert(radius, Blur::boxBlur(m_baseImage, radius)
                                     .convertToFormat(QImage::Format_ARGB32_Premultiplied));
    QImage layer = m_blurred.value(radius);

    // The veil from the mockup; SourceAtop keeps transparent corners clear.
    QPainter painter(&layer);
    painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);
    painter.fillRect(layer.rect(), dimVeil());
    painter.end();

    m_dimLayer = QPixmap::fromImage(layer);
    m_dimLayerSettings = m_dim;
    return m_dimLayer;
}

void EditorScene::clearAll()
{
    clear(); // deletes all items, including m_base and m_dimItem
    m_base = nullptr;
    m_dimItem = nullptr;
    m_baseImage = QImage();
    m_scale = 1.0;
    m_blurred.clear();
    m_dimLayer = QPixmap();
    // A new image starts without the dim effect.
    m_dim.enabled = false;
    emit dimSettingsChanged();
    setSceneRect(QRectF());
    emit annotationsChanged();
}

void EditorScene::setSmoothBase(bool smooth)
{
    if (smooth == m_smoothBase)
        return;
    m_smoothBase = smooth;
    if (m_base)
        m_base->setTransformationMode(smooth ? Qt::SmoothTransformation : Qt::FastTransformation);
    update();
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

QPainterPath EditorScene::dimmedArea() const
{
    QPainterPath area;
    area.addRect(sceneRect());

    // Each frame cuts out its outer outline, rounded like the frame.
    QPainterPath cutouts;
    for (const FrameItem *frame : frames()) {
        const QRectF rect = frame->mapRectToScene(frame->rect());
        QPainterPath cut;
        if (frame->style().rounded) {
            const qreal r = std::min({FrameStyle::kRadius * imageScale(), rect.width() / 2, rect.height() / 2});
            cut.addRoundedRect(rect, r, r);
        } else {
            cut.addRect(rect);
        }
        cutouts = cutouts.united(cut);
    }
    return area.subtracted(cutouts);
}

QColor EditorScene::dimVeil() const
{
    return QColor(12, 14, 18, qRound(m_dim.strength * 2.55));
}

QList<RectItem *> EditorScene::annotations() const
{
    QList<RectItem *> result;
    for (QGraphicsItem *item : items()) {
        if (auto *annotation = dynamic_cast<RectItem *>(item))
            result << annotation;
    }
    std::sort(result.begin(), result.end(),
              [](const RectItem *a, const RectItem *b) { return a->order() < b->order(); });
    return result;
}

QList<RectItem *> EditorScene::selectedAnnotations() const
{
    QList<RectItem *> result = annotations();
    result.removeIf([](const RectItem *item) { return !item->isSelected(); });
    return result;
}

QList<FrameItem *> EditorScene::frames() const
{
    QList<FrameItem *> result;
    for (RectItem *item : annotations()) {
        if (item->type() == FrameItem::Type)
            result << static_cast<FrameItem *>(item);
    }
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
