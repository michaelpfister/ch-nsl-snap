// Tests for the capture backends: reading the GNOME extension's reply
// (D-011) and the picking geometry of the Windows overlay (D-022).

#include "capture/GlobalHotkeys.h"
#include "capture/PickGeometry.h"
#include "capture/RegionSelector.h"
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
    // --- Global hotkeys (F-42) ---

    void gnomeAccelerators()
    {
        const GlobalHotkeys keys;
        QCOMPARE(HotkeyKeys::gnomeAccelerator(keys.last), QStringLiteral("Print"));
        QCOMPARE(HotkeyKeys::gnomeAccelerator(keys.screen), QStringLiteral("<Shift>Print"));
        QCOMPARE(HotkeyKeys::gnomeAccelerator(keys.window), QStringLiteral("<Alt>Print"));
        QCOMPARE(HotkeyKeys::gnomeAccelerator(keys.region), QStringLiteral("<Control><Alt>Print"));
        QCOMPARE(HotkeyKeys::gnomeAccelerator(QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_S)),
                 QStringLiteral("<Shift><Super>s"));
        QCOMPARE(HotkeyKeys::gnomeAccelerator(QKeySequence(Qt::CTRL | Qt::Key_F9)), QStringLiteral("<Control>F9"));
        QVERIFY(HotkeyKeys::gnomeAccelerator(QKeySequence()).isEmpty()); // off
        QVERIFY(HotkeyKeys::gnomeAccelerator(QKeySequence(Qt::Key_Space)).isEmpty()); // not offered
    }

    void windowsHotkeys()
    {
        unsigned mods = 0, vk = 0;
        QVERIFY(HotkeyKeys::windowsHotkey(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Print), &mods, &vk));
        QCOMPARE(mods, 0x0001u | 0x0002u); // MOD_ALT | MOD_CONTROL
        QCOMPARE(vk, 0x2Cu);               // VK_SNAPSHOT
        QVERIFY(HotkeyKeys::windowsHotkey(QKeySequence(Qt::SHIFT | Qt::Key_F12), &mods, &vk));
        QCOMPARE(mods, 0x0004u);
        QCOMPARE(vk, 0x7Bu); // VK_F12
        QVERIFY(HotkeyKeys::windowsHotkey(QKeySequence(Qt::META | Qt::Key_A), &mods, &vk));
        QCOMPARE(mods, 0x0008u);
        QCOMPARE(vk, unsigned('A'));
        QVERIFY(!HotkeyKeys::windowsHotkey(QKeySequence(), &mods, &vk));
    }

    void everyOfferedKeyConverts()
    {
        for (Qt::Key key : HotkeyKeys::offeredKeys()) {
            unsigned mods = 0, vk = 0;
            QVERIFY(!HotkeyKeys::gnomeAccelerator(QKeySequence(key)).isEmpty());
            QVERIFY(HotkeyKeys::windowsHotkey(QKeySequence(key), &mods, &vk));
        }
    }

#ifdef NSL_HAVE_GNOME_BACKEND
    void configureOptionsForTheExtension()
    {
        HotkeyOptions options;
        options.delay = 5;
        options.regionByDragging = true;
        options.lastMode = CaptureResult::Kind::Region;
        options.hotkeys.window = QKeySequence(); // off
        const QVariantMap map = GnomeShellBackend::configureOptions(options);
        QCOMPARE(map.value(QStringLiteral("delay")).typeId(), QMetaType::UInt); // D-Bus u
        QCOMPARE(map.value(QStringLiteral("delay")).toUInt(), 5u);
        QCOMPARE(map.value(QStringLiteral("region-select")).toString(), QStringLiteral("drag"));
        QCOMPARE(map.value(QStringLiteral("last-mode")).toString(), QStringLiteral("region"));
        QCOMPARE(map.value(QStringLiteral("hotkeys-enabled")).toBool(), true);
        QCOMPARE(map.value(QStringLiteral("capture-last")).toStringList(), QStringList{QStringLiteral("Print")});
        QCOMPARE(map.value(QStringLiteral("capture-window")).toStringList(), QStringList());
    }
#endif

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

    void twoClicksSelectARegion()
    {
        RegionSelector selector;
        selector.press(QPoint(100, 50));
        selector.release(QPoint(100, 50));
        QCOMPARE(selector.state(), RegionSelector::State::Started);
        // The region follows the pointer, both corner pixels included.
        selector.move(QPoint(40, 80));
        QCOMPARE(selector.region(), QRect(QPoint(40, 50), QPoint(100, 80)));
        selector.press(QPoint(299, 149));
        QCOMPARE(selector.state(), RegionSelector::State::Done);
        QCOMPARE(selector.region(), QRect(100, 50, 200, 100));
    }

    void accidentalDoubleClickIsIgnored()
    {
        RegionSelector selector;
        selector.press(QPoint(10, 10));
        selector.release(QPoint(10, 10));
        selector.press(QPoint(11, 10)); // 2 × 1 px: too small
        QCOMPARE(selector.state(), RegionSelector::State::Started);
        selector.release(QPoint(11, 10));
        selector.press(QPoint(20, 30));
        QCOMPARE(selector.state(), RegionSelector::State::Done);
    }

    void dragWorksInBothModes()
    {
        for (const auto mode : {RegionSelector::Mode::Clicks, RegionSelector::Mode::Drag}) {
            RegionSelector selector(mode);
            selector.press(QPoint(10, 10));
            selector.move(QPoint(12, 12)); // within the threshold: still a click
            QCOMPARE(selector.state(), RegionSelector::State::Pressed);
            selector.move(QPoint(60, 40));
            QCOMPARE(selector.state(), RegionSelector::State::Dragging);
            selector.release(QPoint(60, 40));
            QCOMPARE(selector.state(), RegionSelector::State::Done);
            QCOMPARE(selector.region(), QRect(QPoint(10, 10), QPoint(60, 40)));
        }
    }

    void dragModeIgnoresPlainClicks()
    {
        RegionSelector selector(RegionSelector::Mode::Drag);
        selector.press(QPoint(10, 10));
        selector.release(QPoint(10, 10));
        QCOMPARE(selector.state(), RegionSelector::State::Idle);
        QVERIFY(selector.region().isEmpty());
    }

    void keyboardClicksAndDiscard()
    {
        // Enter or Space at the (arrow-key) position, in both modes.
        RegionSelector selector(RegionSelector::Mode::Drag);
        selector.click(QPoint(5, 5));
        QCOMPARE(selector.state(), RegionSelector::State::Started);
        selector.discard(); // Backspace
        QCOMPARE(selector.state(), RegionSelector::State::Idle);
        selector.click(QPoint(5, 5));
        selector.move(QPoint(9, 7));
        selector.click(QPoint(9, 7));
        QCOMPARE(selector.region(), QRect(5, 5, 5, 3));
        QCOMPARE(selector.state(), RegionSelector::State::Done);
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

    void screenReply()
    {
        QVariantMap info;
        info.insert(QStringLiteral("mode"), QStringLiteral("screen"));
        info.insert(QStringLiteral("scale"), 1.5);
        const CaptureResult result = GnomeShellBackend::resultFromReply(png(QSize(30, 20), Qt::green), info);
        QCOMPARE(result.kind, CaptureResult::Kind::Screen);
        QCOMPARE(result.scale, 1.5);
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
