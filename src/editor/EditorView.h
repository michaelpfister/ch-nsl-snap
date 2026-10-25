#pragma once

#include <QGraphicsView>
#include <QImage>

class QGraphicsPixmapItem;

// Shows the image being edited. The scene works in image pixels, so later
// annotations (D-012) are positioned in the same coordinates as the export.
class EditorView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit EditorView(QWidget *parent = nullptr);

    // `scale` is image pixels per logical pixel; 100 % zoom shows the image
    // at its logical size, like on the screen it was taken from.
    void setImage(const QImage &image, double scale = 1.0);
    void clear();

    bool hasImage() const { return !m_image.isNull(); }
    QImage image() const { return m_image; }

signals:
    // A local file was dropped onto the view (F-06).
    void fileDropped(const QString &path);

protected:
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void zoomToFit(double scale);

    QImage m_image;
    QGraphicsPixmapItem *m_pixmapItem = nullptr;
};
