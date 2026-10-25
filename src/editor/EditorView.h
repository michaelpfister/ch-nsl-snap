#pragma once

#include "editor/Tool.h"

#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QPointF>

class EditorScene;
class FrameItem;
class QGraphicsItem;
class QUndoStack;

// Shows the document and turns mouse input into editing commands. The scene
// works in image pixels, so annotations use the same coordinates as the
// export.
class EditorView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit EditorView(QUndoStack *undoStack, QWidget *parent = nullptr);

    EditorScene *editorScene() const { return m_scene; }

    // `scale` is image pixels per logical pixel; 100 % zoom shows the image
    // at its logical size, like on the screen it was taken from.
    void setImage(const QImage &image, double scale = 1.0);
    void clear();

    bool hasImage() const;
    // The result with annotations, as saved and copied.
    QImage renderedImage() const;

    // 1.0 = the image at its logical size (100 %).
    double zoom() const;

    void setTool(ToolId tool);
    // Deletes the selected annotations (Delete key, F-46). False if none.
    bool deleteSelection();

signals:
    // A local file was dropped onto the view (F-06).
    void fileDropped(const QString &path);
    void zoomChanged();

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void zoomToFit(double scale);
    void applyZoom(double factor);
    bool isDrawingTool() const;
    QGraphicsItem *annotationAt(const QPoint &viewPos) const;

    EditorScene *m_scene;
    ToolId m_tool = ToolId::Select;

    // A frame being drawn (Frame tool), not yet on the undo stack.
    FrameItem *m_drawing = nullptr;
    QPointF m_drawStart;

    // Positions of the selected items when a drag started, for the undo step.
    QHash<QGraphicsItem *, QPointF> m_positionsAtPress;

    // Middle-button panning.
    bool m_panning = false;
    QPoint m_panLast;
};
