#include "editor/EditorView.h"

#include "editor/ArrowItem.h"
#include "editor/Commands.h"
#include "editor/EditorScene.h"
#include "editor/FrameItem.h"
#include "editor/MarkerItem.h"
#include "editor/RedactItem.h"
#include "editor/ReplaceItem.h"
#include "editor/TextItem.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QUndoStack>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

// A drawn frame smaller than this (screen pixels) counts as a click.
constexpr int kMinDrawSize = 6;
// Crop handles (screen pixels), as large as those of the annotations.
constexpr qreal kCropHandleSize = 8.0;

// The eight handle positions of a rectangle: top left, then clockwise.
QList<QPointF> handlePoints(const QRectF &r)
{
    return {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(), QPointF(r.right(), r.center().y()),
            r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
}

// A crop is made of whole image pixels.
QRect snappedRect(const QRectF &rect)
{
    const QRectF r = rect.normalized();
    const int left = int(std::lround(r.left()));
    const int top = int(std::lround(r.top()));
    return QRect(left, top, int(std::lround(r.right())) - left, int(std::lround(r.bottom())) - top);
}

bool firstLocalFile(const QMimeData *mime, QString *path)
{
    if (!mime || !mime->hasUrls())
        return false;
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile()) {
            *path = url.toLocalFile();
            return true;
        }
    }
    return false;
}

} // namespace

EditorView::EditorView(QUndoStack *undoStack, QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new EditorScene(undoStack, this))
{
    setScene(m_scene);
    setAcceptDrops(true);
    setDragMode(QGraphicsView::RubberBandDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor); // applyZoom anchors itself
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setBackgroundRole(QPalette::Dark);
    // Handles keep their screen size; redraw everything when they move.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    // Mouse moves without a button pressed, for the crop handle cursors.
    viewport()->setMouseTracking(true);

    // The view follows the scene rect (the crop), except while cropping.
    connect(m_scene, &EditorScene::cropChanged, this, [this] {
        if (m_tool == ToolId::Crop && hasImage()) {
            m_cropDraft = m_scene->cropRect(); // e.g. undo while cropping
            emit cropDraftChanged(m_cropDraft);
        } else if (m_fitMode) {
            updateFit();
        }
        viewport()->update();
    });
    // A text edit ends when its editor loses the keyboard focus: a click
    // elsewhere, Escape, or another widget or window. Queued, because the
    // scene is still changing the focus when it emits this.
    connect(m_scene, &QGraphicsScene::focusItemChanged, this, [this] {
        if (m_editingText && m_scene->focusItem() != m_editingText->editor())
            finishTextEditing();
    }, Qt::QueuedConnection);
}

void EditorView::setImage(const QImage &image, double scale)
{
    dropTextEditing();
    m_scene->setBaseImage(image, scale);
    if (m_tool == ToolId::Crop)
        beginCropEditing();
    fitToWindow();
}

void EditorView::clear()
{
    endColorPick(QColor());
    dropTextEditing();
    m_scene->clearAll();
    m_cropDraft = QRect();
    m_cropDrag = CropDrag::None;
    setSceneRect(QRectF());
    emit cropDraftChanged(m_cropDraft);
    resetTransform();
    viewport()->update();
    emit zoomChanged();
}

bool EditorView::hasImage() const
{
    return m_scene->hasImage();
}

QImage EditorView::renderedImage()
{
    finishTextEditing();
    return m_scene->renderImage();
}

double EditorView::zoom() const
{
    return transform().m11() * m_scene->imageScale();
}

void EditorView::setTool(ToolId tool)
{
    if (tool == m_tool)
        return;
    endColorPick(QColor());
    finishTextEditing();
    if (m_tool == ToolId::Crop)
        endCropEditing();
    m_tool = tool;
    if (m_tool == ToolId::Crop && hasImage())
        beginCropEditing();
    setDragMode(isDrawingTool() || m_tool == ToolId::Crop || m_tool == ToolId::Text ? QGraphicsView::NoDrag : QGraphicsView::RubberBandDrag);
    restoreCursor();
}

bool EditorView::isDrawingTool() const
{
    // Tools that draw a new annotation; the others follow (docs/04-features.md).
    return m_tool == ToolId::Frame || m_tool == ToolId::Number || m_tool == ToolId::Pixelate
           || m_tool == ToolId::ReplaceText || m_tool == ToolId::Arrow || m_tool == ToolId::Marker;
}

// --- Free text (F-25) ---------------------------------------------------

void EditorView::startTextEditing(TextItem *item, bool isNew)
{
    finishTextEditing();
    m_editingText = item;
    m_editingIsNew = isNew;
    m_textBefore = item->style().text;
    m_textPosBefore = item->pos();
    m_scene->clearSelection();
    item->setSelected(true);
    setFocus();
    item->setEditing(true);
}

void EditorView::finishTextEditing()
{
    if (!m_editingText)
        return;
    TextItem *item = m_editingText;
    m_editingText = nullptr; // first: the focus change below calls back
    item->setEditing(false);
    // A text that grew beyond the image is moved back into it.
    item->keepInsideScene();

    const bool empty = item->style().text.trimmed().isEmpty();
    if (m_editingIsNew) {
        m_scene->removeItem(item); // AddItemsCommand puts it back
        if (empty) {
            delete item; // just a click
            return;
        }
        m_scene->undoStack()->push(new AddItemsCommand(m_scene, {item}, tr("Text schreiben")));
        emit annotationCreated(item);
    } else if (empty) {
        // Emptied: delete it, but undo brings back the old text.
        TextStyle style = item->style();
        style.text = m_textBefore;
        item->setStyle(style);
        item->setPos(m_textPosBefore);
        m_scene->undoStack()->push(new RemoveItemsCommand(m_scene, {item}));
    } else if (item->style().text != m_textBefore || item->pos() != m_textPosBefore) {
        m_scene->undoStack()->push(new EditTextCommand(item, m_textBefore, m_textPosBefore));
    }
}

void EditorView::dropTextEditing()
{
    if (m_editingText)
        m_editingText->setEditing(false);
    m_editingText = nullptr;
}

// --- Eyedropper ----------------------------------------------------------

void EditorView::startColorPick()
{
    if (!hasImage()) {
        emit colorPicked(QColor());
        return;
    }
    finishTextEditing();
    m_pickingColor = true;
    m_pickPos = viewport()->mapFromGlobal(QCursor::pos());
    viewport()->setCursor(Qt::CrossCursor);
    setFocus();
    viewport()->update();
}

void EditorView::endColorPick(const QColor &color)
{
    if (!m_pickingColor)
        return;
    m_pickingColor = false;
    restoreCursor();
    viewport()->update();
    emit colorPicked(color);
}

QColor EditorView::colorAt(const QPoint &viewPos) const
{
    const QPointF scenePos = mapToScene(viewPos);
    const QPoint pixel(int(std::floor(scenePos.x())), int(std::floor(scenePos.y())));
    // The base image as captured: annotations and the dim layer are not
    // part of the colours of the screenshot.
    if (!m_scene->cropRect().contains(pixel))
        return {};
    QColor color = m_scene->baseImage().pixelColor(pixel);
    color.setAlpha(255);
    return color;
}

void EditorView::drawColorPickLoupe(QPainter *painter)
{
    const QColor color = colorAt(m_pickPos);
    if (!color.isValid())
        return;
    // 11 × 11 image pixels, 8 screen pixels each, next to the cursor.
    constexpr int kPixels = 11;
    constexpr int kCell = 8;
    constexpr int kSize = kPixels * kCell;
    const QPointF scenePos = mapToScene(m_pickPos);
    const QPoint center(int(std::floor(scenePos.x())), int(std::floor(scenePos.y())));
    const QImage source = m_scene->baseImage().copy(center.x() - kPixels / 2, center.y() - kPixels / 2, kPixels, kPixels);

    painter->save();
    painter->resetTransform(); // viewport coordinates
    QPoint topLeft = m_pickPos + QPoint(20, 20);
    if (topLeft.x() + kSize > viewport()->width())
        topLeft.rx() = m_pickPos.x() - 20 - kSize;
    if (topLeft.y() + kSize + 28 > viewport()->height())
        topLeft.ry() = m_pickPos.y() - 20 - kSize - 28;
    const QRect box(topLeft, QSize(kSize, kSize));
    QPainterPath clip;
    clip.addRoundedRect(QRectF(box), 8, 8);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillPath(clip, palette().color(QPalette::Dark));
    painter->setClipPath(clip);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(box, source);
    painter->setClipping(false);
    // The picked pixel, and a frame around the loupe.
    const QRect middle(box.topLeft() + QPoint(kPixels / 2 * kCell, kPixels / 2 * kCell), QSize(kCell, kCell));
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(Qt::white, 1));
    painter->drawRect(middle);
    painter->setPen(QPen(QColor(0, 0, 0, 160), 1));
    painter->drawRect(middle.adjusted(-1, -1, 1, 1));
    painter->setPen(QPen(palette().color(QPalette::Mid), 1));
    painter->drawPath(clip);

    // Colour and hex code below it.
    const QRect label(box.left(), box.bottom() + 6, kSize, 22);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 190));
    painter->drawRoundedRect(label, 6, 6);
    painter->setBrush(color);
    painter->setPen(QPen(Qt::white, 1));
    painter->drawEllipse(QRect(label.left() + 6, label.top() + 5, 12, 12));
    painter->setPen(Qt::white);
    QFont font = painter->font();
    font.setPixelSize(12);
    painter->setFont(font);
    painter->drawText(label.adjusted(24, 0, 0, 0), Qt::AlignVCenter, color.name().toUpper());
    painter->restore();
}

// --- Crop (F-12) --------------------------------------------------------

void EditorView::beginCropEditing()
{
    // Show the whole image, so the crop can also grow again.
    m_scene->clearSelection();
    m_cropDraft = m_scene->cropRect();
    m_cropDrag = CropDrag::None;
    setSceneRect(QRectF(m_scene->baseImage().rect()));
    if (m_fitMode)
        updateFit();
    viewport()->update();
    emit cropDraftChanged(m_cropDraft);
}

void EditorView::endCropEditing()
{
    // Leaving the tool keeps what the user set up, like Enter.
    applyCrop();
    m_cropDrag = CropDrag::None;
    setSceneRect(QRectF()); // follow the scene rect, i.e. the crop, again
    if (m_fitMode)
        updateFit();
    viewport()->update();
}

void EditorView::setCropDraft(const QRect &rect)
{
    if (m_tool != ToolId::Crop || !hasImage())
        return;
    const QRect draft = rect.intersected(m_scene->baseImage().rect());
    if (draft.isEmpty() || draft == m_cropDraft)
        return;
    m_cropDraft = draft;
    viewport()->update();
    emit cropDraftChanged(m_cropDraft);
}

void EditorView::applyCrop()
{
    if (m_tool != ToolId::Crop || !hasImage() || m_cropDraft.isEmpty() || m_cropDraft == m_scene->cropRect())
        return;
    m_scene->undoStack()->push(new SetCropCommand(m_scene, m_cropDraft));
}

void EditorView::cancelCrop()
{
    if (m_tool != ToolId::Crop || !hasImage())
        return;
    m_cropDrag = CropDrag::None;
    m_cropDraft = m_scene->cropRect();
    viewport()->update();
    emit cropDraftChanged(m_cropDraft);
}

int EditorView::cropHandleAt(const QPoint &viewPos) const
{
    const QList<QPointF> points = handlePoints(QRectF(m_cropDraft));
    const qreal grab = (kCropHandleSize + 4) / 2; // a little larger than drawn
    for (int i = 0; i < points.size(); ++i) {
        const QPointF p = mapFromScene(points[i]);
        if (std::abs(p.x() - viewPos.x()) <= grab && std::abs(p.y() - viewPos.y()) <= grab)
            return i;
    }
    return -1;
}

void EditorView::updateCropDrag(const QPointF &scenePos)
{
    // Everything stays on the image.
    const QRectF bounds(m_scene->baseImage().rect());
    const QPointF p(std::clamp(scenePos.x(), bounds.left(), bounds.right()),
                    std::clamp(scenePos.y(), bounds.top(), bounds.bottom()));
    QRect draft = m_cropDraft;
    switch (m_cropDrag) {
    case CropDrag::New: {
        const QRect r = snappedRect(QRectF(m_cropPress, p));
        const QSizeF onScreen = transform().mapRect(QRectF(r)).size();
        // A tiny drag is a click: keep the previous crop.
        draft = (onScreen.width() < kMinDrawSize || onScreen.height() < kMinDrawSize) ? m_cropAtPress : r;
        break;
    }
    case CropDrag::Move: {
        const QPointF delta = scenePos - m_cropPress;
        draft = m_cropAtPress.translated(int(std::lround(delta.x())), int(std::lround(delta.y())));
        // Push it back inside the image, keeping its size.
        const QRect image = m_scene->baseImage().rect();
        draft.moveLeft(std::clamp(draft.left(), image.left(), image.right() + 1 - draft.width()));
        draft.moveTop(std::clamp(draft.top(), image.top(), image.bottom() + 1 - draft.height()));
        break;
    }
    case CropDrag::Handle: {
        QRectF r(m_cropAtPress);
        switch (m_cropHandle) {
        case 0: r.setTopLeft(p); break;
        case 1: r.setTop(p.y()); break;
        case 2: r.setTopRight(p); break;
        case 3: r.setRight(p.x()); break;
        case 4: r.setBottomRight(p); break;
        case 5: r.setBottom(p.y()); break;
        case 6: r.setBottomLeft(p); break;
        case 7: r.setLeft(p.x()); break;
        default: break;
        }
        const QRect snapped = snappedRect(r);
        if (snapped.width() >= 1 && snapped.height() >= 1)
            draft = snapped;
        break;
    }
    case CropDrag::None:
        return;
    }
    if (draft != m_cropDraft) {
        m_cropDraft = draft;
        viewport()->update();
        emit cropDraftChanged(m_cropDraft);
    }
}

void EditorView::updateCropCursor(const QPoint &viewPos)
{
    switch (cropHandleAt(viewPos)) {
    case 0:
    case 4: viewport()->setCursor(Qt::SizeFDiagCursor); return;
    case 2:
    case 6: viewport()->setCursor(Qt::SizeBDiagCursor); return;
    case 1:
    case 5: viewport()->setCursor(Qt::SizeVerCursor); return;
    case 3:
    case 7: viewport()->setCursor(Qt::SizeHorCursor); return;
    default: break;
    }
    const bool inside = QRectF(m_cropDraft).contains(mapToScene(viewPos));
    viewport()->setCursor(inside ? Qt::SizeAllCursor : Qt::CrossCursor);
}

void EditorView::drawCropOverlay(QPainter *painter)
{
    const QRectF draft(m_cropDraft);
    const qreal px = 1.0 / transform().m11(); // one screen pixel in scene units
    painter->save();

    // Darken what will be cut away.
    QPainterPath outside;
    outside.addRect(QRectF(m_scene->baseImage().rect()));
    QPainterPath inside;
    inside.addRect(draft);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 140));
    painter->drawPath(outside.subtracted(inside));

    // Outline and handles keep their screen size.
    const QColor accent = palette().color(QPalette::Highlight);
    QPen outline(accent, 1.5);
    outline.setCosmetic(true);
    painter->setPen(outline);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(draft);
    const qreal h = kCropHandleSize * px;
    painter->setBrush(Qt::white);
    for (const QPointF &point : handlePoints(draft))
        painter->drawRect(QRectF(point - QPointF(h / 2, h / 2), QSizeF(h, h)));
    painter->restore();
}

bool EditorView::deleteSelection()
{
    QList<QGraphicsItem *> items;
    for (RectItem *item : m_scene->selectedAnnotations())
        items << item;
    if (items.isEmpty())
        return false;
    m_scene->undoStack()->push(new RemoveItemsCommand(m_scene, items));
    return true;
}

namespace {

// Steps for zoom in/out (buttons, Ctrl + plus/minus), in percent.
const double kZoomSteps[] = {0.10, 0.25, 0.33, 0.50, 0.67, 0.75, 1.00, 1.25, 1.50,
                             2.00, 3.00, 4.00, 6.00, 8.00};
// Free space around the image in fit mode, in screen pixels.
constexpr int kFitMargin = 24;

} // namespace

void EditorView::fitToWindow()
{
    m_fitMode = true;
    updateFit();
}

void EditorView::updateFit()
{
    if (!hasImage())
        return;
    // Fill the visible area, larger than 100 % too, so the image grows
    // with the window and details become readable (D-017).
    // The visible part: the crop, or the whole image while cropping.
    const QSizeF logical = sceneRect().size() / m_scene->imageScale();
    const QSizeF available = QSizeF(viewport()->size()) - QSizeF(2 * kFitMargin, 2 * kFitMargin);
    const double fit = std::min(available.width() / logical.width(), available.height() / logical.height());
    applyZoom(std::clamp(fit, kMinZoom, kMaxZoom));
    centerOn(sceneRect().center());
}

void EditorView::setZoom(double zoom)
{
    m_fitMode = false;
    applyZoom(zoom);
}

void EditorView::zoomIn()
{
    for (double step : kZoomSteps) {
        if (step > zoom() + 0.001) {
            setZoom(step);
            return;
        }
    }
    setZoom(kMaxZoom);
}

void EditorView::zoomOut()
{
    for (auto it = std::rbegin(kZoomSteps); it != std::rend(kZoomSteps); ++it) {
        if (*it < zoom() - 0.001) {
            setZoom(*it);
            return;
        }
    }
    setZoom(kMinZoom);
}

void EditorView::applyZoom(double zoom, const QPoint *anchor)
{
    if (!hasImage())
        return;
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    const QPoint anchorPos = anchor ? *anchor : viewport()->rect().center();
    const QPointF anchorScene = mapToScene(anchorPos);

    // One scene unit is one image pixel; at 100 % it covers 1/scale logical
    // pixels, i.e. one physical pixel on the screen it was taken from.
    const double m = zoom / m_scene->imageScale();
    setTransform(QTransform::fromScale(m, m));

    // Keep the anchor point under the mouse (or the centre) in place.
    const QPointF moved = mapFromScene(anchorScene) - QPointF(anchorPos);
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + qRound(moved.x()));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + qRound(moved.y()));

    // From two screen pixels per image pixel on, show crisp pixels instead
    // of a blurry upscale, so details can be read exactly.
    m_scene->setSmoothBase(m * devicePixelRatioF() < 2.0);
    for (RectItem *item : m_scene->annotations())
        item->viewZoomChanged();
    emit zoomChanged();
}

QGraphicsItem *EditorView::annotationAt(const QPoint &viewPos) const
{
    for (QGraphicsItem *item : items(viewPos)) {
        if (dynamic_cast<RectItem *>(item))
            return item;
    }
    return nullptr;
}

void EditorView::startPan(const QPoint &pos)
{
    m_panning = true;
    m_panLast = pos;
    viewport()->setCursor(Qt::ClosedHandCursor);
}

void EditorView::restoreCursor()
{
    if (m_pickingColor)
        viewport()->setCursor(Qt::CrossCursor);
    else if (m_spaceHeld)
        viewport()->setCursor(Qt::OpenHandCursor);
    else
        viewport()->setCursor(m_tool == ToolId::Text           ? Qt::IBeamCursor
                              : isDrawingTool() || m_tool == ToolId::Crop ? Qt::CrossCursor
                                                                        : Qt::ArrowCursor);
}

void EditorView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_fitMode)
        updateFit(); // the image grows and shrinks with the window
}

bool EditorView::viewportEvent(QEvent *event)
{
    // Touchpad pinch (Wayland pointer gestures, macOS trackpad).
    if (event->type() == QEvent::NativeGesture) {
        auto *gesture = static_cast<QNativeGestureEvent *>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture && hasImage()) {
            m_fitMode = false;
            const QPoint anchor = gesture->position().toPoint();
            applyZoom(zoom() * (1.0 + gesture->value()), &anchor);
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}

void EditorView::keyPressEvent(QKeyEvent *event)
{
    if (m_pickingColor && event->key() == Qt::Key_Escape) {
        endColorPick(QColor());
        return;
    }
    // While typing a text every key belongs to it, Space included.
    if (m_editingText) {
        QGraphicsView::keyPressEvent(event);
        return;
    }
    // Space + drag pans, for touchpads without a middle button.
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceHeld = true;
        restoreCursor();
        return;
    }
    // "Enter bestätigt" (crop tool).
    if (m_tool == ToolId::Crop && hasImage()
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        applyCrop();
        emit cropConfirmed();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void EditorView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && m_spaceHeld) {
        m_spaceHeld = false;
        m_panning = false;
        restoreCursor();
        return;
    }
    QGraphicsView::keyReleaseEvent(event);
}

void EditorView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && m_spaceHeld)) {
        startPan(event->position().toPoint());
        return;
    }

    // Crop tool: a handle resizes the draft, inside it moves, outside starts
    // a new one. Annotations cannot be picked meanwhile.
    if (m_tool == ToolId::Crop && hasImage()) {
        if (event->button() == Qt::LeftButton) {
            const QPoint viewPos = event->position().toPoint();
            const QPointF scenePos = mapToScene(viewPos);
            const QRectF bounds(m_scene->baseImage().rect());
            m_cropHandle = cropHandleAt(viewPos);
            m_cropAtPress = m_cropDraft;
            if (m_cropHandle >= 0) {
                m_cropDrag = CropDrag::Handle;
                m_cropPress = scenePos;
            } else if (QRectF(m_cropDraft).contains(scenePos)) {
                m_cropDrag = CropDrag::Move;
                m_cropPress = scenePos;
            } else {
                m_cropDrag = CropDrag::New;
                m_cropPress = QPointF(std::clamp(scenePos.x(), bounds.left(), bounds.right()),
                                      std::clamp(scenePos.y(), bounds.top(), bounds.bottom()));
            }
        }
        return;
    }

    // Eyedropper: the click picks, the right button cancels; nothing else
    // happens with it.
    if (m_pickingColor) {
        if (event->button() == Qt::LeftButton) {
            if (const QColor color = colorAt(event->position().toPoint()); color.isValid())
                endColorPick(color);
        } else if (event->button() == Qt::RightButton) {
            endColorPick(QColor());
        }
        return;
    }

    // While typing, a click into the text places the cursor; anywhere else
    // it ends the edit first and then does what it would do anyway.
    if (m_editingText) {
        if (annotationAt(event->position().toPoint()) == m_editingText) {
            QGraphicsView::mousePressEvent(event);
            return;
        }
        finishTextEditing();
    }

    // Text tool (F-25): a click on a text edits it, elsewhere on the image
    // it starts a new one there.
    if (event->button() == Qt::LeftButton && m_tool == ToolId::Text && hasImage()) {
        QGraphicsItem *hit = annotationAt(event->position().toPoint());
        if (hit && hit->type() == TextItem::Type) {
            startTextEditing(static_cast<TextItem *>(hit), false);
            return;
        }
        const QPointF click = mapToScene(event->position().toPoint());
        if (!hit && m_scene->sceneRect().contains(click)) {
            TextStyle style = m_scene->textStyle();
            style.text.clear();
            auto *item = new TextItem(QPointF(), style, m_scene->imageScale());
            // The text starts at the click, its (first and only) line
            // centred on it.
            const qreal p = 4 * m_scene->imageScale(); // TextItem's padding
            item->setPos(click - QPointF(p, item->rect().height() / 2));
            m_scene->addItem(item);
            item->keepInsideScene();
            startTextEditing(item, true);
            return;
        }
    }

    // Frame tool on empty space: start a new frame. On an existing frame the
    // tool behaves like Select, so the frame just drawn can be adjusted.
    // Frames start only on the image; outside it the press selects as usual.
    if (event->button() == Qt::LeftButton && isDrawingTool() && hasImage()
        && m_scene->sceneRect().contains(mapToScene(event->position().toPoint()))
        && !annotationAt(event->position().toPoint())) {
        m_scene->clearSelection();
        m_drawStart = mapToScene(event->position().toPoint());
        const QRectF start(m_drawStart, m_drawStart);
        if (m_tool == ToolId::Arrow) {
            m_drawing = new ArrowItem(QLineF(m_drawStart, m_drawStart), m_scene->arrowStyle(), m_scene->imageScale());
        } else if (m_tool == ToolId::Marker) {
            m_drawing = new MarkerItem(start, m_scene->markerStyle(), m_scene->imageScale());
        } else if (m_tool == ToolId::ReplaceText) {
            ReplaceStyle style;
            style.text = m_scene->replaceText();
            m_drawing = new ReplaceItem(start, style, m_scene->imageScale());
        } else if (m_tool == ToolId::Pixelate) {
            // Alt switches this area to blur ("Alt für Weichzeichnen").
            RedactStyle style = m_scene->redactStyle();
            if (event->modifiers() & Qt::AltModifier)
                style.mode = RedactMode::Blur;
            m_drawing = new RedactItem(start, style, m_scene->imageScale());
        } else {
            // The number tool uses the badge position of the current style;
            // the frame tool draws plain frames.
            FrameStyle style = m_scene->frameStyle();
            if (m_tool == ToolId::Frame)
                style.badge = BadgePosition::None;
            m_drawing = new FrameItem(start, style, m_scene->imageScale());
        }
        m_scene->addItem(m_drawing);
        return;
    }

    QGraphicsView::mousePressEvent(event);
    m_positionsAtPress.clear();
    for (QGraphicsItem *item : m_scene->selectedItems())
        m_positionsAtPress.insert(item, item->pos());
}

void EditorView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        const QPoint delta = event->position().toPoint() - m_panLast;
        m_panLast = event->position().toPoint();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        return;
    }

    if (m_pickingColor) {
        m_pickPos = event->position().toPoint();
        viewport()->update();
        return;
    }

    if (m_tool == ToolId::Crop && hasImage()) {
        if (m_cropDrag != CropDrag::None)
            updateCropDrag(mapToScene(event->position().toPoint()));
        else if (!m_spaceHeld)
            updateCropCursor(event->position().toPoint());
        return;
    }

    if (m_drawing) {
        // Clamped to the image, so frames never reach outside it.
        const QRectF bounds = m_scene->sceneRect();
        QPointF end = mapToScene(event->position().toPoint());
        end = QPointF(std::clamp(end.x(), bounds.left(), bounds.right()),
                      std::clamp(end.y(), bounds.top(), bounds.bottom()));
        if (m_drawing->type() == ArrowItem::Type) {
            // Shift: horizontal, vertical or diagonal (steps of 45°).
            if (event->modifiers() & Qt::ShiftModifier) {
                QLineF line(m_drawStart, end);
                line.setAngle(std::round(line.angle() / 45) * 45);
                end = line.p2();
                end = QPointF(std::clamp(end.x(), bounds.left(), bounds.right()),
                              std::clamp(end.y(), bounds.top(), bounds.bottom()));
            }
            static_cast<ArrowItem *>(m_drawing)->setLine(QLineF(m_drawStart, end));
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier) {
            // Square ("Shift für Quadrat").
            const bool left = end.x() < m_drawStart.x();
            const bool up = end.y() < m_drawStart.y();
            // As large as the drag, but only as far as the image reaches.
            const qreal side = std::min({std::max(std::abs(end.x() - m_drawStart.x()), std::abs(end.y() - m_drawStart.y())),
                                         left ? m_drawStart.x() - bounds.left() : bounds.right() - m_drawStart.x(),
                                         up ? m_drawStart.y() - bounds.top() : bounds.bottom() - m_drawStart.y()});
            end = m_drawStart + QPointF(left ? -side : side, up ? -side : side);
        }
        m_drawing->setRect(QRectF(m_drawStart, end));
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_pickingColor)
        return;
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        restoreCursor();
        return;
    }

    if (m_tool == ToolId::Crop && hasImage()) {
        if (event->button() == Qt::LeftButton && m_cropDrag != CropDrag::None) {
            updateCropDrag(mapToScene(event->position().toPoint()));
            m_cropDrag = CropDrag::None;
        }
        return;
    }

    if (m_drawing && event->button() == Qt::LeftButton) {
        RectItem *item = m_drawing;
        m_drawing = nullptr;
        // A replaced text takes colours, size and position from the original.
        const ReplaceItem::Match match = item->type() == ReplaceItem::Type
                                             ? static_cast<ReplaceItem *>(item)->matchOriginal()
                                             : ReplaceItem::Match::Ok;
        m_scene->removeItem(item);
        const QSizeF onScreen = transform().mapRect(item->rect()).size();
        // An arrow is a line: only its length counts.
        const bool tooSmall = item->type() == ArrowItem::Type
                                  ? std::hypot(onScreen.width(), onScreen.height()) < 2 * kMinDrawSize
                                  : onScreen.width() < kMinDrawSize || onScreen.height() < kMinDrawSize;
        if (tooSmall) {
            delete item; // just a click
            return;
        }
        QString text = tr("Rahmen zeichnen");
        if (item->type() == RedactItem::Type)
            text = tr("Verpixeln");
        else if (item->type() == ArrowItem::Type)
            text = tr("Pfeil zeichnen");
        else if (item->type() == MarkerItem::Type)
            text = tr("Markieren");
        else if (item->type() == ReplaceItem::Type)
            text = tr("Text ersetzen");
        else if (static_cast<FrameItem *>(item)->isNumbered())
            text = tr("Nummer setzen");
        m_scene->undoStack()->push(new AddItemsCommand(m_scene, {item}, text));
        emit annotationCreated(item);
        if (match == ReplaceItem::Match::CutsText)
            emit replaceCutsText();
        else if (match == ReplaceItem::Match::NoPlainBackground)
            emit replaceNoPlainBackground();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);

    // Turn a finished drag into one undo step.
    QList<MoveItemsCommand::Move> moves;
    for (auto it = m_positionsAtPress.cbegin(); it != m_positionsAtPress.cend(); ++it) {
        if (it.key()->scene() == m_scene && it.key()->pos() != it.value())
            moves.append({it.key(), it.value(), it.key()->pos()});
    }
    m_positionsAtPress.clear();
    if (!moves.isEmpty())
        m_scene->undoStack()->push(new MoveItemsCommand(moves));
}

void EditorView::mouseDoubleClickEvent(QMouseEvent *event)
{
    // A double click on a free text edits it, with every tool (F-25).
    if (m_tool != ToolId::Crop && event->button() == Qt::LeftButton && !m_editingText) {
        QGraphicsItem *hit = annotationAt(event->position().toPoint());
        if (hit && hit->type() == TextItem::Type) {
            startTextEditing(static_cast<TextItem *>(hit), false);
            return;
        }
    }
    // A double click inside the crop confirms it, like Enter.
    if (m_tool == ToolId::Crop && hasImage()) {
        if (event->button() == Qt::LeftButton
            && QRectF(m_cropDraft).contains(mapToScene(event->position().toPoint()))) {
            applyCrop();
            emit cropConfirmed();
        }
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void EditorView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);
    if (!hasImage())
        return;

    // Checkerboard under the image, so transparent corners are visible.
    const QRectF area = sceneRect().intersected(rect);
    const qreal cell = 8.0 / transform().m11(); // 8 screen pixels per cell
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->fillRect(area, QColor(0xcc, 0xcc, 0xcc));
    painter->setBrush(QColor(0xf2, 0xf2, 0xf2));
    for (qreal y = std::floor(area.top() / cell) * cell; y < area.bottom(); y += cell) {
        for (qreal x = std::floor(area.left() / cell) * cell; x < area.right(); x += cell) {
            if ((qint64(x / cell) + qint64(y / cell)) % 2 == 0)
                painter->drawRect(QRectF(x, y, cell, cell).intersected(area));
        }
    }
    painter->restore();
}

void EditorView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (hasImage() && m_tool == ToolId::Crop) {
        drawCropOverlay(painter);
    } else if (hasImage()) {
        // The base image and annotations reach beyond a crop; cover them
        // with the canvas, so the view shows what the export contains.
        if (m_scene->isCropped()) {
            QPainterPath outside;
            outside.addRect(mapToScene(viewport()->rect()).boundingRect().united(sceneRect()));
            QPainterPath crop;
            crop.addRect(sceneRect());
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, false);
            painter->fillPath(outside.subtracted(crop), palette().color(QPalette::Dark));
            painter->restore();
        }
    } else {
        // Empty state hint, drawn in viewport coordinates.
        painter->save();
        painter->resetTransform();
        painter->setPen(palette().color(QPalette::PlaceholderText));
        painter->drawText(viewport()->rect(), Qt::AlignCenter,
                          tr("Fenster aufnehmen oder ein Bild hierher ziehen"));
        painter->restore();
    }
    if (m_pickingColor)
        drawColorPickLoupe(painter); // on top of everything
}

void EditorView::wheelEvent(QWheelEvent *event)
{
    if (!(event->modifiers() & Qt::ControlModifier) || !hasImage()) {
        QGraphicsView::wheelEvent(event); // scrolls; Shift scrolls sideways
        return;
    }
    // Proportional to the wheel/touchpad movement: one wheel notch (120)
    // zooms by 20 %, small touchpad deltas by a little.
    m_fitMode = false;
    const QPoint anchor = event->position().toPoint();
    applyZoom(zoom() * std::pow(1.2, event->angleDelta().y() / 120.0), &anchor);
}

void EditorView::dragEnterEvent(QDragEnterEvent *event)
{
    QString path;
    if (firstLocalFile(event->mimeData(), &path))
        event->acceptProposedAction();
}

void EditorView::dragMoveEvent(QDragMoveEvent *event)
{
    // QGraphicsView would forward this to the scene, which rejects it.
    event->acceptProposedAction();
}

void EditorView::dropEvent(QDropEvent *event)
{
    QString path;
    if (firstLocalFile(event->mimeData(), &path)) {
        event->acceptProposedAction();
        emit fileDropped(path);
    }
}
