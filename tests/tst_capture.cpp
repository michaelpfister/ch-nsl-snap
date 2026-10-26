// Tests for the capture backends: reading the GNOME extension's reply
// (D-011) and the picking geometry of the Windows overlay (D-022).

#include "capture/PickGeometry.h"
#ifdef NSL_HAVE_GNOME_BACKEND
#include "capture/gnome/GnomeShellBackend.h"
#endif

#include <QBuffer>
#include <QTest>

#ifdef NSL_HAVE_GNOME_BACKEND
namespace {

QByteArray png(const QSize &size, const QColor &color)
{
    QImage image(size, QImage::Format_ARGB32);
    image.fill(color);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

} // namespace
#endif

class TestCapture : public QObject
{
    Q_OBJECT

private slots:
    void topmostWindowWins()
    {
        // Topmost first: a dialog above its main window.
        const QList<QRect> frames{QRect(100, 100, 200, 100), QRect(0, 0, 800, 600)};
        QCOMPARE(PickGeometry::topmostAt(frames, QPoint(150, 150)), 0);
        QCOMPARE(PickGeometry::topmostAt(frames, QPoint(50, 50)), 1);
        QCOMPARE(PickGeometry::topmostAt(frames, QPoint(900, 50)), -1);
    }

    void nativeAndLocalCoordinates()
    {
        // A second monitor at x = 2560 (native), scale 1.5.
        const QRect screen(2560, 0, 2880, 1620);
        QCOMPARE(PickGeometry::localToNative(QPointF(100, 10), screen, 1.5), QPoint(2710, 15));
        QCOMPARE(PickGeometry::nativeToLocal(QRect(2710, 15, 300, 150), screen, 1.5), QRectF(100, 10, 200, 100));
    }

    void frameInCaptureSkipsInvisibleBorders()
    {
        // Windows 10/11: the window rectangle has 7 px invisible borders
        // left, right and bottom around the visible frame.
        const QRect windowRect(93, 143, 814, 507);
        const QRect frame(100, 143, 800, 500);
        QCOMPARE(PickGeometry::frameInCapture(windowRect.size(), windowRect, frame), QRect(7, 0, 800, 500));
        // A capture that is already just the frame.
        QCOMPARE(PickGeometry::frameInCapture(frame.size(), windowRect, frame), QRect(0, 0, 800, 500));
        // Never outside the image.
        QCOMPARE(PickGeometry::frameInCapture(QSize(700, 400), windowRect, frame), QRect(0, 0, 700, 400));
    }

#ifdef NSL_HAVE_GNOME_BACKEND
    void regionReply()
    {
        QVariantMap info;
        info.insert(QStringLiteral("mode"), QStringLiteral("region"));
        info.insert(QStringLiteral("scale"), 2.0);
        const CaptureResult result = GnomeShellBackend::resultFromReply(png(QSize(40, 30), Qt::red), info);
        QCOMPARE(result.kind, CaptureResult::Kind::Region);
        QCOMPARE(result.image.size(), QSize(40, 30));
        QCOMPARE(result.scale, 2.0);
        QVERIFY(result.title.isEmpty());
    }

    void windowReply()
    {
        QVariantMap info;
        info.insert(QStringLiteral("mode"), QStringLiteral("window"));
        info.insert(QStringLiteral("title"), QStringLiteral("Dateien"));
        const CaptureResult result = GnomeShellBackend::resultFromReply(png(QSize(10, 10), Qt::blue), info);
        QCOMPARE(result.kind, CaptureResult::Kind::Window);
        QCOMPARE(result.title, QStringLiteral("Dateien"));
        QCOMPARE(result.scale, 1.0); // missing: scale 1
    }

    void versionOneReplyIsAWindow()
    {
        // Extension version 1 (PickWindow) sends no mode.
        QVariantMap info;
        info.insert(QStringLiteral("title"), QStringLiteral("Alt"));
        const CaptureResult result = GnomeShellBackend::resultFromReply(png(QSize(10, 10), Qt::blue), info);
        QCOMPARE(result.kind, CaptureResult::Kind::Window);
        QVERIFY(result.frameRect.isNull()); // no rects: no crash, empty
    }

    void brokenImage()
    {
        const CaptureResult result = GnomeShellBackend::resultFromReply("not a png", {});
        QVERIFY(result.image.isNull());
    }
#endif
};

QTEST_GUILESS_MAIN(TestCapture)
#include "tst_capture.moc"
