#include "editor/EditorView.h"

#include "editor/Commands.h"
#include "editor/EditorScene.h"
#include "editor/FrameItem.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QScrollBar>
#include <QUndoStack>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

// A drawn frame smaller than this (screen pixels) counts as a click.
constexpr int kMinDrawSize = 6;

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
}

void EditorView::setImage(const QImage &image, double scale)
{
    m_scene->setBaseImage(image, scale);
    fitToWindow();
}

void EditorView::clear()
{
    m_scene->clearAll();
    resetTransform();
    viewport()->update();
    emit zoomChanged();
}

bool EditorView::hasImage() const
{
    return m_scene->hasImage();
}

QImage EditorView::renderedImage() const
{
    return m_scene->renderImage();
}

double EditorView::zoom() const
{
    return transform().m11() * m_scene->imageScale();
}

void EditorView::setTool(ToolId tool)
{
    m_tool = tool;
    setDragMode(isDrawingTool() ? QGraphicsView::NoDrag : QGraphicsView::RubberBandDrag);
    viewport()->setCursor(isDrawingTool() ? Qt::CrossCursor : Qt::ArrowCursor);
}

bool EditorView::isDrawingTool() const
{
    // Frame and numbered frame draw so far; the others follow (docs/04-features.md).
    return m_tool == ToolId::Frame || m_tool == ToolId::Number;
}

bool EditorView::deleteSelection()
{
    QList<QGraphicsItem *> items;
    for (FrameItem *frame : m_scene->selectedFrames())
        items << frame;
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
    const QSizeF logical = QSizeF(m_scene->baseImage().size()) / m_scene->imageScale();
    const QSizeF available = QSizeF(viewport()->size()) - QSizeF(2 * kFitMargin, 2 * kFitMargin);
    const double fit = std::min(available.width() / logical.width(), available.height() / logical.height());
    applyZoom(std::clamp(fit, kMinZoom, kMaxZoom));
    centerOn(m_scene->sceneRect().center());
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
    for (FrameItem *frame : m_scene->frames())
        frame->viewZoomChanged();
    emit zoomChanged();
}

QGraphicsItem *EditorView::annotationAt(const QPoint &viewPos) const
{
    for (QGraphicsItem *item : items(viewPos)) {
        if (item->type() == FrameItem::Type)
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
    if (m_spaceHeld)
        viewport()->setCursor(Qt::OpenHandCursor);
    else
        viewport()->setCursor(isDrawingTool() ? Qt::CrossCursor : Qt::ArrowCursor);
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
    // Space + drag pans, for touchpads without a middle button.
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceHeld = true;
        restoreCursor();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

void EditorView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
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

    // Frame tool on empty space: start a new frame. On an existing frame the
    // tool behaves like Select, so the frame just drawn can be adjusted.
    // Frames start only on the image; outside it the press selects as usual.
    if (event->button() == Qt::LeftButton && isDrawingTool() && hasImage()
        && m_scene->sceneRect().contains(mapToScene(event->position().toPoint()))
        && !annotationAt(event->position().toPoint())) {
        m_scene->clearSelection();
        m_drawStart = mapToScene(event->position().toPoint());
        // The number tool uses the badge position of the current style; the
        // frame tool draws plain frames.
        FrameStyle style = m_scene->frameStyle();
        if (m_tool == ToolId::Frame)
            style.badge = BadgePosition::None;
        m_drawing = new FrameItem(QRectF(m_drawStart, m_drawStart), style, m_scene->imageScale());
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

    if (m_drawing) {
        // Clamped to the image, so frames never reach outside it.
        const QRectF bounds = m_scene->sceneRect();
        QPointF end = mapToScene(event->position().toPoint());
        end = QPointF(std::clamp(end.x(), bounds.left(), bounds.right()),
                      std::clamp(end.y(), bounds.top(), bounds.bottom()));
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
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        m_panning = false;
        restoreCursor();
        return;
    }

    if (m_drawing && event->button() == Qt::LeftButton) {
        FrameItem *frame = m_drawing;
        m_drawing = nullptr;
        m_scene->removeItem(frame);
        const QSizeF onScreen = transform().mapRect(frame->rect()).size();
        if (onScreen.width() < kMinDrawSize || onScreen.height() < kMinDrawSize) {
            delete frame; // just a click
            return;
        }
        m_scene->undoStack()->push(new AddItemsCommand(m_scene, {frame},
                                                       frame->isNumbered() ? tr("Nummer setzen") : tr("Rahmen zeichnen")));
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
    if (hasImage())
        return;

    // Empty state hint, drawn in viewport coordinates.
    painter->save();
    painter->resetTransform();
    painter->setPen(palette().color(QPalette::PlaceholderText));
    painter->drawText(viewport()->rect(), Qt::AlignCenter,
                      tr("Fenster aufnehmen oder ein Bild hierher ziehen"));
    painter->restore();
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
