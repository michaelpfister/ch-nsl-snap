// The app's mark (D-043): its poses, painting, and the animator with and
// without the system's "fewer animations".

#include "ui/LogoMark.h"

#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>

namespace {

// Paints `pose` into a 88 × 88 image (2 px per logo unit, origin at 10).
QImage painted(const LogoPose &pose)
{
    QImage image(88, 88, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    LogoMark::paint(painter, QRectF(0, 0, 88, 88), pose, Qt::black, 5);
    return image;
}

bool inked(const QImage &image, qreal logoX, qreal logoY)
{
    const QPoint p(qRound((logoX - 10) * 2), qRound((logoY - 10) * 2));
    return image.rect().contains(p) && qAlpha(image.pixel(p)) > 128;
}

} // namespace

class TestLogoMark : public QObject
{
    Q_OBJECT

private slots:
    void restIsTheLogo()
    {
        const QImage image = painted(LogoPose());
        QVERIFY(inked(image, 15, 25)); // left arm of the top-left corner
        QVERIFY(inked(image, 25, 15)); // its top arm
        QVERIFY(inked(image, 40, 49)); // the bottom-right corner
        QVERIFY(inked(image, 32, 32)); // the slash through the centre
        QVERIFY(!inked(image, 20, 44)); // empty bottom-left
    }

    void arrowPointsRight()
    {
        // The right corner becomes the head, the slash its shaft.
        const QImage image = painted(LogoMark::arrow());
        QVERIFY(inked(image, 53, 32));  // near the tip, on the axis
        QVERIFY(inked(image, 35, 32));  // the shaft
        QVERIFY(!inked(image, 15, 25)); // the left corner is gone
        QVERIFY(!inked(image, 32, 20)); // nothing above the shaft
    }

    void timerTurnsOnceAndEmptiesTheRing()
    {
        QCOMPARE(LogoMark::timer(1).turn, 0.0);
        QCOMPARE(LogoMark::timer(0.5).turn, -180.0);
        QCOMPARE(LogoMark::timer(0).turn, -360.0);
        QCOMPARE(LogoMark::timer(0.25).ring, 0.25);
        // A full turn looks like the logo again.
        QCOMPARE(painted(LogoMark::timer(0)), painted(LogoPose()));
    }

    void mixGoesBetween()
    {
        const LogoPose half = LogoPose::mix(LogoPose(), LogoMark::arrow(), 0.5);
        QCOMPARE(half.turn, -22.5);
        QCOMPARE(half.c1Opacity, 0.5);
        // Overshoot moves, but opacity stays within 0..1.
        const LogoPose past = LogoPose::mix(LogoPose(), LogoMark::arrow(), 1.2);
        QVERIFY(past.turn < -45);
        QCOMPARE(past.c1Opacity, 0.0);
    }

    void animatorPlaysToTheEnd()
    {
        qputenv("NSL_REDUCED_MOTION", "0");
        LogoAnimator animator;
        QSignalSpy finished(&animator, &LogoAnimator::finished);
        QSignalSpy changed(&animator, &LogoAnimator::changed);
        animator.play(LogoAnimator::startSteps());
        QVERIFY(animator.isRunning());
        QVERIFY(finished.wait(3000));
        QVERIFY(changed.size() > 5); // it moved in between
        QCOMPARE(animator.pose().flash, 0.0);
        QCOMPARE(animator.pose().slashScale, 1.0);
    }

    void fewerAnimationsJumpToTheEnd()
    {
        qputenv("NSL_REDUCED_MOTION", "1");
        LogoAnimator animator;
        QSignalSpy finished(&animator, &LogoAnimator::finished);
        animator.play(LogoAnimator::updateSteps());
        QCOMPARE(finished.size(), 1);
        QVERIFY(!animator.isRunning());
        QCOMPARE(animator.pose().turn, 0.0);
        qputenv("NSL_REDUCED_MOTION", "0");
    }
};

QTEST_MAIN(TestLogoMark)
#include "tst_logomark.moc"
