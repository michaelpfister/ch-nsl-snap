#include "app/CountdownPositionPicker.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <iterator>

namespace {

// Sizes in logical pixels: the monitor (16:10), its stand, the dots.
constexpr int kScreenWidth = 168;
constexpr int kScreenHeight = 105;
constexpr int kBezel = 6;
constexpr int kStandHeight = 16;
constexpr qreal kDotRadius = 8;
// The dots sit this far inside the screen's edges.
constexpr qreal kDotInset = 18;

const CountdownPosition kPositions[] = {
    CountdownPosition::TopLeft,    CountdownPosition::TopRight,    CountdownPosition::Center,
    CountdownPosition::BottomLeft, CountdownPosition::BottomRight,
};

} // namespace

CountdownPositionPicker::CountdownPositionPicker(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // for the hover ring
    setAccessibleName(tr("Ort des Countdowns"));
    setToolTip(tr("Punkt anklicken: Hier erscheint der Countdown"));
}

QString CountdownPositionPicker::positionName(CountdownPosition position)
{
    switch (position) {
    case CountdownPosition::TopLeft: return tr("Oben links");
    case CountdownPosition::TopRight: return tr("Oben rechts");
    case CountdownPosition::BottomLeft: return tr("Unten links");
    case CountdownPosition::BottomRight: return tr("Unten rechts");
    case CountdownPosition::Center: break;
    }
    return tr("Mitte");
}

void CountdownPositionPicker::setPosition(CountdownPosition position)
{
    if (m_position == position)
        return;
    m_position = position;
    setAccessibleDescription(positionName(position));
    update();
}

QSize CountdownPositionPicker::sizeHint() const
{
    return QSize(kScreenWidth + 2 * kBezel, kScreenHeight + 2 * kBezel + kStandHeight);
}

QRectF CountdownPositionPicker::screenRect() const
{
    // Centred horizontally, at the top; the stand goes below.
    const qreal x = (width() - kScreenWidth) / 2.0;
    return QRectF(x, kBezel, kScreenWidth, kScreenHeight);
}

QPointF CountdownPositionPicker::dotCenter(CountdownPosition position) const
{
    const QRectF screen = screenRect().adjusted(kDotInset, kDotInset, -kDotInset, -kDotInset);
    switch (position) {
    case CountdownPosition::TopLeft: return screen.topLeft();
    case CountdownPosition::TopRight: return screen.topRight();
    case CountdownPosition::BottomLeft: return screen.bottomLeft();
    case CountdownPosition::BottomRight: return screen.bottomRight();
    case CountdownPosition::Center: break;
    }
    return screen.center();
}

bool CountdownPositionPicker::dotAt(const QPointF &point, CountdownPosition *position) const
{
    // A generous target: twice the dot's radius.
    for (CountdownPosition candidate : kPositions) {
        const QPointF delta = point - dotCenter(candidate);
        if (QPointF::dotProduct(delta, delta) <= 4 * kDotRadius * kDotRadius) {
            *position = candidate;
            return true;
        }
    }
    return false;
}

void CountdownPositionPicker::choose(CountdownPosition position)
{
    if (position == m_position)
        return;
    setPosition(position);
    emit positionChanged(position);
}

void CountdownPositionPicker::paintEvent(QPaintEvent *)
{
    // Colours from the palette, which Theme::apply fills with the tokens:
    // Highlight is the accent, Mid the border, Base the panel.
    const QPalette pal = palette();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // The monitor: bezel, screen, stand.
    const QRectF screen = screenRect();
    const QRectF bezel = screen.adjusted(-kBezel, -kBezel, kBezel, kBezel);
    painter.setPen(Qt::NoPen);
    painter.setBrush(pal.color(QPalette::Mid));
    painter.drawRoundedRect(bezel, 8, 8);
    const qreal standX = bezel.center().x();
    QPainterPath stand;
    stand.moveTo(standX - 10, bezel.bottom());
    stand.lineTo(standX + 10, bezel.bottom());
    stand.lineTo(standX + 14, bezel.bottom() + kStandHeight - 3);
    stand.lineTo(standX - 14, bezel.bottom() + kStandHeight - 3);
    stand.closeSubpath();
    painter.drawPath(stand);
    painter.drawRoundedRect(QRectF(standX - 28, bezel.bottom() + kStandHeight - 4, 56, 4), 2, 2);
    painter.setBrush(pal.color(QPalette::AlternateBase));
    painter.drawRoundedRect(screen, 4, 4);

    // The dots: the chosen one filled, the others as rings.
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor muted = pal.color(QPalette::PlaceholderText);
    for (int i = 0; i < int(std::size(kPositions)); ++i) {
        const CountdownPosition position = kPositions[i];
        const QPointF center = dotCenter(position);
        if (position == m_position) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(accent);
            painter.drawEllipse(center, kDotRadius, kDotRadius);
        } else {
            painter.setPen(QPen(i == m_hovered ? accent : muted, 1.5));
            painter.setBrush(pal.color(QPalette::Base));
            painter.drawEllipse(center, kDotRadius - 1, kDotRadius - 1);
        }
    }

    // The focus ring around the chosen dot, for the keyboard.
    if (hasFocus()) {
        painter.setPen(QPen(accent, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(dotCenter(m_position), kDotRadius + 4, kDotRadius + 4);
    }
}

void CountdownPositionPicker::mousePressEvent(QMouseEvent *event)
{
    CountdownPosition position;
    if (event->button() == Qt::LeftButton && dotAt(event->position(), &position))
        choose(position);
    else
        QWidget::mousePressEvent(event);
}

void CountdownPositionPicker::mouseMoveEvent(QMouseEvent *event)
{
    CountdownPosition position;
    int hovered = -1;
    if (dotAt(event->position(), &position)) {
        for (int i = 0; i < int(std::size(kPositions)); ++i) {
            if (kPositions[i] == position)
                hovered = i;
        }
    }
    setCursor(hovered >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        update();
    }
}

void CountdownPositionPicker::leaveEvent(QEvent *event)
{
    m_hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

void CountdownPositionPicker::keyPressEvent(QKeyEvent *event)
{
    // The arrow keys go through the dots in reading order: top left, top
    // right, middle, bottom left, bottom right.
    int step = 0;
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Up)
        step = -1;
    else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down)
        step = 1;
    if (step == 0) {
        QWidget::keyPressEvent(event);
        return;
    }
    const int count = int(std::size(kPositions));
    int index = 0;
    for (int i = 0; i < count; ++i) {
        if (kPositions[i] == m_position)
            index = i;
    }
    choose(kPositions[std::clamp(index + step, 0, count - 1)]);
}
