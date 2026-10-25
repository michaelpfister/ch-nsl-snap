#include "editor/EditorView.h"

#include "editor/Commands.h"
#include "editor/EditorScene.h"
#include "editor/FrameItem.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
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
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setBackgroundRole(QPalette::Dark);
    // Handles keep their screen size; redraw everything when they move.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
}

void EditorView::setImage(const QImage &image, double scale)
{
    m_scene->setBaseImage(image, scale);
    zoomToFit(scale);
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
    // Only the frame tool draws so far; the others follow (docs/04-features.md).
    return m_tool == ToolId::Frame;
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

void EditorView::zoomToFit(double scale)
{
    // Logical size (100 %), but never larger than the visible area.
    const QSizeF logical = QSizeF(m_scene->baseImage().size()) / scale;
    const QSizeF available = viewport()->size() - QSize(20, 20);
    const double fit = std::min({1.0, available.width() / logical.width(),
                                 available.height() / logical.height()});
    // One scene unit is one image pixel; at 100 % it covers 1/scale logical
    // pixels, i.e. one physical pixel on the screen it was taken from.
    resetTransform();
    applyZoom(fit / scale);
    centerOn(m_scene->sceneRect().center());
}

void EditorView::applyZoom(double factor)
{
    QGraphicsView::scale(factor, factor);
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

void EditorView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panLast = event->position().toPoint();
        viewport()->setCursor(Qt::ClosedHandCursor);
        return;
    }

    // Frame tool on empty space: start a new frame. On an existing frame the
    // tool behaves like Select, so the frame just drawn can be adjusted.
    if (event->button() == Qt::LeftButton && isDrawingTool() && hasImage()
        && !annotationAt(event->position().toPoint())) {
        m_scene->clearSelection();
        m_drawStart = mapToScene(event->position().toPoint());
        m_drawing = new FrameItem(QRectF(m_drawStart, m_drawStart), m_scene->frameStyle(), m_scene->imageScale());
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
        QPointF end = mapToScene(event->position().toPoint());
        if (event->modifiers() & Qt::ShiftModifier) {
            // Square ("Shift für Quadrat").
            const qreal side = std::max(std::abs(end.x() - m_drawStart.x()), std::abs(end.y() - m_drawStart.y()));
            end = m_drawStart + QPointF(end.x() < m_drawStart.x() ? -side : side,
                                        end.y() < m_drawStart.y() ? -side : side);
        }
        m_drawing->setRect(QRectF(m_drawStart, end));
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void EditorView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_panning && event->button() == Qt::MiddleButton) {
        m_panning = false;
        viewport()->setCursor(isDrawingTool() ? Qt::CrossCursor : Qt::ArrowCursor);
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
        m_scene->undoStack()->push(new AddItemsCommand(m_scene, {frame}, tr("Rahmen zeichnen")));
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
        QGraphicsView::wheelEvent(event);
        return;
    }
    applyZoom(event->angleDelta().y() > 0 ? 1.25 : 0.8);
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
