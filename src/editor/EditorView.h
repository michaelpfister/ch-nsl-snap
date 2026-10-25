#pragma once

#include "editor/Tool.h"

#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRect>

class EditorScene;
class RectItem;
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

    // Crop tool (F-12): while it is active the whole image is shown and the
    // user adjusts a draft rectangle; applyCrop() turns it into an undo
    // step, cancelCrop() forgets it. Leaving the tool applies it.
    QRect cropDraft() const { return m_cropDraft; }
    void setCropDraft(const QRect &rect);
    void applyCrop();
    void cancelCrop();

    // Deletes the selected annotations (Delete key, F-46). False if none.
    bool deleteSelection();

signals:
    // A local file was dropped onto the view (F-06).
    void fileDropped(const QString &path);
    void zoomChanged();
    // The user drew a new annotation (already on the undo stack).
    void annotationCreated(RectItem *item);
    // Problems when recognising the original under a new replaced text (F-29).
    void replaceCutsText();
    void replaceNoPlainBackground();
    // The crop draft changed (dragged, reset); `rect` in image pixels.
    void cropDraftChanged(const QRect &rect);
    // Enter or a double click confirmed the crop.
    void cropConfirmed();

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
    void mouseDoubleClickEvent(QMouseEvent *event) override;
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

    // Crop tool (F-12).
    enum class CropDrag { None, New, Move, Handle };
    void beginCropEditing();
    void endCropEditing();
    // Index of the draft's handle under `viewPos` (0 = top left, clockwise), or -1.
    int cropHandleAt(const QPoint &viewPos) const;
    void updateCropDrag(const QPointF &scenePos);
    void updateCropCursor(const QPoint &viewPos);
    void drawCropOverlay(QPainter *painter);

    EditorScene *m_scene;
    ToolId m_tool = ToolId::Select;

    // A frame or pixelated area being drawn, not yet on the undo stack.
    RectItem *m_drawing = nullptr;
    QPointF m_drawStart;

    // Positions of the selected items when a drag started, for the undo step.
    QHash<QGraphicsItem *, QPointF> m_positionsAtPress;

    // Panning with the middle button, or Space + left button (touchpads).
    bool m_panning = false;
    bool m_spaceHeld = false;
    QPoint m_panLast;

    bool m_fitMode = true;

    QRect m_cropDraft;
    CropDrag m_cropDrag = CropDrag::None;
    int m_cropHandle = -1;
    QPointF m_cropPress;
    QRect m_cropAtPress;
};
