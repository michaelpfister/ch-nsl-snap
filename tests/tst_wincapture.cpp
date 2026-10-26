// Tests for window capture on Windows (D-022) with real windows: needs a
// desktop session, which the GitHub Windows runners have.

#include "capture/PickGeometry.h"
#include "capture/windows/WgcCapture.h"

#include <QApplication>
#include <QPainter>
#include <QTest>
#include <QWidget>

namespace {

const QColor kRed(0xE5484D);
const QColor kBlue(0x0090FF);

// Left half red, right half blue.
class Pattern : public QWidget
{
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(0, 0, width() / 2, height(), kRed);
        p.fillRect(width() / 2, 0, width() - width() / 2, height(), kBlue);
    }
};

QRect toQRect(const WgcCapture::Rect &r)
{
    return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

// The captured frame of `widget`, as the backend crops it.
QImage captureOf(const WgcCapture::Freeze &freeze, const QWidget &widget, WgcCapture::Window *found = nullptr)
{
    const auto &windows = freeze.windows();
    for (std::size_t i = 0; i < windows.size(); ++i) {
        if (windows[i].hwnd != reinterpret_cast<HWND>(widget.winId()))
            continue;
        WgcCapture::Pixels pixels;
        if (!freeze.pixels(i, pixels))
            return {};
        if (found)
            *found = windows[i];
        const QImage whole(pixels.bgra.data(), pixels.width, pixels.height, pixels.width * 4,
                           QImage::Format_ARGB32_Premultiplied);
        const QRect frame = PickGeometry::frameInCapture(whole.size(), toQRect(windows[i].windowRect),
                                                         toQRect(windows[i].frame));
        return whole.copy(frame).convertToFormat(QImage::Format_ARGB32);
    }
    return {};
}

bool near(const QColor &a, const QColor &b)
{
    return std::abs(a.red() - b.red()) <= 3 && std::abs(a.green() - b.green()) <= 3
           && std::abs(a.blue() - b.blue()) <= 3 && a.alpha() == 255;
}

} // namespace

class TestWinCapture : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!WgcCapture::isSupported())
            QSKIP("Windows.Graphics.Capture is not supported here");
    }

    void capturesAWindowWithItsFrame()
    {
        Pattern window;
        window.setWindowTitle(QStringLiteral("NSL Snap Testfenster"));
        window.resize(400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(500); // let DWM compose it

        const auto freeze = WgcCapture::Freeze::capture();
        WgcCapture::Window info;
        const QImage image = captureOf(*freeze, window, &info);
        QVERIFY2(!image.isNull(), "the test window was not frozen");
        qInfo("frame %dx%d, window rect %dx%d, image %dx%d, dpi %u, corner alpha %d",
              info.frame.width(), info.frame.height(), info.windowRect.width(), info.windowRect.height(),
              image.width(), image.height(), info.dpi, image.pixelColor(0, 0).alpha());

        // The visible frame: the client area plus the title bar, nothing of
        // the invisible borders.
        QCOMPARE(image.size(), QSize(info.frame.width(), info.frame.height()));
        // The client area at the bottom: red left, blue right.
        const int y = image.height() - 20;
        QVERIFY2(near(image.pixelColor(image.width() / 4, y), kRed),
                 qPrintable(image.pixelColor(image.width() / 4, y).name(QColor::HexArgb)));
        QVERIFY2(near(image.pixelColor(image.width() * 3 / 4, y), kBlue),
                 qPrintable(image.pixelColor(image.width() * 3 / 4, y).name(QColor::HexArgb)));
    }

    void coveredWindowIsCapturedWhole()
    {
        Pattern window;
        window.setWindowTitle(QStringLiteral("NSL Snap Testfenster"));
        window.setGeometry(200, 200, 400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // A grey window right on top of it.
        QWidget cover;
        cover.setWindowTitle(QStringLiteral("NSL Snap Abdeckung"));
        cover.setStyleSheet(QStringLiteral("background: #808080"));
        cover.setGeometry(window.geometry());
        cover.show();
        QVERIFY(QTest::qWaitForWindowExposed(&cover));
        cover.raise();
        QTest::qWait(500);

        const auto freeze = WgcCapture::Freeze::capture();
        const QImage image = captureOf(*freeze, window);
        QVERIFY2(!image.isNull(), "the covered window was not frozen");
        const int y = image.height() - 20;
        QVERIFY(near(image.pixelColor(image.width() / 4, y), kRed));
        QVERIFY(near(image.pixelColor(image.width() * 3 / 4, y), kBlue));
    }

    void windowListIsTopmostFirst()
    {
        Pattern below;
        below.setWindowTitle(QStringLiteral("NSL Snap unten"));
        below.setGeometry(100, 100, 300, 200);
        below.show();
        QVERIFY(QTest::qWaitForWindowExposed(&below));
        Pattern above;
        above.setWindowTitle(QStringLiteral("NSL Snap oben"));
        above.setGeometry(150, 150, 300, 200);
        above.show();
        QVERIFY(QTest::qWaitForWindowExposed(&above));
        above.raise();
        above.activateWindow();
        QTest::qWait(300);

        int belowIndex = -1;
        int aboveIndex = -1;
        const auto windows = WgcCapture::listWindows();
        for (std::size_t i = 0; i < windows.size(); ++i) {
            if (windows[i].hwnd == reinterpret_cast<HWND>(below.winId()))
                belowIndex = int(i);
            if (windows[i].hwnd == reinterpret_cast<HWND>(above.winId()))
                aboveIndex = int(i);
        }
        QVERIFY(belowIndex >= 0 && aboveIndex >= 0);
        QVERIFY(aboveIndex < belowIndex);
    }
};

// Real windows are needed: the "windows" platform even where the
// environment asks for offscreen (the CI's test step does).
int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "windows");
    QApplication app(argc, argv);
    TestWinCapture test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_wincapture.moc"
