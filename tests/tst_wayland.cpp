// Tests for capture on Wayland without GNOME (D-031, D-032): Hyprland's
// window list, the app's own native coordinates, reading copied buffers,
// and the portal's request paths. The capture itself needs a compositor
// and is tried by hand (docs/07-development-setup.md).

#include "capture/PickGeometry.h"
#include "capture/portal/PortalBackend.h"
#include "capture/wayland/HyprlandIpc.h"
#include "capture/wayland/WaylandBackend.h"
#include "capture/wayland/WaylandFreeze.h"

#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTest>

#include <wayland-client-protocol.h>

#include <thread>

namespace {

// Shaped like `hyprctl -j monitors`: two monitors, the second with an open
// special workspace (scratchpad).
const QByteArray kMonitors = R"([
  {"id": 0, "name": "DP-1", "x": 0, "y": 0, "width": 2560, "height": 1440, "scale": 1.0,
   "activeWorkspace": {"id": 1, "name": "1"}, "specialWorkspace": {"id": 0, "name": ""}},
  {"id": 1, "name": "HDMI-A-1", "x": 2560, "y": 0, "width": 3840, "height": 2160, "scale": 2.0,
   "activeWorkspace": {"id": 2, "name": "2"}, "specialWorkspace": {"id": -98, "name": "special:magic"}}
])";

// Shaped like `hyprctl -j clients`.
const QByteArray kClients = R"([
  {"address": "0x55d0c3a1b2c0", "mapped": true, "hidden": false, "at": [10, 40], "size": [1200, 1380],
   "workspace": {"id": 1, "name": "1"}, "floating": false, "pinned": false, "fullscreen": 0, "monitor": 0,
   "class": "firefox", "title": "Tiled, focused", "focusHistoryID": 0},
  {"address": "0x55d0c3a1b3d0", "mapped": true, "hidden": false, "at": [400, 300], "size": [800, 600],
   "workspace": {"id": 1, "name": "1"}, "floating": true, "pinned": false, "fullscreen": 0, "monitor": 0,
   "class": "org.gnome.Nautilus", "title": "Floating", "focusHistoryID": 2},
  {"address": "0x55d0c3a1b4e0", "mapped": true, "hidden": false, "at": [1220, 40], "size": [1330, 1380],
   "workspace": {"id": 1, "name": "1"}, "floating": false, "pinned": false, "fullscreen": 0, "monitor": 0,
   "class": "kitty", "title": "Tiled, older", "focusHistoryID": 3},
  {"address": "0x55d0c3a1b5f0", "mapped": true, "hidden": false, "at": [100, 100], "size": [500, 500],
   "workspace": {"id": 3, "name": "3"}, "floating": false, "pinned": false, "fullscreen": 0, "monitor": 0,
   "class": "kitty", "title": "Other workspace", "focusHistoryID": 1},
  {"address": "0x55d0c3a1b600", "mapped": true, "hidden": true, "at": [10, 40], "size": [1200, 1380],
   "workspace": {"id": 1, "name": "1"}, "floating": false, "pinned": false, "fullscreen": 0, "monitor": 0,
   "class": "kitty", "title": "Hidden in a group", "focusHistoryID": 4},
  {"address": "0x55d0c3a1b710", "mapped": true, "hidden": false, "at": [2600, 50], "size": [1800, 1000],
   "workspace": {"id": -98, "name": "special:magic"}, "floating": true, "pinned": false, "fullscreen": false,
   "monitor": 1, "class": "pavucontrol", "title": "Scratchpad", "focusHistoryID": 5},
  {"address": "0x55d0c3a1b820", "mapped": true, "hidden": false, "at": [2560, 0], "size": [1920, 1080],
   "workspace": {"id": 2, "name": "2"}, "floating": false, "pinned": false, "fullscreen": 2, "monitor": 1,
   "class": "mpv", "title": "Full screen", "focusHistoryID": 6}
])";

} // namespace

class TestWayland : public QObject
{
    Q_OBJECT

private slots:
    void hyprlandVisibleWindowsTopmostFirst()
    {
        const QList<HyprlandIpc::Window> windows = HyprlandIpc::visibleWindows(kClients, kMonitors);
        QStringList titles;
        for (const HyprlandIpc::Window &window : windows)
            titles << window.title;
        // Not on an active workspace or hidden: left out. The open special
        // workspace on top, then full screen, floating, tiled by recency.
        QCOMPARE(titles, QStringList({QStringLiteral("Scratchpad"), QStringLiteral("Full screen"),
                                      QStringLiteral("Floating"), QStringLiteral("Tiled, focused"),
                                      QStringLiteral("Tiled, older")}));
    }

    void hyprlandWindowDetails()
    {
        const QList<HyprlandIpc::Window> windows = HyprlandIpc::visibleWindows(kClients, kMonitors);
        const HyprlandIpc::Window &floating = windows[2];
        // The protocol's handle is the lower 32 bits of the address.
        QCOMPARE(floating.handle, 0xc3a1b3d0u);
        QCOMPARE(floating.rect, QRect(400, 300, 800, 600));
        QCOMPARE(floating.monitor, QStringLiteral("DP-1"));
        QCOMPARE(floating.appClass, QStringLiteral("org.gnome.Nautilus"));
        QCOMPARE(windows[0].monitor, QStringLiteral("HDMI-A-1"));
    }

    void hyprlandSocketRequest()
    {
        // A stand-in for Hyprland's control socket, where Hyprland 0.40+
        // puts it: answers one command, then closes, as Hyprland does.
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        QVERIFY(QDir(runtime.path()).mkpath(QStringLiteral("hypr/test")));
        qputenv("XDG_RUNTIME_DIR", runtime.path().toUtf8());
        qputenv("HYPRLAND_INSTANCE_SIGNATURE", "test");
        const QString path = runtime.filePath(QStringLiteral("hypr/test/.socket.sock"));
        // The request blocks, so the server runs in a thread of its own.
        QByteArray received;
        std::thread server([path, &received] {
            QLocalServer listener;
            if (!listener.listen(path) || !listener.waitForNewConnection(5000))
                return;
            QLocalSocket *client = listener.nextPendingConnection();
            if (client->waitForReadyRead(5000))
                received = client->readAll();
            client->write(kClients);
            client->waitForBytesWritten(5000);
            client->disconnectFromServer();
        });
        // Wait until the server listens.
        QTRY_VERIFY(QFileInfo::exists(path));
        const QByteArray answer = HyprlandIpc::request("j/clients");
        server.join();
        QCOMPARE(received, QByteArray("j/clients"));
        QCOMPARE(answer, kClients);
        qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
        QVERIFY(HyprlandIpc::request("j/clients").isEmpty());
    }

    void hyprlandBrokenAnswers()
    {
        QVERIFY(HyprlandIpc::visibleWindows({}, {}).isEmpty());
        QVERIFY(HyprlandIpc::visibleWindows("not json", kMonitors).isEmpty());
        // Without monitors no window has a place.
        QVERIFY(HyprlandIpc::visibleWindows(kClients, "[]").isEmpty());
    }

    void nativeLayoutWithMixedScales()
    {
        // 2560 x 1440 at scale 1, right of it 3840 x 2160 at scale 2
        // (1920 x 1080 logical).
        const QList<QRect> logical{QRect(0, 0, 2560, 1440), QRect(2560, 0, 1920, 1080)};
        const QList<QSize> physical{QSize(2560, 1440), QSize(3840, 2160)};
        const QList<QRect> native = PickGeometry::nativeLayout(logical, physical);
        // Side by side without a gap or an overlap.
        QCOMPARE(native[0], QRect(0, 0, 2560, 1440));
        QCOMPARE(native[1], QRect(2560, 0, 3840, 2160));
    }

    void nativeLayoutStacked()
    {
        // A scale 2 laptop (1280 x 800 logical) below a scale 1 monitor,
        // and a third monitor right of both: nothing overlaps.
        const QList<QRect> logical{QRect(0, 0, 1920, 1080), QRect(320, 1080, 1280, 800), QRect(1920, 0, 1920, 1080)};
        const QList<QSize> physical{QSize(1920, 1080), QSize(2560, 1600), QSize(1920, 1080)};
        const QList<QRect> native = PickGeometry::nativeLayout(logical, physical);
        for (qsizetype i = 0; i < native.size(); ++i) {
            QCOMPARE(native[i].size(), physical[i]);
            for (qsizetype j = i + 1; j < native.size(); ++j)
                QVERIFY2(!native[i].intersects(native[j]), qPrintable(QStringLiteral("%1 and %2").arg(i).arg(j)));
        }
        // The laptop starts right below the monitor above it.
        QCOMPARE(native[1].top(), native[0].bottom() + 1);
    }

    void nativeLayoutSameScale()
    {
        // The common case: all monitors at 1.25; native is logical * 1.25 exactly.
        const QList<QRect> logical{QRect(0, 0, 2048, 1152), QRect(2048, 0, 2048, 1152)};
        const QList<QSize> physical{QSize(2560, 1440), QSize(2560, 1440)};
        const QList<QRect> native = PickGeometry::nativeLayout(logical, physical);
        QCOMPARE(native[1], QRect(2560, 0, 2560, 1440));
    }

    void logicalToNativeOnAMonitor()
    {
        const QRect screen(2560, 0, 1920, 1080);
        const QRect native(5120, 0, 3840, 2160);
        QCOMPARE(PickGeometry::logicalToNative(QRect(2600, 50, 1800, 1000), screen, native),
                 QRect(5200, 100, 3600, 2000));
        // Fractional scale: edges are rounded, neighbours still touch.
        const QRect fractional(0, 0, 2048, 1152);
        const QRect fractionalNative(0, 0, 2560, 1440);
        const QRect left = PickGeometry::logicalToNative(QRect(0, 0, 1023, 500), fractional, fractionalNative);
        const QRect right = PickGeometry::logicalToNative(QRect(1023, 0, 1025, 500), fractional, fractionalNative);
        QCOMPARE(left.right() + 1, right.left());
        QCOMPARE(right.right(), 2559);
    }

    void shmFormats()
    {
        QCOMPARE(WaylandFreeze::imageFormat(WL_SHM_FORMAT_ARGB8888), QImage::Format_ARGB32_Premultiplied);
        QCOMPARE(WaylandFreeze::imageFormat(WL_SHM_FORMAT_XRGB8888), QImage::Format_RGB32);
        QCOMPARE(WaylandFreeze::imageFormat(WL_SHM_FORMAT_XBGR8888), QImage::Format_RGBX8888);
        QCOMPARE(WaylandFreeze::imageFormat(WL_SHM_FORMAT_XRGB2101010), QImage::Format_RGB30);
        QCOMPARE(WaylandFreeze::imageFormat(WL_SHM_FORMAT_YUYV), QImage::Format_Invalid);
    }

    void bufferToImage()
    {
        // Two rows of ARGB8888, premultiplied: half-transparent red, then
        // opaque blue. In memory a little-endian word is B, G, R, A.
        const uchar data[] = {
            0x00, 0x00, 0x80, 0x80, // red at alpha 128, premultiplied
            0xff, 0x00, 0x00, 0xff, // opaque blue
        };
        const QImage image = WaylandFreeze::toImage(data, 1, 2, 4, WL_SHM_FORMAT_ARGB8888, false, true);
        QCOMPARE(image.format(), QImage::Format_ARGB32);
        QCOMPARE(qAlpha(image.pixel(0, 0)), 128);
        QCOMPARE(qRed(image.pixel(0, 0)), 255); // no longer premultiplied
        QCOMPARE(image.pixel(0, 1), qRgba(0, 0, 255, 255));

        // y_invert: the compositor wrote the rows bottom up.
        const QImage flipped = WaylandFreeze::toImage(data, 1, 2, 4, WL_SHM_FORMAT_ARGB8888, true, true);
        QCOMPARE(flipped.pixel(0, 0), qRgba(0, 0, 255, 255));

        // Screens are opaque; the copy owns its pixels.
        const QImage screen = WaylandFreeze::toImage(data, 1, 2, 4, WL_SHM_FORMAT_XRGB8888, false, false);
        QCOMPARE(screen.format(), QImage::Format_RGB32);
        QVERIFY(screen.constBits() != data);

        // XBGR8888: the bytes are R, G, B, X.
        const uchar rgbx[] = {0x10, 0x20, 0x30, 0x00};
        QCOMPARE(WaylandFreeze::toImage(rgbx, 1, 1, 4, WL_SHM_FORMAT_XBGR8888, false, false).pixel(0, 0),
                 qRgb(0x10, 0x20, 0x30));

        QVERIFY(WaylandFreeze::toImage(data, 1, 2, 4, WL_SHM_FORMAT_YUYV, false, false).isNull());
    }

    void windowPictureWithoutMargins()
    {
        // The usual case: the picture is exactly the window, at scale 1.5.
        QImage picture(1200, 900, QImage::Format_ARGB32);
        picture.fill(Qt::white);
        QCOMPARE(WaylandBackend::frameInPicture(picture, QSize(800, 600), 1.5), QRect(0, 0, 800, 600));
    }

    void windowPictureWithShadow()
    {
        // A client that draws its own shadow: 20 px transparent margins
        // around an 800 x 600 window, at scale 1.
        QImage picture(840, 640, QImage::Format_ARGB32);
        picture.fill(qRgba(0, 0, 0, 40));
        for (int y = 20; y < 620; ++y) {
            for (int x = 20; x < 820; ++x)
                picture.setPixel(x, y, qRgba(255, 255, 255, 255));
        }
        QCOMPARE(WaylandBackend::frameInPicture(picture, QSize(800, 600), 1.0), QRect(20, 20, 800, 600));
    }

    void portalRequestPath()
    {
        QCOMPARE(PortalBackend::requestPath(QStringLiteral(":1.42"), QStringLiteral("nsl_snap_7")),
                 QStringLiteral("/org/freedesktop/portal/desktop/request/1_42/nsl_snap_7"));
    }

    void portalSplitsTheDesktop()
    {
        // Two monitors at scale 2, the portal's picture in physical pixels.
        const QList<QRect> logical{QRect(0, 0, 1920, 1080), QRect(1920, 200, 1280, 720)};
        const QList<QRect> parts = PortalBackend::splitDesktop(QSize(6400, 2160), logical);
        QCOMPARE(parts[0], QRect(0, 0, 3840, 2160));
        QCOMPARE(parts[1], QRect(3840, 400, 2560, 1440));
        // A picture at the logical size: the parts are the logical rectangles.
        QCOMPARE(PortalBackend::splitDesktop(QSize(3200, 1080), {QRect(0, 0, 3200, 1080)})[0],
                 QRect(0, 0, 3200, 1080));
    }
};

QTEST_MAIN(TestWayland)
#include "tst_wayland.moc"
