#include "ui/ColorWheel.h"

#include <QConicalGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr qreal kRingWidth = 16; // widget pixels

// Barycentric coordinates of `p` in the triangle (a, b, c).
void barycentric(const QPointF &p, const QPointF &a, const QPointF &b, const QPointF &c,
                 qreal *u, qreal *v, qreal *w)
{
    const QPointF v0 = b - a, v1 = c - a, v2 = p - a;
    const qreal d00 = QPointF::dotProduct(v0, v0);
    const qreal d01 = QPointF::dotProduct(v0, v1);
    const qreal d11 = QPointF::dotProduct(v1, v1);
    const qreal d20 = QPointF::dotProduct(v2, v0);
    const qreal d21 = QPointF::dotProduct(v2, v1);
    const qreal denom = d00 * d11 - d01 * d01;
    *v = (d11 * d20 - d01 * d21) / denom;
    *w = (d00 * d21 - d01 * d20) / denom;
    *u = 1.0 - *v - *w;
}

// A point on the ring for a hue. Hue 0 (red) is on the right, and hues go
// counter-clockwise, as in most colour wheels.
QPointF onCircle(const QPointF &center, qreal radius, qreal hue, qreal offsetDegrees = 0)
{
    const qreal angle = (hue * 360.0 + offsetDegrees) * std::numbers::pi / 180.0;
    return center + QPointF(std::cos(angle) * radius, -std::sin(angle) * radius);
}

// Draws a marker: a white ring with a dark edge, visible on every colour.
void drawMarker(QPainter &p, const QPointF &pos, qreal radius)
{
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 140), 3.5));
    p.drawEllipse(pos, radius, radius);
    p.setPen(QPen(Qt::white, 2));
    p.drawEllipse(pos, radius, radius);
}

} // namespace

ColorWheel::ColorWheel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 160);
    setCursor(Qt::CrossCursor);
    setAccessibleName(tr("Farbrad"));
}

QColor ColorWheel::color() const
{
    return QColor::fromHsvF(float(m_hue), float(m_saturation), float(m_value));
}

void ColorWheel::setColor(const QColor &color)
{
    if (!color.isValid() || color == this->color())
        return;
    float h, s, v;
    color.getHsvF(&h, &s, &v);
    if (h >= 0) // -1 for greys: keep the hue the user had
        m_hue = h;
    m_saturation = s;
    m_value = v;
    update();
}

QPointF ColorWheel::center() const
{
    return QRectF(rect()).center();
}

qreal ColorWheel::outerRadius() const
{
    return std::min(width(), height()) / 2.0 - 1;
}

qreal ColorWheel::innerRadius() const
{
    return outerRadius() - kRingWidth;
}

void ColorWheel::triangle(QPointF *hue, QPointF *white, QPointF *black) const
{
    const qreal r = innerRadius() - 4;
    *hue = onCircle(center(), r, m_hue);
    *white = onCircle(center(), r, m_hue, 120);
    *black = onCircle(center(), r, m_hue, 240);
}

QPointF ColorWheel::triangleMarker() const
{
    // The colour is hue · s·v + white · (1 − s)·v + black · (1 − v).
    QPointF hue, white, black;
    triangle(&hue, &white, &black);
    return hue * (m_saturation * m_value) + white * ((1 - m_saturation) * m_value) + black * (1 - m_value);
}

void ColorWheel::updateTriangleImage()
{
    if (m_triangleImageHue == m_hue && m_triangleImage.size() == size() * devicePixelRatio())
        return;
    const qreal dpr = devicePixelRatio();
    m_triangleImage = QImage(size() * dpr, QImage::Format_ARGB32_Premultiplied);
    m_triangleImage.setDevicePixelRatio(dpr);
    m_triangleImage.fill(Qt::transparent);
    m_triangleImageHue = m_hue;

    QPointF a, b, c;
    triangle(&a, &b, &c);
    const QColor pure = QColor::fromHsvF(float(m_hue), 1, 1);
    const QRect box = QPolygonF({a, b, c}).boundingRect().toAlignedRect();
    // Every pixel mixes the three corners (hue, white, black) by its
    // barycentric coordinates; edge pixels are cut by the clip below.
    for (int y = int(box.top() * dpr); y <= int(box.bottom() * dpr); ++y) {
        if (y < 0 || y >= m_triangleImage.height())
            continue;
        auto *line = reinterpret_cast<QRgb *>(m_triangleImage.scanLine(y));
        for (int x = int(box.left() * dpr); x <= int(box.right() * dpr); ++x) {
            if (x < 0 || x >= m_triangleImage.width())
                continue;
            qreal u, v, w;
            barycentric(QPointF((x + 0.5) / dpr, (y + 0.5) / dpr), a, b, c, &u, &v, &w);
            u = std::clamp(u, 0.0, 1.0);
            v = std::clamp(v, 0.0, 1.0);
            const auto mix = [u, v](float hueChannel) {
                return std::clamp(int(std::lround((u * hueChannel + v) * 255)), 0, 255);
            };
            line[x] = qRgb(mix(pure.redF()), mix(pure.greenF()), mix(pure.blueF()));
        }
    }
}

void ColorWheel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Ring: a conical gradient through all hues, cut to a ring.
    QConicalGradient gradient(center(), 0);
    for (int i = 0; i <= 12; ++i)
        gradient.setColorAt(i / 12.0, QColor::fromHsvF(float(i % 12) / 12.0f, 1, 1));
    QPainterPath ring;
    ring.addEllipse(center(), outerRadius(), outerRadius());
    ring.addEllipse(center(), innerRadius(), innerRadius());
    p.fillPath(ring, gradient);

    // Triangle.
    updateTriangleImage();
    QPointF a, b, c;
    triangle(&a, &b, &c);
    QPainterPath tri;
    tri.addPolygon(QPolygonF({a, b, c}));
    tri.closeSubpath();
    p.save();
    p.setClipPath(tri);
    p.drawImage(QPointF(0, 0), m_triangleImage);
    p.restore();

    drawMarker(p, onCircle(center(), (outerRadius() + innerRadius()) / 2, m_hue), kRingWidth / 2 - 1);
    drawMarker(p, triangleMarker(), 6);
}

void ColorWheel::setHueFrom(const QPointF &pos)
{
    const QPointF d = pos - center();
    qreal angle = std::atan2(-d.y(), d.x()) / (2 * std::numbers::pi);
    if (angle < 0)
        angle += 1;
    m_hue = angle;
    update();
    emit colorChanged(color());
}

void ColorWheel::setSaturationValueFrom(const QPointF &pos)
{
    QPointF a, b, c;
    triangle(&a, &b, &c);
    qreal u, v, w;
    barycentric(pos, a, b, c, &u, &v, &w);
    // Outside the triangle: the nearest point on its edge (approximately,
    // by clamping and renormalising the coordinates).
    u = std::max(0.0, u);
    v = std::max(0.0, v);
    w = std::max(0.0, w);
    const qreal sum = u + v + w;
    u /= sum;
    v /= sum;
    m_value = std::clamp(u + v, 0.0, 1.0);
    m_saturation = m_value > 0 ? std::clamp(u / m_value, 0.0, 1.0) : 0.0;
    update();
    emit colorChanged(color());
}

void ColorWheel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const QPointF pos = event->position();
    const qreal distance = std::hypot(pos.x() - center().x(), pos.y() - center().y());
    if (distance >= innerRadius() && distance <= outerRadius() + 2) {
        m_drag = Drag::Ring;
        setHueFrom(pos);
    } else if (distance < innerRadius()) {
        m_drag = Drag::Triangle;
        setSaturationValueFrom(pos);
    }
}

void ColorWheel::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag == Drag::Ring)
        setHueFrom(event->position());
    else if (m_drag == Drag::Triangle)
        setSaturationValueFrom(event->position());
}

void ColorWheel::mouseReleaseEvent(QMouseEvent *)
{
    m_drag = Drag::None;
}

void ColorWheel::resizeEvent(QResizeEvent *)
{
    m_triangleImageHue = -1; // rebuild for the new size
}
