// Tests for reading the GNOME extension's reply (D-011): window or region.

#include "capture/gnome/GnomeShellBackend.h"

#include <QBuffer>
#include <QTest>

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

class TestCapture : public QObject
{
    Q_OBJECT

private slots:
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
};

QTEST_GUILESS_MAIN(TestCapture)
#include "tst_capture.moc"
