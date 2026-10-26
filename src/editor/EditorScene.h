#pragma once

#include "editor/ArrowStyle.h"
#include "editor/DimSettings.h"
#include "editor/FrameStyle.h"
#include "editor/MarkerStyle.h"
#include "editor/RedactStyle.h"
#include "editor/ReplaceStyle.h"
#include "editor/TextStyle.h"

#include <QGraphicsScene>
#include <QHash>
#include <QImage>
#include <QPainterPath>
#include <QPixmap>

class DimItem;
class FrameItem;
class RectItem;
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
    // The whole captured image, also when cropped: annotations keep their
    // image coordinates, so undoing a crop restores everything (F-12).
    QImage baseImage() const { return m_baseImage; }
    double imageScale() const { return m_scale; }

    // Crop (F-12): the part of the image that is shown and exported, in
    // image pixels. It is also the scene rect, so annotations stay inside
    // it. Annotations completely outside are hidden, not deleted. Change it
    // through SetCropCommand for undo.
    QRect cropRect() const { return m_crop; }
    void setCropRect(const QRect &rect);
    bool isCropped() const { return m_crop != m_baseImage.rect(); }

    // The result as exported (F-40, F-41): base image plus annotations,
    // without selection handles.
    QImage renderImage();
    bool isExporting() const { return m_exporting; }

    // Smooth or crisp (nearest-pixel) drawing of the image; the view
    // switches to crisp at high zoom (D-017).
    void setSmoothBase(bool smooth);
    bool smoothBase() const { return m_smoothBase; }

    // The image scale is stored in the PNG's resolution (96 dpi × scale), so
    // a saved HiDPI capture opens with the same scale again.
    static void setScaleMetadata(QImage &image, double scale);
    static double scaleFromMetadata(const QImage &image);

    // All annotations (frames, pixelated areas) in creation order
    // (RectItem::order): the order of the object list. Without the ones
    // hidden by the crop, so the visible numbers stay 1, 2, 3, …
    QList<RectItem *> annotations() const;
    QList<RectItem *> selectedAnnotations() const;
    // Only the frames, in the same order.
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
    // The dimmed area: the image minus the frame cut-outs.
    QPainterPath dimmedArea() const;
    // The dark veil for the current strength.
    QColor dimVeil() const;

    // Text for new replaced texts: the last one typed (F-29).
    QString replaceText() const { return m_replaceText; }
    void setReplaceText(const QString &text) { m_replaceText = text; }

    // Look of new arrows (F-44) and text markers (F-28); follow the panel.
    ArrowStyle arrowStyle() const { return m_arrowStyle; }
    void setArrowStyle(const ArrowStyle &style) { m_arrowStyle = style; }
    MarkerStyle markerStyle() const { return m_markerStyle; }
    void setMarkerStyle(const MarkerStyle &style) { m_markerStyle = style; }

    // Look of new free texts (F-25); follows the properties panel.
    TextStyle textStyle() const { return m_textStyle; }
    void setTextStyle(const TextStyle &style) { m_textStyle = style; }

    // Style for new pixelated areas; follows the properties panel.
    RedactStyle redactStyle() const { return m_redactStyle; }
    void setRedactStyle(const RedactStyle &style) { m_redactStyle = style; }

    // Style for new frames; follows the properties panel. Its badge is the
    // position used by the number tool.
    FrameStyle frameStyle() const { return m_frameStyle; }
    void setFrameStyle(const FrameStyle &style) { m_frameStyle = style; }

signals:
    // Annotations were added or removed (not emitted for moves or restyles).
    void annotationsChanged();
    void dimSettingsChanged();
    void cropChanged();

private:
    QUndoStack *m_undoStack;
    QGraphicsPixmapItem *m_base = nullptr;
    QImage m_baseImage;
    QRect m_crop;
    double m_scale = 1.0;
    bool m_exporting = false;
    bool m_smoothBase = true;
    FrameStyle m_frameStyle;
    RedactStyle m_redactStyle;
    TextStyle m_textStyle;
    ArrowStyle m_arrowStyle;
    MarkerStyle m_markerStyle;
    QString m_replaceText = QStringLiteral("Firstname Lastname");
    DimSettings m_dim;
    DimItem *m_dimItem = nullptr;
    // Caches for dimLayer(): the blur is the slow part, so it is kept per
    // radius; the layer itself is cheap to rebuild from it.
    mutable QHash<int, QImage> m_blurred;
    mutable QPixmap m_dimLayer;
    mutable DimSettings m_dimLayerSettings;
};
