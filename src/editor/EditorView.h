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

    // Zoom (D-017). 1.0 = the image at its logical size (100 %).
    static constexpr double kMinZoom = 0.05;
    static constexpr double kMaxZoom = 8.0;
    double zoom() const;
    // Fit mode: the image fills the visible area and follows window resizes.
    // New images start in it; any manual zoom leaves it.
    bool isFitMode() const { return m_fitMode; }
    void fitToWindow();
    // Sets the zoom around the centre of the view and leaves fit mode.
    void setZoom(double zoom);
    void zoomIn();
    void zoomOut();

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
    void resizeEvent(QResizeEvent *event) override;
    bool viewportEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    // Sets the zoom; `anchor` (viewport position) stays in place, or the
    // view centre if null.
    void applyZoom(double zoom, const QPoint *anchor = nullptr);
    void updateFit();
    void startPan(const QPoint &pos);
    void restoreCursor();
    bool isDrawingTool() const;
    QGraphicsItem *annotationAt(const QPoint &viewPos) const;

    EditorScene *m_scene;
    ToolId m_tool = ToolId::Select;

    // A frame being drawn (Frame tool), not yet on the undo stack.
    FrameItem *m_drawing = nullptr;
    QPointF m_drawStart;

    // Positions of the selected items when a drag started, for the undo step.
    QHash<QGraphicsItem *, QPointF> m_positionsAtPress;

    // Panning with the middle button, or Space + left button (touchpads).
    bool m_panning = false;
    bool m_spaceHeld = false;
    QPoint m_panLast;

    bool m_fitMode = true;
};
