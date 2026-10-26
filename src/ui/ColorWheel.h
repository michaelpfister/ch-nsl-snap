#pragma once

#include <QColor>
#include <QImage>
#include <QWidget>

// A colour wheel with a triangle (HSV): the ring picks the hue, the
// triangle inside picks saturation and brightness. The triangle turns so
// that its coloured corner points to the hue on the ring; the other two
// corners are white and black.
class ColorWheel : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheel(QWidget *parent = nullptr);

    QColor color() const;
    // Shows `color` without emitting colorChanged.
    void setColor(const QColor &color);

    QSize sizeHint() const override { return QSize(200, 200); }

signals:
    // The user dragged on the ring or in the triangle.
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class Drag { None, Ring, Triangle };

    // Geometry, in widget pixels.
    QPointF center() const;
    qreal outerRadius() const;
    qreal innerRadius() const;
    // Corners of the triangle for the current hue: hue, white, black.
    void triangle(QPointF *hue, QPointF *white, QPointF *black) const;
    // The point in the triangle that shows the current saturation and value.
    QPointF triangleMarker() const;

    void setHueFrom(const QPointF &pos);
    void setSaturationValueFrom(const QPointF &pos);
    // The triangle as an image; rebuilt when the hue or the size changes.
    void updateTriangleImage();

    // Kept separately, so the hue survives grey, white and black.
    qreal m_hue = 0;        // 0–1
    qreal m_saturation = 1; // 0–1
    qreal m_value = 1;      // 0–1
    Drag m_drag = Drag::None;
    QImage m_triangleImage;
    qreal m_triangleImageHue = -1;
};
