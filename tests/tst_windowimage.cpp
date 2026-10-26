// Tests for the window crop (D-010) and corner cleanup (D-009) with a
// synthetic capture that has the same features as a real libadwaita window:
// a shadow margin, rounded corners over the shadow, and a soft outline that
// lies inside the frame on three sides and outside it on the right.

#include "capture/CaptureResult.h"
#include "imaging/WindowImage.h"

#include <QPainter>
#include <QPainterPath>
#include <QTest>

namespace {

const QRect kFrame(20, 10, 160, 120);      // in the buffer image, scale 1
const QRect kCore(QPoint(22, 12), QPoint(179, 127));
const QColor kWindow(40, 44, 48);

int alpha(const QImage &image, int x, int y)
{
    return qAlpha(image.pixel(x, y));
}

void setPixel(QImage &image, int x, int y, int a)
{
    image.setPixel(x, y, qRgba(kWindow.red(), kWindow.green(), kWindow.blue(), a));
}

CaptureResult syntheticCapture(int windowAlpha = 255)
{
    QImage image(200, 150, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    {
        QPainter p(&image);
        // Shadow under and around the window.
        p.fillRect(QRect(10, 5, 180, 140), QColor(0, 0, 0, 40));
        // Opaque core with rounded corners, anti-aliased over the shadow.
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(kCore).adjusted(0, 0, 1, 1), 12, 12);
        QColor fill = kWindow;
        fill.setAlpha(windowAlpha);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillPath(path, fill);
    }
    // Soft outline (alpha 150), 2 px: inside the frame left, top and bottom,
    // outside it on the right.
    for (int y = 24; y < 116; ++y) {
        setPixel(image, 20, y, 150);
        setPixel(image, 21, y, 150);
        setPixel(image, 180, y, 150);
        setPixel(image, 181, y, 150);
    }
    for (int x = 34; x < 168; ++x) {
        setPixel(image, x, 10, 150);
        setPixel(image, x, 11, 150);
        setPixel(image, x, 128, 150);
        setPixel(image, x, 129, 150);
    }

    CaptureResult capture;
    capture.image = image;
    capture.bufferRect = QRect(100, 100, 200, 150);
    capture.frameRect = kFrame.translated(100, 100);
    capture.scale = 1.0;
    return capture;
}

} // namespace

class TestWindowImage : public QObject
{
    Q_OBJECT

private slots:
    void frameInImageUsesScale()
    {
        // Values from the first real capture (Files, scale 2).
        const QRect frame = WindowImage::frameInImage(QRect(101, 66, 1334, 776), QRect(54, 29, 1428, 870), 2.0);
        QCOMPARE(frame, QRect(94, 74, 2668, 1552));
    }

    void opaqueCoreIgnoresSoftOutline()
    {
        const CaptureResult capture = syntheticCapture();
        const auto core = WindowImage::opaqueCore(capture.image, kFrame, capture.scale);
        QVERIFY(core.has_value());
        QCOMPARE(*core, kCore);
    }

    void translucentWindowHasNoCore()
    {
        const CaptureResult capture = syntheticCapture(200);
        QVERIFY(!WindowImage::opaqueCore(capture.image, kFrame, capture.scale).has_value());
    }

    void cleanCornersRemovesShadowOnly()
    {
        QImage image = syntheticCapture().image.copy(kCore);
        QCOMPARE(alpha(image, 0, 0), 40); // shadow shows through the corner
        const int cleared = WindowImage::cleanCorners(image, 1.0);
        QVERIFY(cleared > 0);
        const int w = image.width();
        const int h = image.height();
        QCOMPARE(alpha(image, 0, 0), 0);
        QCOMPARE(alpha(image, w - 1, 0), 0);
        QCOMPARE(alpha(image, 0, h - 1), 0);
        QCOMPARE(alpha(image, w - 1, h - 1), 0);
        // The window itself is untouched.
        QCOMPARE(alpha(image, w / 2, 0), 255);
        QCOMPARE(alpha(image, 20, 20), 255);
    }

    void squareCornersAreLeftAlone()
    {
        QImage image(50, 40, QImage::Format_ARGB32);
        image.fill(kWindow);
        QCOMPARE(WindowImage::cleanCorners(image, 1.0), 0);
        QCOMPARE(alpha(image, 0, 0), 255);
    }

    void prepareGivesHardEdgesAndCleanCorners()
    {
        const QImage image = WindowImage::prepare(syntheticCapture());
        QCOMPARE(image.size(), kCore.size());
        const int w = image.width();
        const int h = image.height();
        QCOMPARE(alpha(image, 0, h / 2), 255);
        QCOMPARE(alpha(image, w - 1, h / 2), 255);
        QCOMPARE(alpha(image, w / 2, 0), 255);
        QCOMPARE(alpha(image, w / 2, h - 1), 255);
        QCOMPARE(alpha(image, 0, 0), 0);
        QCOMPARE(alpha(image, w - 1, h - 1), 0);
    }

    void squareCaptureGetsTheSystemsRoundedCorners()
    {
        // As Windows 11 delivers a window (D-022): square, opaque, with a
        // 1 px semi-transparent border; DWM rounds it only on the screen.
        QImage image(300, 200, QImage::Format_ARGB32);
        image.fill(qRgba(40, 44, 48, 108));
        QPainter(&image).fillRect(image.rect().adjusted(1, 1, -1, -1), kWindow);
        CaptureResult capture;
        capture.image = image;
        capture.scale = 1.5;
        capture.frameRect = capture.bufferRect = QRect(0, 0, 200, 134);
        capture.cornerRadius = 8;

        const QImage out = WindowImage::prepare(capture);
        QCOMPARE(out.size(), QSize(298, 198)); // the border is trimmed (D-010)
        const int w = out.width();
        const int h = out.height();
        for (const QPoint corner : {QPoint(0, 0), QPoint(w - 1, 0), QPoint(0, h - 1), QPoint(w - 1, h - 1)})
            QCOMPARE(alpha(out, corner.x(), corner.y()), 0);
        // 8 logical px = 12 image px: the sides and the inside stay opaque.
        QCOMPARE(alpha(out, w / 2, 0), 255);
        QCOMPARE(alpha(out, 0, h / 2), 255);
        QCOMPARE(alpha(out, 12, 12), 255);
        QCOMPARE(alpha(out, 13, 0), 255);
        // Without a radius the corners stay square.
        capture.cornerRadius = 0;
        QCOMPARE(alpha(WindowImage::prepare(capture), 0, 0), 255);
    }

    void prepareFallsBackToFrameForTranslucentWindows()
    {
        const QImage image = WindowImage::prepare(syntheticCapture(200));
        QCOMPARE(image.size(), kFrame.size());
    }
};

QTEST_GUILESS_MAIN(TestWindowImage)
#include "tst_windowimage.moc"
