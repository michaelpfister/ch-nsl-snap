#include "ui/LogoMark.h"

#include "ui/Motion.h"

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

#include <algorithm>

namespace {

constexpr QPointF kCentre(32, 32);
// The strokes lie between 10 and 54: the box shows this square.
constexpr qreal kViewOrigin = 10;
constexpr qreal kViewSize = 44;

qreal lerp(qreal a, qreal b, qreal t)
{
    return a + (b - a) * t;
}

// The three strokes of logo.svg (resources/icons), in logo units.
QPainterPath topLeftCorner()
{
    QPainterPath path(QPointF(15, 31));
    path.lineTo(15, 18);
    path.arcTo(QRectF(15, 15, 6, 6), 180, -90); // the rounded corner, to (18, 15)
    path.lineTo(31, 15);
    return path;
}

QPainterPath bottomRightCorner()
{
    QPainterPath path(QPointF(49, 33));
    path.lineTo(49, 46);
    path.arcTo(QRectF(43, 43, 6, 6), 0, -90); // to (46, 49)
    path.lineTo(33, 49);
    return path;
}

// Turns by `degrees` around the logo's centre (negative: counterclockwise
// on the screen, as CSS and SVG count).
QTransform turnedAroundCentre(qreal degrees)
{
    QTransform transform;
    transform.translate(kCentre.x(), kCentre.y());
    transform.rotate(degrees);
    transform.translate(-kCentre.x(), -kCentre.y());
    return transform;
}

} // namespace

LogoPose LogoPose::mix(const LogoPose &a, const LogoPose &b, qreal t)
{
    LogoPose p;
    p.turn = lerp(a.turn, b.turn, t);
    p.c1Offset = a.c1Offset + (b.c1Offset - a.c1Offset) * t;
    p.c2Offset = a.c2Offset + (b.c2Offset - a.c2Offset) * t;
    // Opacities and the flash do not overshoot.
    const qreal clamped = std::clamp(t, 0.0, 1.0);
    p.c1Opacity = lerp(a.c1Opacity, b.c1Opacity, clamped);
    p.c2Opacity = lerp(a.c2Opacity, b.c2Opacity, clamped);
    p.slashScale = std::max(0.0, lerp(a.slashScale, b.slashScale, t));
    p.slashTurn = lerp(a.slashTurn, b.slashTurn, t);
    p.slashShift = lerp(a.slashShift, b.slashShift, t);
    p.nudge = lerp(a.nudge, b.nudge, t);
    p.flash = lerp(a.flash, b.flash, clamped);
    p.ring = std::clamp(lerp(a.ring, b.ring, t), 0.0, 1.0);
    return p;
}

namespace LogoMark {

void paint(QPainter &painter, const QRectF &box, const LogoPose &pose, const QColor &color, qreal strokeWidth)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    // Logo units to the box.
    painter.translate(box.topLeft());
    painter.scale(box.width() / kViewSize, box.height() / kViewSize);
    painter.translate(-kViewOrigin, -kViewOrigin);

    QPen pen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setBrush(Qt::NoBrush);
    const auto stroke = [&](const QPainterPath &path, const QTransform &transform, qreal opacity) {
        if (opacity <= 0)
            return;
        QColor c = color;
        c.setAlphaF(c.alphaF() * std::min(opacity, 1.0));
        pen.setColor(c);
        painter.setPen(pen);
        painter.drawPath(transform.map(path));
    };

    const QTransform turn = turnedAroundCentre(pose.turn);
    const QTransform nudge = QTransform::fromTranslate(pose.nudge, 0);
    stroke(topLeftCorner(), QTransform::fromTranslate(pose.c1Offset.x(), pose.c1Offset.y()) * turn, pose.c1Opacity);
    stroke(bottomRightCorner(), QTransform::fromTranslate(pose.c2Offset.x(), pose.c2Offset.y()) * turn * nudge,
           pose.c2Opacity);

    // The slash: scaled from the centre, turned on its own, then moved.
    if (pose.slashScale > 0) {
        QTransform slash;
        slash.translate(kCentre.x(), kCentre.y());
        slash.scale(pose.slashScale, pose.slashScale);
        slash.translate(-kCentre.x(), -kCentre.y());
        slash = slash * turnedAroundCentre(pose.slashTurn) * QTransform::fromTranslate(pose.slashShift, 0) * nudge;
        QPainterPath line(QPointF(23, 41));
        line.lineTo(41, 23);
        stroke(line, slash, 1);
    }
    painter.restore();
}

LogoPose startHidden()
{
    LogoPose p;
    p.c1Offset = QPointF(9, 9);
    p.c2Offset = QPointF(-9, -9);
    p.c1Opacity = 0;
    p.c2Opacity = 0;
    p.slashScale = 0;
    return p;
}

LogoPose startShutter()
{
    LogoPose p;
    p.c1Offset = QPointF(2, 2);
    p.c2Offset = QPointF(-2, -2);
    p.flash = 0.8;
    return p;
}

LogoPose arrowTurned()
{
    LogoPose p;
    p.turn = -45;
    p.slashTurn = -45;
    return p;
}

LogoPose arrow()
{
    // The right corner turned by -45° is a ">" with its tip at (56, 32);
    // the slash, lying flat (another quarter turn) and moved right, ends
    // in that tip.
    LogoPose p = arrowTurned();
    p.slashTurn = -135;
    p.slashShift = 10;
    p.c1Opacity = 0;
    return p;
}

LogoPose timer(qreal rest)
{
    // Counterclockwise; at zero exactly one turn, upright again.
    LogoPose p;
    p.turn = -360 * (1 - rest);
    p.slashTurn = p.turn;
    p.ring = rest;
    return p;
}

} // namespace LogoMark

LogoAnimator::LogoAnimator(QObject *parent)
    : QObject(parent)
    , m_animation(new QVariantAnimation(this))
{
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (m_steps.isEmpty())
            return;
        m_pose = LogoPose::mix(m_from, m_steps.first().pose, value.toReal());
        emit changed();
    });
    connect(m_animation, &QVariantAnimation::finished, this, [this] {
        if (m_steps.isEmpty())
            return;
        m_pose = m_steps.takeFirst().pose;
        emit changed();
        next();
    });
}

void LogoAnimator::setPose(const LogoPose &pose)
{
    stop();
    m_pose = pose;
    emit changed();
}

void LogoAnimator::play(const QList<Step> &steps)
{
    stop();
    if (steps.isEmpty())
        return;
    if (Motion::reduced()) {
        // Fewer animations: straight to the end.
        m_pose = steps.last().pose;
        emit changed();
        emit finished();
        return;
    }
    m_steps = steps;
    next();
}

void LogoAnimator::stop()
{
    m_steps.clear();
    m_animation->stop();
}

bool LogoAnimator::isRunning() const
{
    return !m_steps.isEmpty();
}

void LogoAnimator::next()
{
    if (m_steps.isEmpty()) {
        emit finished();
        return;
    }
    m_from = m_pose;
    const Step &step = m_steps.first();
    m_animation->setDuration(std::max(1, step.ms));
    m_animation->setEasingCurve(step.easing);
    m_animation->start();
}

QList<LogoAnimator::Step> LogoAnimator::startSteps()
{
    // Frames out of the middle, the slash springs up, then a flash and the
    // frames twitch like a shutter: a photo of the slash.
    QEasingCurve spring(QEasingCurve::OutBack);
    spring.setOvershoot(2.2);
    LogoPose framesOut = LogoMark::startHidden();
    framesOut.c1Offset = framesOut.c2Offset = QPointF();
    framesOut.c1Opacity = framesOut.c2Opacity = 1;
    return {
        {LogoMark::startHidden(), 40, QEasingCurve::Linear},
        {framesOut, 400, QEasingCurve::OutCubic},
        {LogoPose(), 280, spring},
        {LogoMark::startShutter(), 120, QEasingCurve::InOutQuad},
        {LogoPose(), 400, QEasingCurve::OutCubic},
    };
}

QList<LogoAnimator::Step> LogoAnimator::updateSteps()
{
    // The bar grows first (UpdateBar), then the mark turns into an arrow
    // pointing right, nudges twice and turns back.
    const LogoPose rest;
    const LogoPose arrow = LogoMark::arrow();
    LogoPose nudged = arrow;
    nudged.nudge = 5;
    return {
        {rest, 260, QEasingCurve::Linear},
        {LogoMark::arrowTurned(), 300, QEasingCurve::OutCubic},
        {arrow, 300, QEasingCurve::OutCubic},
        {arrow, 80, QEasingCurve::Linear},
        {nudged, 140, QEasingCurve::InOutQuad},
        {arrow, 140, QEasingCurve::InOutQuad},
        {arrow, 70, QEasingCurve::Linear},
        {nudged, 140, QEasingCurve::InOutQuad},
        {arrow, 140, QEasingCurve::InOutQuad},
        {arrow, 330, QEasingCurve::Linear},
        {LogoMark::arrowTurned(), 300, QEasingCurve::OutCubic},
        {rest, 300, QEasingCurve::OutCubic},
    };
}

QList<LogoAnimator::Step> LogoAnimator::timerSteps(qreal toRest)
{
    // One tick of the egg timer: turn and ring together, with a little
    // spring at the end.
    QEasingCurve tick(QEasingCurve::OutBack);
    tick.setOvershoot(1.6);
    return {{LogoMark::timer(toRest), 450, tick}};
}

QList<LogoAnimator::Step> LogoAnimator::windUpSteps()
{
    QEasingCurve spring(QEasingCurve::OutBack);
    spring.setOvershoot(1.2);
    return {{LogoMark::timer(1), 650, spring}};
}

LogoMarkWidget::LogoMarkWidget(QWidget *parent)
    : QWidget(parent)
    , m_animator(new LogoAnimator(this))
{
    connect(m_animator, &LogoAnimator::changed, this, qOverload<>(&QWidget::update));
}

void LogoMarkWidget::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void LogoMarkWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // A square on the left, the strokes 1/9 of it wide (5 of 44 units).
    const qreal side = std::min(width(), height());
    const QRectF box(0, (height() - side) / 2, side, side);
    // Without a colour of its own: the text colour, which the style sheet sets.
    const QColor color = m_color.isValid() ? m_color : palette().color(QPalette::WindowText);
    LogoMark::paint(painter, box, m_animator->pose(), color, 5.5);
}
