#pragma once

#include "editor/DimSettings.h"
#include "editor/FrameStyle.h"

#include <QGraphicsScene>
#include <QHash>
#include <QImage>
#include <QPixmap>

class DimItem;
class FrameItem;
class QGraphicsPixmapItem;
class QUndoStack;

// The document being edited (D-012): the captured image as an unchanged
// base layer plus annotation items on top. Coordinates are image pixels.
class EditorScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit EditorScene(QUndoStack *undoStack, QObject *parent = nullptr);

    QUndoStack *undoStack() const { return m_undoStack; }

    // Replaces everything. The caller clears the undo stack first, because
    // its commands refer to the old items.
    void setBaseImage(const QImage &image, double scale);
    void clearAll();

    bool hasImage() const { return !m_baseImage.isNull(); }
    QImage baseImage() const { return m_baseImage; }
    double imageScale() const { return m_scale; }

    // The result as exported (F-40, F-41): base image plus annotations,
    // without selection handles.
    QImage renderImage();
    bool isExporting() const { return m_exporting; }

    // The image scale is stored in the PNG's resolution (96 dpi × scale), so
    // a saved HiDPI capture opens with the same scale again.
    static void setScaleMetadata(QImage &image, double scale);
    static double scaleFromMetadata(const QImage &image);

    // Annotation frames in creation order (FrameItem::order).
    QList<FrameItem *> frames() const;
    QList<FrameItem *> selectedFrames() const;

    // Number of a numbered frame: its position among the numbered frames
    // in creation order, so deleting one renumbers the rest (F-24).
    int numberOf(const FrameItem *frame) const;

    // Dimmed background (F-27). Change it through SetDimCommand for undo.
    DimSettings dimSettings() const { return m_dim; }
    void setDimSettings(const DimSettings &settings);
    // The whole image as it looks dimmed: blurred with the current radius
    // and darkened. Cached until the image or the settings change, so
    // repainting and switching on and off cost nothing.
    QPixmap dimLayer() const;

    // Style for new frames; follows the properties panel. Its badge is the
    // position used by the number tool.
    FrameStyle frameStyle() const { return m_frameStyle; }
    void setFrameStyle(const FrameStyle &style) { m_frameStyle = style; }

signals:
    // Annotations were added or removed (not emitted for moves or restyles).
    void annotationsChanged();
    void dimSettingsChanged();

private:
    QUndoStack *m_undoStack;
    QGraphicsPixmapItem *m_base = nullptr;
    QImage m_baseImage;
    double m_scale = 1.0;
    bool m_exporting = false;
    FrameStyle m_frameStyle;
    DimSettings m_dim;
    DimItem *m_dimItem = nullptr;
    // Caches for dimLayer(): the blur is the slow part, so it is kept per
    // radius; the layer itself is cheap to rebuild from it.
    mutable QHash<int, QImage> m_blurred;
    mutable QPixmap m_dimLayer;
    mutable DimSettings m_dimLayerSettings;
};
