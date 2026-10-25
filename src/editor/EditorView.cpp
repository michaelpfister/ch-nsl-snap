#include "editor/EditorView.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMimeData>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

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

EditorView::EditorView(QWidget *parent)
    : QGraphicsView(parent)
{
    setScene(new QGraphicsScene(this));
    setAcceptDrops(true);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setBackgroundRole(QPalette::Dark);
}

void EditorView::setImage(const QImage &image, double scale)
{
    clear();
    m_image = image;
    m_pixmapItem = scene()->addPixmap(QPixmap::fromImage(image));
    scene()->setSceneRect(m_pixmapItem->boundingRect());
    zoomToFit(scale);
}

void EditorView::clear()
{
    scene()->clear(); // also deletes m_pixmapItem
    m_pixmapItem = nullptr;
    m_image = QImage();
    resetTransform();
    viewport()->update();
}

void EditorView::zoomToFit(double scale)
{
    // Logical size (100 %), but never larger than the visible area.
    const QSizeF logical = QSizeF(m_image.size()) / scale;
    const QSizeF available = viewport()->size() - QSize(20, 20);
    const double fit = std::min({1.0, available.width() / logical.width(),
                                 available.height() / logical.height()});
    // One scene unit is one image pixel; at 100 % it covers 1/scale logical
    // pixels, i.e. one physical pixel on the screen it was taken from.
    resetTransform();
    const double factor = fit / scale;
    QGraphicsView::scale(factor, factor);
    centerOn(m_pixmapItem);
}

void EditorView::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);
    if (m_image.isNull())
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
    if (!m_image.isNull())
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
    if (!(event->modifiers() & Qt::ControlModifier) || m_image.isNull()) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
    QGraphicsView::scale(factor, factor);
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
