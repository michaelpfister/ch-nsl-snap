// Tests for window capture on Windows (D-022) with real windows: needs a
// desktop session, which the GitHub Windows runners have.

#include "capture/PickGeometry.h"
#include "capture/CaptureResult.h"
#include "capture/windows/WgcCapture.h"
#include "capture/windows/WindowsBackend.h"
#include "imaging/WindowImage.h"
#include "recording/RecordingController.h"
#include "recording/windows/WindowsRecordingBackend.h"

#include <QApplication>
#include <QDeadlineTimer>
#include <QFile>
#include <QMediaPlayer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QOperatingSystemVersion>
#include <QPainter>
#include <QTest>
#include <QWidget>

#include <cmath>
#include <functional>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h> // IsWindow, GetSystemMetrics

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

// A frame of a window stream, cut to the visible frame as the recording
// backend does it.
QImage streamFrame(const WgcCapture::Pixels &pixels, HWND hwnd)
{
    const QImage whole(pixels.bgra.data(), pixels.width, pixels.height, pixels.width * 4,
                       QImage::Format_ARGB32_Premultiplied);
    WgcCapture::Rect windowRect, frame;
    if (!WgcCapture::windowRects(hwnd, windowRect, frame))
        return {};
    return whole.copy(PickGeometry::frameInCapture(whole.size(), toQRect(windowRect), toQRect(frame)))
        .convertToFormat(QImage::Format_ARGB32);
}

// Waits up to `timeoutMs` for a frame from `stream` for which `wanted`
// holds. Not QTRY_VERIFY: that evaluates its expression once more after
// waiting, and takeLatest() hands each frame out only once.
bool waitForFrame(WgcCapture::Stream &stream, WgcCapture::Pixels &pixels, int timeoutMs,
                  const std::function<bool(const WgcCapture::Pixels &)> &wanted = {})
{
    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (stream.takeLatest(pixels) && (!wanted || wanted(pixels)))
            return true;
        QTest::qWait(20);
    }
    return false;
}

// The duration of a video as Qt reads it, in ms; -1 if it does not load.
qint64 durationOf(const QString &path)
{
    QMediaPlayer player;
    player.setSource(QUrl::fromLocalFile(path));
    QDeadlineTimer deadline(5000);
    while (player.mediaStatus() != QMediaPlayer::LoadedMedia && player.mediaStatus() != QMediaPlayer::InvalidMedia
           && !deadline.hasExpired())
        QTest::qWait(20);
    return player.mediaStatus() == QMediaPlayer::LoadedMedia ? player.duration() : -1;
}

// A recording controller for the tests: MPEG-4 Part 2 (software, on every
// runner), short countdown, files in `folder`.
void prepare(RecordingController &controller, const QString &folder)
{
    controller.setVideoCodec(QMediaFormat::VideoCodec::MPEG4);
    controller.setCountdownTickInterval(10);
    controller.setTemporaryFolder(folder);
}

bool similar(const QColor &a, const QColor &b)
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
        window.setWindowTitle(QStringLiteral("Snapslash Testfenster"));
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
        QVERIFY2(similar(image.pixelColor(image.width() / 4, y), kRed),
                 qPrintable(image.pixelColor(image.width() / 4, y).name(QColor::HexArgb)));
        QVERIFY2(similar(image.pixelColor(image.width() * 3 / 4, y), kBlue),
                 qPrintable(image.pixelColor(image.width() * 3 / 4, y).name(QColor::HexArgb)));
    }

    void roundedCornersBecomeTransparent()
    {
        // Windows 11 rounds the corners of normal windows (DWM); the result
        // must show them as transparent, like on GNOME (D-009, D-022).
        if (QOperatingSystemVersion::current() < QOperatingSystemVersion::Windows11)
            QSKIP("square window corners before Windows 11");
        Pattern window;
        window.setWindowTitle(QStringLiteral("Snapslash Testfenster"));
        window.resize(400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(500);

        const auto freeze = WgcCapture::Freeze::capture();
        WgcCapture::Window info;
        const QImage frame = captureOf(*freeze, window, &info);
        QVERIFY(!frame.isNull());
        // As the backend hands it to the editor.
        CaptureResult result;
        result.image = frame;
        result.scale = info.dpi / 96.0;
        result.frameRect = result.bufferRect = QRect(0, 0, int(std::ceil(frame.width() / result.scale)),
                                                     int(std::ceil(frame.height() / result.scale)));
        result.cornerRadius = WindowsBackend::cornerRadius(info);
        QCOMPARE(result.cornerRadius, 8.0);
        const QImage image = WindowImage::prepare(result);
        const auto alphaAt = [&image](int x, int y) { return image.pixelColor(x, y).alpha(); };
        const int right = image.width() - 1;
        const int bottom = image.height() - 1;
        QVERIFY2(alphaAt(0, 0) == 0 && alphaAt(right, 0) == 0 && alphaAt(0, bottom) == 0 && alphaAt(right, bottom) == 0,
                 qPrintable(QStringLiteral("corner alpha %1 %2 %3 %4 (raw %5), image %6x%7")
                                .arg(alphaAt(0, 0)).arg(alphaAt(right, 0)).arg(alphaAt(0, bottom))
                                .arg(alphaAt(right, bottom)).arg(frame.pixelColor(0, 0).alpha())
                                .arg(image.width()).arg(image.height())));
        // The middle of the edges stays opaque: the corners are cut, not the sides.
        QCOMPARE(alphaAt(image.width() / 2, 0), 255);
    }

    void coveredWindowIsCapturedWhole()
    {
        Pattern window;
        window.setWindowTitle(QStringLiteral("Snapslash Testfenster"));
        window.setGeometry(200, 200, 400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // A grey window right on top of it.
        QWidget cover;
        cover.setWindowTitle(QStringLiteral("Snapslash Abdeckung"));
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
        QVERIFY(similar(image.pixelColor(image.width() / 4, y), kRed));
        QVERIFY(similar(image.pixelColor(image.width() * 3 / 4, y), kBlue));
    }

    // Videos (F-50, D-039): the continuous capture.
    void streamFollowsAWindow()
    {
        auto *window = new Pattern;
        window->setWindowTitle(QStringLiteral("Snapslash Videofenster"));
        window->setGeometry(200, 200, 400, 300);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::qWait(300);
        const HWND hwnd = reinterpret_cast<HWND>(window->winId());

        auto stream = WgcCapture::Stream::forWindow(hwnd);
        QVERIFY2(stream, "the window stream did not start");
        WgcCapture::Pixels pixels;
        QVERIFY2(waitForFrame(*stream, pixels, 3000), stream->statistics().c_str());
        qInfo("window stream: %s", stream->statistics().c_str());
        QImage image = streamFrame(pixels, hwnd);
        QVERIFY(!image.isNull());
        const int y = image.height() - 20;
        QVERIFY2(similar(image.pixelColor(image.width() / 4, y), kRed),
                 qPrintable(image.pixelColor(image.width() / 4, y).name(QColor::HexArgb)));
        QVERIFY(similar(image.pixelColor(image.width() * 3 / 4, y), kBlue));
        const int firstWidth = image.width();
        // Nothing new until the window changes.
        QTest::qWait(200);
        stream->takeLatest(pixels);

        // Resized: the frames follow, at the new size.
        window->resize(600, 300);
        QVERIFY2(waitForFrame(*stream, pixels, 5000,
                              [&](const WgcCapture::Pixels &frame) {
                                  return streamFrame(frame, hwnd).width() >= firstWidth + 150;
                              }),
                 stream->statistics().c_str());
        image = streamFrame(pixels, hwnd);
        qInfo("window stream after resizing: %s", stream->statistics().c_str());
        qInfo("stream frame %dx%d after resizing from %d", image.width(), image.height(), firstWidth);
        QVERIFY(similar(image.pixelColor(image.width() * 3 / 4, image.height() - 20), kBlue));

        // Closed: the stream says so.
        delete window;
        QTRY_VERIFY_WITH_TIMEOUT(stream->isClosed() || !IsWindow(hwnd), 5000);
        qInfo("stream closed: %d", int(stream->isClosed()));
    }

    void streamOfAMonitor()
    {
        // The primary monitor holds the point 0,0.
        auto stream = WgcCapture::Stream::forMonitorAt(0, 0);
        QVERIFY2(stream, "the monitor stream did not start");
        WgcCapture::Pixels pixels;
        QVERIFY2(waitForFrame(*stream, pixels, 3000), stream->statistics().c_str());
        qInfo("monitor stream: %s", stream->statistics().c_str());
        qInfo("monitor frame %dx%d, screen %dx%d", pixels.width, pixels.height, GetSystemMetrics(SM_CXSCREEN),
              GetSystemMetrics(SM_CYSCREEN));
        QCOMPARE(pixels.width, GetSystemMetrics(SM_CXSCREEN));
        QCOMPARE(pixels.height, GetSystemMetrics(SM_CYSCREEN));
    }

    void recordsAWindow()
    {
        QTemporaryDir dir;
        Pattern window;
        window.setWindowTitle(QStringLiteral("Snapslash Videofenster"));
        window.setGeometry(200, 200, 400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(300);

        WindowsRecordingBackend backend;
        WindowsBackend::VideoPick pick;
        pick.hwnd = reinterpret_cast<HWND>(window.winId());
        pick.title = QStringLiteral("Videofenster");
        WgcCapture::Rect windowRect, frame;
        QVERIFY(WgcCapture::windowRects(pick.hwnd, windowRect, frame));
        pick.size = QSize(frame.width(), frame.height());
        backend.setPresetPick(pick);
        int frames = 0;
        connect(&backend, &RecordingBackend::frameReady, this, [&frames] { ++frames; });

        RecordingController controller(&backend);
        prepare(controller, dir.path());
        QSignalSpy started(&controller, &RecordingController::recordingStarted);
        QSignalSpy finished(&controller, &RecordingController::finished);
        QSignalSpy failed(&controller, &RecordingController::failed);
        controller.start(RecordingBackend::Source::Window);
        QVERIFY(started.wait(5000));
        // Some movement, so Windows sends frames.
        for (int i = 0; i < 10; ++i) {
            window.resize(400 + i * 4, 300);
            QTest::qWait(80);
        }
        controller.stop();
        QVERIFY(finished.wait(15000));
        QCOMPARE(failed.count(), 0);
        qInfo("%d frames from the window", frames);
        QVERIFY(frames > 0);
        const auto result = finished.first().first().value<RecordingController::Result>();
        QCOMPARE(result.title, QStringLiteral("Videofenster"));
        const qint64 duration = durationOf(result.path);
        QVERIFY2(duration > 500, qPrintable(QString::number(duration)));
    }

    void closingTheWindowEndsTheRecording()
    {
        QTemporaryDir dir;
        auto *window = new Pattern;
        window->setWindowTitle(QStringLiteral("Snapslash Videofenster"));
        window->setGeometry(200, 200, 400, 300);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::qWait(300);

        WindowsRecordingBackend backend;
        WindowsBackend::VideoPick pick;
        pick.hwnd = reinterpret_cast<HWND>(window->winId());
        pick.size = QSize(400, 300);
        backend.setPresetPick(pick);
        RecordingController controller(&backend);
        prepare(controller, dir.path());
        QSignalSpy started(&controller, &RecordingController::recordingStarted);
        QSignalSpy finished(&controller, &RecordingController::finished);
        controller.start(RecordingBackend::Source::Window);
        QVERIFY(started.wait(5000));
        QTest::qWait(500);
        delete window;
        QVERIFY(finished.wait(15000));
        const auto result = finished.first().first().value<RecordingController::Result>();
        QVERIFY(!result.endReason.isEmpty());
        QVERIFY(QFile::exists(result.path));
    }

    void windowListIsTopmostFirst()
    {
        Pattern below;
        below.setWindowTitle(QStringLiteral("Snapslash unten"));
        below.setGeometry(100, 100, 300, 200);
        below.show();
        QVERIFY(QTest::qWaitForWindowExposed(&below));
        Pattern above;
        above.setWindowTitle(QStringLiteral("Snapslash oben"));
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
