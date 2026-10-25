// Tests for the background blur (F-27).

#include "imaging/Blur.h"

#include <QTest>

class TestBlur : public QObject
{
    Q_OBJECT

private slots:
    void radiusZeroIsIdentity()
    {
        QImage image(10, 10, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QCOMPARE(Blur::boxBlur(image, 0), image);
    }

    void uniformImageStaysUniform()
    {
        QImage image(30, 20, QImage::Format_ARGB32);
        image.fill(QColor(40, 80, 120));
        const QImage out = Blur::boxBlur(image, 4).convertToFormat(QImage::Format_ARGB32);
        QCOMPARE(out.size(), image.size());
        QCOMPARE(out.pixelColor(0, 0), QColor(40, 80, 120));
        QCOMPARE(out.pixelColor(15, 10), QColor(40, 80, 120));
    }

    void edgeGetsSoft()
    {
        // Left half black, right half white: the boundary turns grey.
        QImage image(40, 10, QImage::Format_ARGB32);
        image.fill(Qt::white);
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 20; ++x)
                image.setPixelColor(x, y, Qt::black);
        const QImage out = Blur::boxBlur(image, 3).convertToFormat(QImage::Format_ARGB32);
        const int mid = out.pixelColor(20, 5).red();
        QVERIFY(mid > 60 && mid < 200);
        QCOMPARE(out.pixelColor(0, 5).red(), 0);
        QCOMPARE(out.pixelColor(39, 5).red(), 255);
    }

    void transparentAreaDoesNotTurnBlack()
    {
        // Premultiplied blurring: a white pixel next to transparency stays white.
        QImage image(20, 1, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        for (int x = 10; x < 20; ++x)
            image.setPixelColor(x, 0, Qt::white);
        const QColor edge = Blur::boxBlur(image, 2).convertToFormat(QImage::Format_ARGB32).pixelColor(10, 0);
        QVERIFY(edge.alpha() > 0 && edge.alpha() < 255);
        QCOMPARE(edge.red(), 255);
    }
};

QTEST_GUILESS_MAIN(TestBlur)
#include "tst_blur.moc"
