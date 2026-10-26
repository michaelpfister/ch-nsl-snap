#pragma once

#include "editor/Tool.h"

#include <QGraphicsView>
#include <QHash>
#include <QImage>
#include <QPointF>
#include <QRect>

class EditorScene;
class RectItem;
class TextItem;
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
    // Ends a text edit first, so the export never shows a text cursor.
    QImage renderedImage();

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

    // Free text (F-25): typing happens on the canvas. Ending the edit (click
    // outside, Escape, another tool) turns it into one undo step; an empty
    // text is removed.
    bool isEditingText() const { return m_editingText != nullptr; }
    void finishTextEditing();

    // Eyedropper of the colour popup: the next click on the image picks the
    // colour of that pixel (colorPicked); Escape or the right button cancel.
    // A magnifier near the cursor shows the pixels and the colour.
    void startColorPick();
    bool isPickingColor() const { return m_pickingColor; }

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
    // The eyedropper ended: the picked colour, or invalid if cancelled.
    void colorPicked(const QColor &color);
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
    // `isNew`: the item is in the scene but not yet on the undo stack.
    void startTextEditing(TextItem *item, bool isNew);
    // Forgets the edit without an undo step (the scene is being cleared).
    void dropTextEditing();

    // Crop tool (F-12).
    enum class CropDrag { None, New, Move, Handle };
    void beginCropEditing();
    void endCropEditing();
    // Index of the draft's handle under `viewPos` (0 = top left, clockwise), or -1.
    int cropHandleAt(const QPoint &viewPos) const;
    void updateCropDrag(const QPointF &scenePos);
    void updateCropCursor(const QPoint &viewPos);
    void drawCropOverlay(QPainter *painter);
    // Eyedropper: ends the pick with `color` (invalid = cancelled).
    void endColorPick(const QColor &color);
    void drawColorPickLoupe(QPainter *painter);
    // The base image's colour at a viewport position; invalid off the image.
    QColor colorAt(const QPoint &viewPos) const;

    EditorScene *m_scene;
    ToolId m_tool = ToolId::Select;

    // A frame or pixelated area being drawn, not yet on the undo stack.
    RectItem *m_drawing = nullptr;
    QPointF m_drawStart;

    // The free text being typed (F-25), with its state before the edit.
    TextItem *m_editingText = nullptr;
    bool m_editingIsNew = false;
    QString m_textBefore;
    QPointF m_textPosBefore;

    // Positions of the selected items when a drag started, for the undo step.
    QHash<QGraphicsItem *, QPointF> m_positionsAtPress;

    // Panning with the middle button, or Space + left button (touchpads).
    bool m_panning = false;
    bool m_spaceHeld = false;
    QPoint m_panLast;

    bool m_fitMode = true;

    bool m_pickingColor = false;
    QPoint m_pickPos; // viewport position of the cursor while picking

    QRect m_cropDraft;
    CropDrag m_cropDrag = CropDrag::None;
    int m_cropHandle = -1;
    QPointF m_cropPress;
    QRect m_cropAtPress;
};
