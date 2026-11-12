#pragma once

#include <QColor>
#include <QEasingCurve>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QWidget>

class QPainter;
class QRectF;
class QVariantAnimation;

// The app's mark: the three strokes of the logo (two frame corners and the
// slash) without the coloured tile, drawn in the colour of its place
// (D-043). The strokes move: they fly out at start, turn into an arrow for
// an update, and turn like an egg timer in the countdown.
//
// All lengths are in logo units: the logo is 64 × 64, the strokes lie
// between 10 and 54, the centre is (32, 32).

// Where the strokes are. The default is the logo at rest.
struct LogoPose
{
    qreal turn = 0;          // degrees, counterclockwise negative: both corners
    QPointF c1Offset;        // the top-left corner, moved by this
    QPointF c2Offset;        // the bottom-right corner
    qreal c1Opacity = 1;
    qreal c2Opacity = 1;
    qreal slashScale = 1;    // 0: a dot in the middle
    qreal slashTurn = 0;     // degrees, the slash on its own
    qreal slashShift = 0;    // moves the slash sideways after turning it
    qreal nudge = 0;         // moves the right corner and the slash to the right
    qreal flash = 0;         // 0..1, for the place behind the mark (start)
    qreal ring = 1;          // 0..1, the countdown's ring (Countdown)

    // In between `a` and `b`; t from 0 to 1 (and beyond, for overshoot).
    static LogoPose mix(const LogoPose &a, const LogoPose &b, qreal t);
};

namespace LogoMark {

// Paints the strokes into `box` (square; the strokes fill it).
void paint(QPainter &painter, const QRectF &box, const LogoPose &pose, const QColor &color, qreal strokeWidth = 5);

// The poses of the three animations, for the animator and the tests.
LogoPose startHidden();     // corners together in the middle, no slash
LogoPose startShutter();    // corners a little closed, flash on
LogoPose arrowTurned();     // turned 45° counterclockwise: right corner as ">"
LogoPose arrow();           // the slash as the arrow's shaft, left corner gone
LogoPose timer(qreal rest); // the countdown: rest 1 → 0, one turn in all

} // namespace LogoMark

// Plays a series of poses, one after the other, and says when the pose
// changes, so its owner repaints. With the system's "fewer animations"
// (Motion::reduced) it jumps to the last pose at once.
class LogoAnimator : public QObject
{
    Q_OBJECT

public:
    struct Step
    {
        LogoPose pose;
        int ms = 300;
        QEasingCurve easing = QEasingCurve::OutCubic;
    };

    explicit LogoAnimator(QObject *parent = nullptr);

    const LogoPose &pose() const { return m_pose; }
    void setPose(const LogoPose &pose);
    void play(const QList<Step> &steps);
    void stop();
    bool isRunning() const;

    // The three animations of D-043.
    static QList<Step> startSteps();
    static QList<Step> updateSteps();
    static QList<Step> timerSteps(qreal toRest);

signals:
    void changed();
    void finished();

private:
    void next();

    LogoPose m_pose;
    LogoPose m_from;
    QList<Step> m_steps;
    QVariantAnimation *m_animation = nullptr;
};

// The mark as a widget (the update bar): a square on the left, in the
// widget's text colour (style sheet "color") unless setColor() gives one.
class LogoMarkWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LogoMarkWidget(QWidget *parent = nullptr);

    LogoAnimator *animator() const { return m_animator; }
    void setColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    LogoAnimator *m_animator;
    QColor m_color;
};
