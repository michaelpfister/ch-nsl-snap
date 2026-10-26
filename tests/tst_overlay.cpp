// Tests for the app's own capture overlay (Windows, later macOS; D-022,
// D-023), offscreen on a simulated HiDPI monitor: twice as many native
// pixels as logical ones.

#include "capture/CaptureOverlay.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

class TestOverlay : public QObject
{
    Q_OBJECT

private:
    CaptureOverlay *makeOverlay(CaptureOverlay::Mode mode, RegionSelector::Mode regionMode = RegionSelector::Mode::Clicks,
                                const QList<QRect> &frames = {})
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        CaptureOverlay::Screen s;
        s.screen = screen;
        s.native = QRect(QPoint(0, 0), screen->geometry().size() * 2);
        s.picture = QImage(s.native.size(), QImage::Format_RGB32);
        s.picture.fill(Qt::darkGray);
        auto *overlay = new CaptureOverlay({s}, frames, mode, regionMode, this);
        overlay->show();
        QWidget *window = overlay->windows().first();
        if (!QTest::qWaitForWindowExposed(window))
            qWarning("overlay not exposed");
        return overlay;
    }

private slots:
    void twoClicksAtPhysicalPixels()
    {
        CaptureOverlay *overlay = makeOverlay(CaptureOverlay::Mode::Region);
        QSignalSpy selected(overlay, &CaptureOverlay::regionSelected);
        QWidget *window = overlay->windows().first();
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(50, 40));
        QCOMPARE(overlay->region().state(), RegionSelector::State::Started);
        QCOMPARE(overlay->region().start(), QPoint(100, 80)); // logical 50 = native 100
        QTest::mouseMove(window, QPoint(120, 90));
        QCOMPARE(overlay->region().region(), QRect(QPoint(100, 80), QPoint(240, 180)));
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(150, 90));
        QCOMPARE(selected.count(), 1);
        QCOMPARE(selected.first().first().toRect(), QRect(QPoint(100, 80), QPoint(300, 180)));
        QVERIFY(!window->isVisible());
        delete overlay;
    }

    void arrowKeysAndEnterArePixelExact()
    {
        CaptureOverlay *overlay = makeOverlay(CaptureOverlay::Mode::Region);
        QSignalSpy selected(overlay, &CaptureOverlay::regionSelected);
        QWidget *window = overlay->windows().first();
        QTest::mouseMove(window, QPoint(50, 40));
        QCOMPARE(overlay->cursor(), QPoint(100, 80));
        // One physical pixel per key: half a logical pixel here.
        QTest::keyClick(window, Qt::Key_Right);
        QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(overlay->cursor(), QPoint(101, 81));
        QTest::keyClick(window, Qt::Key_Return); // first corner
        QTest::keyClick(window, Qt::Key_Right, Qt::ControlModifier); // 10 px
        QTest::keyClick(window, Qt::Key_Right, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Down, Qt::ControlModifier);
        QTest::keyClick(window, Qt::Key_Left);
        QCOMPARE(overlay->cursor(), QPoint(120, 91));
        QTest::keyClick(window, Qt::Key_Space); // second corner
        QCOMPARE(selected.count(), 1);
        QCOMPARE(selected.first().first().toRect(), QRect(QPoint(101, 81), QPoint(120, 91)));
        delete overlay;
    }

    void dragModeWantsADrag()
    {
        CaptureOverlay *overlay = makeOverlay(CaptureOverlay::Mode::Region, RegionSelector::Mode::Drag);
        QSignalSpy selected(overlay, &CaptureOverlay::regionSelected);
        QWidget *window = overlay->windows().first();
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(50, 40));
        QCOMPARE(overlay->region().state(), RegionSelector::State::Idle); // a click starts nothing
        QTest::mousePress(window, Qt::LeftButton, {}, QPoint(10, 10));
        for (int i = 1; i <= 5; ++i)
            QTest::mouseMove(window, QPoint(10 + 20 * i, 10 + 10 * i));
        QTest::mouseRelease(window, Qt::LeftButton, {}, QPoint(110, 60));
        QCOMPARE(selected.count(), 1);
        QCOMPARE(selected.first().first().toRect(), QRect(QPoint(20, 20), QPoint(220, 120)));
        delete overlay;
    }

    void backspaceStartsAgain()
    {
        CaptureOverlay *overlay = makeOverlay(CaptureOverlay::Mode::Region);
        QWidget *window = overlay->windows().first();
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(50, 40));
        QVERIFY(overlay->region().hasStart());
        QTest::keyClick(window, Qt::Key_Backspace);
        QVERIFY(!overlay->region().hasStart());
        delete overlay;
    }

    void windowModeAndSwitchingModes()
    {
        // Native frames: a dialog above a main window.
        const QList<QRect> frames{QRect(200, 200, 200, 100), QRect(100, 100, 600, 400)};
        CaptureOverlay *overlay = makeOverlay(CaptureOverlay::Mode::Window, RegionSelector::Mode::Clicks, frames);
        QSignalSpy picked(overlay, &CaptureOverlay::windowPicked);
        QWidget *window = overlay->windows().first();
        QTest::mouseMove(window, QPoint(60, 60)); // native 120: the main window
        QCOMPARE(overlay->hoveredWindow(), 1);
        QTest::keyClick(window, Qt::Key_R);
        QCOMPARE(overlay->mode(), CaptureOverlay::Mode::Region);
        QTest::keyClick(window, Qt::Key_W);
        QCOMPARE(overlay->mode(), CaptureOverlay::Mode::Window);
        QTest::mouseClick(window, Qt::LeftButton, {}, QPoint(110, 110)); // native 220: the dialog
        QCOMPARE(picked.count(), 1);
        QCOMPARE(picked.first().first().toInt(), 0);
        delete overlay;
    }

    void escapeAndRightClickCancel()
    {
        for (const bool rightClick : {false, true}) {
            CaptureOverlay *overlay = makeOverlay(CaptureOverlay::Mode::Region);
            QSignalSpy cancelled(overlay, &CaptureOverlay::cancelled);
            QWidget *window = overlay->windows().first();
            if (rightClick)
                QTest::mouseClick(window, Qt::RightButton, {}, QPoint(20, 20));
            else
                QTest::keyClick(window, Qt::Key_Escape);
            QCOMPARE(cancelled.count(), 1);
            delete overlay;
        }
    }

    void magnifierIsDrawn()
    {
        // A picture with one white pixel: in the magnifier it becomes a big square.
        QScreen *screen = QGuiApplication::primaryScreen();
        CaptureOverlay::Screen s;
        s.screen = screen;
        s.native = QRect(QPoint(0, 0), screen->geometry().size() * 2);
        s.picture = QImage(s.native.size(), QImage::Format_RGB32);
        s.picture.fill(Qt::black);
        s.picture.setPixel(301, 201, qRgb(255, 255, 255));
        CaptureOverlay overlay({s}, {}, CaptureOverlay::Mode::Region, RegionSelector::Mode::Clicks);
        overlay.show();
        QWidget *window = overlay.windows().first();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::mouseMove(window, QPoint(150, 100)); // native 300, 200
        QTest::keyClick(window, Qt::Key_Right);
        QTest::keyClick(window, Qt::Key_Down); // now on the white pixel
        const QImage shot = window->grab().toImage();
        if (!qEnvironmentVariableIsEmpty("NSL_TEST_SHOTS"))
            shot.save(qEnvironmentVariable("NSL_TEST_SHOTS") + QStringLiteral("/magnifier.png"));
        // The magnifier sits 24 px below right of the pointer; its middle
        // square (11 × 11 px) is the white pixel, its neighbours black.
        const double dpr = shot.devicePixelRatio();
        const QPointF centre(150.75 + 24 + 7 * 11 + 5.5, 100.75 + 24 + 7 * 11 + 5.5);
        const QPoint middle = (centre * dpr).toPoint();
        QCOMPARE(QColor(shot.pixel(middle)), QColor(Qt::white));
        QCOMPARE(QColor(shot.pixel(middle + QPoint(int(11 * dpr), 0))).lightness() < 60, true);
    }
};

QTEST_MAIN(TestOverlay)
#include "tst_overlay.moc"
