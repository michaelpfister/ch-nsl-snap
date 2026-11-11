// Tests for video recording (F-50, D-039): the frame helpers, the encoder
// and the flow of RecordingController with a backend that paints its own
// frames. They encode MPEG-4 Part 2 into the same MP4 container: FFmpeg's
// software encoder, the same on every machine, whereas H.264 depends on
// the graphics card (Q-28). h264WhereAvailable() tries H.264 where Qt
// offers it.

#include "capture/CaptureOverlay.h"
#include "recording/RecordingBackend.h"
#include "recording/RecordingController.h"
#include "recording/RecordingIndicator.h"
#include "recording/TrimTimeline.h"
#include "recording/TrimWindow.h"
#include "recording/VideoEncoder.h"
#include "recording/VideoFrames.h"
#include "recording/VideoTrimmer.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QPushButton>
#include <QToolButton>
#include <QTest>
#include <QVideoFrame>
#include <QVideoSink>

namespace {

QMediaFormat::VideoCodec testCodec()
{
    return QMediaFormat::VideoCodec::MPEG4;
}

QImage solid(const QSize &size, const QColor &color, QImage::Format format = QImage::Format_RGB32)
{
    QImage image(size, format);
    image.fill(color);
    return image;
}

// A backend without a screen: the pick answers at once, every frame
// request is answered with a picture of a moving square.
class FakeBackend : public RecordingBackend
{
public:
    using RecordingBackend::RecordingBackend;

    bool isAvailable() const override { return true; }
    QString unavailableReason() const override { return {}; }
    bool supportsWindow() const override { return true; }

    void pick(Source source) override
    {
        ++picks;
        if (cancelPick) {
            QMetaObject::invokeMethod(this, [this] { emit cancelled(); }, Qt::QueuedConnection);
            return;
        }
        QMetaObject::invokeMethod(this, [this, source] { emit picked(source, QStringLiteral("Testfenster"), size); },
                                  Qt::QueuedConnection);
    }
    void startStream() override
    {
        streaming = true;
        if (failStart)
            emit streamEnded(QStringLiteral("kaputt"));
    }
    void requestFrame() override
    {
        if (!streaming || endAfter == 0)
            return;
        if (endAfter > 0)
            --endAfter;
        ++requests;
        QImage frame = solid(frameSize, Qt::darkBlue);
        QPainter painter(&frame);
        painter.fillRect((requests * 4) % frameSize.width(), 10, 20, 20, Qt::yellow);
        painter.end();
        QMetaObject::invokeMethod(this, [this, frame] { emit frameReady(frame); }, Qt::QueuedConnection);
        if (endAfter == 0)
            QMetaObject::invokeMethod(this, [this] { emit streamEnded(QStringLiteral("Fenster zu")); },
                                      Qt::QueuedConnection);
    }
    void stopStream() override { streaming = false; }

    QSize size = QSize(161, 121); // odd on purpose: the video is 160 x 120
    QSize frameSize = QSize(161, 121);
    bool cancelPick = false;
    bool failStart = false;
    int endAfter = -1; // frames until the stream ends by itself; -1: never
    bool streaming = false;
    int picks = 0;
    int requests = 0;
};

// The video's duration as Qt reads it back, in ms; -1 if it does not load.
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

// The colour of second `second` in makeVideo(): red, green, blue, ...
QColor colourOfSecond(int second)
{
    static const QColor colours[] = {Qt::red, Qt::green, Qt::blue, Qt::yellow};
    return colours[second % 4];
}

// A video of `seconds` seconds at 30 fps, one colour per second.
bool makeVideo(const QString &path, int seconds)
{
    VideoEncoder encoder;
    encoder.setVideoCodec(testCodec());
    encoder.setKeepAllFrames(true);
    QSignalSpy finished(&encoder, &VideoEncoder::finished);
    encoder.start(path, QSize(160, 120));
    for (int i = 0; i < seconds * VideoEncoder::kFramesPerSecond; ++i) {
        encoder.addFrame(solid(QSize(160, 120), colourOfSecond(i / VideoEncoder::kFramesPerSecond)),
                         qint64(i) * 1000000 / VideoEncoder::kFramesPerSecond);
        if (encoder.queuedFrames() > 10)
            QTest::qWait(5);
    }
    encoder.finish();
    return finished.wait(10000);
}

// The colour in the middle of the first and the last frame of a video.
std::pair<QColor, QColor> firstAndLastColour(const QString &path)
{
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);
    QColor first, last;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame &frame) {
        if (!frame.isValid())
            return;
        const QImage image = frame.toImage();
        const QColor colour = image.pixelColor(image.width() / 2, image.height() / 2);
        if (!first.isValid())
            first = colour;
        last = colour;
    });
    player.setSource(QUrl::fromLocalFile(path));
    player.setPlaybackRate(4);
    player.play();
    QDeadlineTimer deadline(10000);
    while (player.mediaStatus() != QMediaPlayer::EndOfMedia && player.mediaStatus() != QMediaPlayer::InvalidMedia
           && !deadline.hasExpired())
        QTest::qWait(20);
    return {first, last};
}

// Lossy encoding changes colours a little: which of red, green and blue it is.
char dominant(const QColor &colour)
{
    if (colour.red() > 150 && colour.green() < 100 && colour.blue() < 100)
        return 'r';
    if (colour.green() > 150 && colour.red() < 100 && colour.blue() < 100)
        return 'g';
    if (colour.blue() > 150 && colour.red() < 100 && colour.green() < 100)
        return 'b';
    return '?';
}

} // namespace

class TestRecording : public QObject
{
    Q_OBJECT

private slots:
    void evenSize()
    {
        QCOMPARE(VideoFrames::evenSize(QSize(1920, 1080)), QSize(1920, 1080));
        QCOMPARE(VideoFrames::evenSize(QSize(1001, 777)), QSize(1000, 776));
        QCOMPARE(VideoFrames::evenSize(QSize(1, 0)), QSize(2, 2));
    }

    void fitKeepsTheSameSize()
    {
        const QImage frame = solid(QSize(100, 80), Qt::red);
        const QImage fitted = VideoFrames::fit(frame, QSize(100, 80));
        QCOMPARE(fitted.size(), QSize(100, 80));
        QCOMPARE(fitted.format(), QImage::Format_RGB32);
        QCOMPARE(fitted.pixelColor(50, 40), QColor(Qt::red));
    }

    void fitCutsTheOddPixel()
    {
        QImage frame = solid(QSize(101, 81), Qt::red);
        frame.setPixelColor(100, 80, Qt::blue); // the dropped corner
        const QImage fitted = VideoFrames::fit(frame, QSize(100, 80));
        QCOMPARE(fitted.size(), QSize(100, 80));
        QCOMPARE(fitted.pixelColor(99, 79), QColor(Qt::red));
    }

    void fitScalesAResizedWindow()
    {
        // Twice as wide as the video: scaled to half, black above and below (Q-30).
        const QImage frame = solid(QSize(400, 100), Qt::green, QImage::Format_ARGB32);
        const QImage fitted = VideoFrames::fit(frame, QSize(200, 100));
        QCOMPARE(fitted.size(), QSize(200, 100));
        QCOMPARE(fitted.pixelColor(100, 50), QColor(Qt::green));
        QCOMPARE(fitted.pixelColor(100, 5), QColor(Qt::black));
        QCOMPARE(fitted.pixelColor(100, 94), QColor(Qt::black));
        QVERIFY(!fitted.hasAlphaChannel());
    }

    void framePixels()
    {
        // A window at 10,10 of 100 x 50 logical px in a picture at scale 2.
        QCOMPARE(VideoFrames::toPixels(QRect(10, 10, 100, 50), 2.0, QSize(300, 200)), QRect(20, 20, 200, 100));
        // Kept inside the picture.
        QCOMPARE(VideoFrames::toPixels(QRect(0, 0, 200, 200), 1.0, QSize(100, 50)), QRect(0, 0, 100, 50));
    }

    void indicatorTime()
    {
        QCOMPARE(RecordingIndicator::formatTime(0), QStringLiteral("0:00"));
        QCOMPARE(RecordingIndicator::formatTime(65'000), QStringLiteral("1:05"));
        QCOMPARE(RecordingIndicator::formatTime(600'000), QStringLiteral("10:00"));
        QCOMPARE(RecordingIndicator::formatTime(3'723'000), QStringLiteral("1:02:03"));
    }

    void encoderWritesAnMp4()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("video.mp4"));
        VideoEncoder encoder;
        encoder.setVideoCodec(testCodec());
        QSignalSpy finished(&encoder, &VideoEncoder::finished);
        QSignalSpy failed(&encoder, &VideoEncoder::failed);
        encoder.start(path, QSize(160, 120));
        QVERIFY(encoder.isRunning());
        // One second of frames, handed over as fast as the encoder takes them.
        for (int i = 0; i < VideoEncoder::kFramesPerSecond; ++i) {
            encoder.addFrame(solid(QSize(160, 120), QColor::fromHsv(i * 10, 200, 200)),
                             qint64(i) * 1000000 / VideoEncoder::kFramesPerSecond);
            QTest::qWait(5);
        }
        // A frame of the wrong size is ignored.
        encoder.addFrame(solid(QSize(10, 10), Qt::red), 1000000);
        encoder.finish();
        QVERIFY(finished.wait(10000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(encoder.framesWritten() > 0);
        QVERIFY(QFile(path).size() > 0);
        const qint64 duration = durationOf(path);
        QVERIFY2(duration > 700 && duration < 1300, qPrintable(QString::number(duration)));
    }

    void h264WhereAvailable()
    {
        // Opt-in: a listed hardware encoder can still fail on a machine
        // without a display (CI runners), which says nothing about the app.
        // Which encoders this machine has, in the log (Q-28).
        QMediaFormat mp4(QMediaFormat::MPEG4);
        qInfo() << "MP4 video encoders:" << mp4.supportedVideoCodecs(QMediaFormat::Encode);
        if (!qEnvironmentVariableIsSet("NSL_TEST_H264"))
            QSKIP("Set NSL_TEST_H264=1 to try the machine's H.264 encoder.");
        if (!VideoEncoder::canEncodeH264())
            QSKIP("Qt Multimedia offers no H.264 encoder here (Q-28).");
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("h264.mp4"));
        VideoEncoder encoder;
        QSignalSpy finished(&encoder, &VideoEncoder::finished);
        encoder.start(path, QSize(320, 240));
        for (int i = 0; i < VideoEncoder::kFramesPerSecond; ++i) {
            encoder.addFrame(solid(QSize(320, 240), QColor::fromHsv(i * 10, 200, 200)),
                             qint64(i) * 1000000 / VideoEncoder::kFramesPerSecond);
            QTest::qWait(5);
        }
        encoder.finish();
        QVERIFY(finished.wait(10000));
        QVERIFY(durationOf(path) > 0);
    }

    void recordsUntilStopped()
    {
        QTemporaryDir dir;
        FakeBackend backend;
        RecordingController controller(&backend);
        controller.setVideoCodec(testCodec());
        controller.setCountdownTickInterval(10);
        controller.setTemporaryFolder(dir.path());
        QSignalSpy started(&controller, &RecordingController::recordingStarted);
        QSignalSpy finished(&controller, &RecordingController::finished);
        QSignalSpy failed(&controller, &RecordingController::failed);

        QVERIFY(controller.isAvailable());
        controller.start(RecordingBackend::Source::Window);
        QCOMPARE(controller.state(), RecordingController::State::Picking);
        QVERIFY(started.wait(3000));
        QCOMPARE(controller.state(), RecordingController::State::Recording);
        // The indicator is visible the whole time (D-003).
        QVERIFY(controller.indicator());
        QVERIFY(controller.indicator()->isVisible());
        QTest::qWait(700);
        // A click on the indicator stops.
        QTest::mouseClick(controller.indicator(), Qt::LeftButton);
        QCOMPARE(controller.state(), RecordingController::State::Finishing);
        QVERIFY(!backend.streaming);
        QVERIFY(finished.wait(10000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(controller.state(), RecordingController::State::Idle);

        const auto result = finished.first().first().value<RecordingController::Result>();
        QCOMPARE(result.title, QStringLiteral("Testfenster"));
        QVERIFY(result.durationMs >= 600);
        QVERIFY(!result.limitReached);
        QVERIFY(QFile::exists(result.path));
        QVERIFY(result.path.startsWith(dir.path()));
        QVERIFY(result.path.endsWith(QStringLiteral(".mp4")));
        const qint64 duration = durationOf(result.path);
        QVERIFY2(duration > 400 && duration < 1500, qPrintable(QString::number(duration)));
    }

    void stopsAtTheLimit()
    {
        QTemporaryDir dir;
        FakeBackend backend;
        RecordingController controller(&backend);
        controller.setVideoCodec(testCodec());
        controller.setCountdownTickInterval(10);
        controller.setTemporaryFolder(dir.path());
        controller.setMaxDuration(400);
        QSignalSpy finished(&controller, &RecordingController::finished);
        controller.start(RecordingBackend::Source::Screen);
        QVERIFY(finished.wait(10000));
        const auto result = finished.first().first().value<RecordingController::Result>();
        QVERIFY(result.limitReached);
        QVERIFY(result.durationMs >= 400 && result.durationMs < 1000);
    }

    void resizedFramesFitTheVideo()
    {
        // The window grows after the pick: frames are scaled into the start size.
        QTemporaryDir dir;
        FakeBackend backend;
        backend.frameSize = QSize(320, 240);
        RecordingController controller(&backend);
        controller.setVideoCodec(testCodec());
        controller.setCountdownTickInterval(10);
        controller.setTemporaryFolder(dir.path());
        controller.setMaxDuration(300);
        QSignalSpy finished(&controller, &RecordingController::finished);
        controller.start(RecordingBackend::Source::Window);
        QVERIFY(finished.wait(10000));
        const auto result = finished.first().first().value<RecordingController::Result>();
        QVERIFY(durationOf(result.path) > 0);
    }

    void streamEndsByItself()
    {
        QTemporaryDir dir;
        FakeBackend backend;
        backend.endAfter = 10;
        RecordingController controller(&backend);
        controller.setVideoCodec(testCodec());
        controller.setCountdownTickInterval(10);
        controller.setTemporaryFolder(dir.path());
        QSignalSpy finished(&controller, &RecordingController::finished);
        controller.start(RecordingBackend::Source::Window);
        QVERIFY(finished.wait(10000));
        const auto result = finished.first().first().value<RecordingController::Result>();
        QCOMPARE(result.endReason, QStringLiteral("Fenster zu"));
        QVERIFY(QFile::exists(result.path));
    }

    void streamThatNeverStartsFails()
    {
        QTemporaryDir dir;
        FakeBackend backend;
        backend.failStart = true;
        RecordingController controller(&backend);
        controller.setVideoCodec(testCodec());
        controller.setCountdownTickInterval(10);
        controller.setTemporaryFolder(dir.path());
        QSignalSpy failed(&controller, &RecordingController::failed);
        QSignalSpy started(&controller, &RecordingController::recordingStarted);
        controller.start(RecordingBackend::Source::Screen);
        QVERIFY(failed.wait(3000));
        QCOMPARE(failed.first().first().toString(), QStringLiteral("kaputt"));
        QCOMPARE(started.count(), 0);
        QCOMPARE(controller.state(), RecordingController::State::Idle);
        QVERIFY(QDir(dir.path()).isEmpty()); // no file left behind
    }

    void cancelledPickAndCountdown()
    {
        FakeBackend backend;
        backend.cancelPick = true;
        RecordingController controller(&backend);
        controller.setVideoCodec(testCodec());
        QSignalSpy cancelled(&controller, &RecordingController::cancelled);
        controller.start(RecordingBackend::Source::Window);
        QVERIFY(cancelled.wait(3000));
        QCOMPARE(controller.state(), RecordingController::State::Idle);

        // Stopped during the countdown: cancelled, no recording.
        backend.cancelPick = false;
        controller.setCountdownTickInterval(5000);
        QSignalSpy started(&controller, &RecordingController::recordingStarted);
        controller.start(RecordingBackend::Source::Window);
        QTRY_COMPARE(controller.state(), RecordingController::State::CountingDown);
        controller.stop();
        QCOMPARE(cancelled.count(), 2);
        QCOMPARE(started.count(), 0);
        QCOMPARE(controller.state(), RecordingController::State::Idle);
    }

    void timelineFrames()
    {
        QCOMPARE(TrimTimeline::frameToMs(30), qint64(1000));
        QCOMPARE(TrimTimeline::msToFrame(1000), 30);
        QCOMPARE(TrimTimeline::msToFrame(1017), 31); // rounded to the nearest frame
        QCOMPARE(TrimTimeline::msToFrame(TrimTimeline::frameToMs(7)), 7);
    }

    void timelineHandles()
    {
        TrimTimeline timeline;
        timeline.resize(600, 60);
        timeline.setFrameCount(300);
        QVERIFY(timeline.isWhole());
        QCOMPARE(timeline.endFrame(), 300);
        QSignalSpy moved(&timeline, &TrimTimeline::handleMoved);

        // Right: one frame; Shift + Right: one second.
        QTest::keyClick(&timeline, Qt::Key_Right);
        QCOMPARE(timeline.startFrame(), 1);
        QTest::keyClick(&timeline, Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(timeline.startFrame(), 31);
        QVERIFY(!timeline.isWhole());
        QCOMPARE(moved.last().first().toInt(), 31);
        // Down: the end handle; it shows the last frame kept.
        QTest::keyClick(&timeline, Qt::Key_Down);
        QCOMPARE(timeline.selectedHandle(), TrimTimeline::Handle::End);
        QTest::keyClick(&timeline, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(timeline.endFrame(), 270);
        QCOMPARE(moved.last().first().toInt(), 269);
        // The handles never cross: at least one frame stays.
        timeline.setRange(100, 50);
        QCOMPARE(timeline.startFrame(), 100);
        QCOMPARE(timeline.endFrame(), 101);
        timeline.setRange(-5, 1000);
        QVERIFY(timeline.isWhole());

        // Dragging the end handle with the mouse.
        timeline.setRange(0, 300);
        const int endX = timeline.width() - 6; // the handle sits at the right end
        QTest::mousePress(&timeline, Qt::LeftButton, {}, QPoint(endX, 30));
        QCOMPARE(timeline.selectedHandle(), TrimTimeline::Handle::End);
        QMouseEvent move(QEvent::MouseMove, QPointF(timeline.width() / 2.0, 30), timeline.mapToGlobal(QPointF(timeline.width() / 2.0, 30)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&timeline, &move);
        QTest::mouseRelease(&timeline, Qt::LeftButton, {}, QPoint(timeline.width() / 2, 30));
        QVERIFY(qAbs(timeline.endFrame() - 150) <= 2);
    }

    void trimmerKeepsThePart()
    {
        QTemporaryDir dir;
        const QString input = dir.filePath(QStringLiteral("in.mp4"));
        const QString output = dir.filePath(QStringLiteral("out.mp4"));
        QVERIFY(makeVideo(input, 3));
        VideoTrimmer trimmer;
        trimmer.setVideoCodec(testCodec());
        QSignalSpy finished(&trimmer, &VideoTrimmer::finished);
        QSignalSpy failed(&trimmer, &VideoTrimmer::failed);
        QSignalSpy progress(&trimmer, &VideoTrimmer::progress);
        // The green second only.
        trimmer.start(input, output, 1000, 2000);
        QVERIFY(finished.wait(20000));
        QCOMPARE(failed.count(), 0);
        QVERIFY(progress.count() > 1);
        const qint64 duration = durationOf(output);
        QVERIFY2(duration > 900 && duration < 1100, qPrintable(QString::number(duration)));
        const auto [first, last] = firstAndLastColour(output);
        QCOMPARE(dominant(first), 'g');
        QCOMPARE(dominant(last), 'g');
    }

    void trimmerFailsOnABrokenFile()
    {
        QTemporaryDir dir;
        const QString input = dir.filePath(QStringLiteral("broken.mp4"));
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a video");
        file.close();
        VideoTrimmer trimmer;
        trimmer.setVideoCodec(testCodec());
        QSignalSpy failed(&trimmer, &VideoTrimmer::failed);
        trimmer.start(input, dir.filePath(QStringLiteral("out.mp4")), 0, 1000);
        QVERIFY(failed.wait(10000));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("out.mp4"))));
    }

    void trimWindowSavesThePart()
    {
        QTemporaryDir dir;
        const QString recording = dir.filePath(QStringLiteral("rec.mp4"));
        QVERIFY(makeVideo(recording, 3));
        auto *window = new TrimWindow(recording, QStringLiteral("Test"));
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setVideoCodec(testCodec());
        QSignalSpy saved(window, &TrimWindow::saved);
        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isLoaded(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!window->currentFrame().isNull(), 5000);
        QCOMPARE(window->timeline()->frameCount(), 90);
        // The icons are in the resources (play, save).
        QVERIFY(!window->findChild<QToolButton *>()->icon().pixmap(20).isNull());

        // From 2 s to the end: the blue second.
        window->timeline()->setRange(60, 90);
        QTRY_COMPARE_WITH_TIMEOUT(dominant(window->currentFrame().pixelColor(80, 60)), 'r', 2000);
        emit window->timeline()->handleMoved(60); // as a drag would: shows that frame
        QTRY_COMPARE_WITH_TIMEOUT(dominant(window->currentFrame().pixelColor(80, 60)), 'b', 5000);
        window->findChild<QPushButton *>(QStringLiteral("save"))->click();
        QVERIFY(saved.wait(20000));
        const QString path = saved.first().first().toString();
        QVERIFY(path != recording);
        QVERIFY(QFile::exists(path));
        const qint64 duration = durationOf(path);
        QVERIFY2(duration > 900 && duration < 1100, qPrintable(QString::number(duration)));
        // The window closed and deleted the untrimmed recording; the part stays.
        QTRY_VERIFY(!QFile::exists(recording));
        QVERIFY(QFile::exists(path));
    }

    void trimWindowSavesWholeOrDiscards()
    {
        QTemporaryDir dir;
        const QString recording = dir.filePath(QStringLiteral("rec.mp4"));
        QVERIFY(makeVideo(recording, 1));

        auto *window = new TrimWindow(recording, QString());
        window->setAttribute(Qt::WA_DeleteOnClose);
        QSignalSpy saved(window, &TrimWindow::saved);
        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isLoaded(), 5000);
        window->findChild<QPushButton *>(QStringLiteral("saveWhole"))->click();
        QCOMPARE(saved.count(), 1);
        QCOMPARE(saved.first().first().toString(), recording);
        QTest::qWait(50);
        QVERIFY(QFile::exists(recording)); // handed over, not deleted

        window = new TrimWindow(recording, QString());
        window->setAttribute(Qt::WA_DeleteOnClose);
        QSignalSpy discarded(window, &TrimWindow::discarded);
        window->show();
        QTRY_VERIFY_WITH_TIMEOUT(window->isLoaded(), 5000);
        window->findChild<QPushButton *>(QStringLiteral("discard"))->click();
        QCOMPARE(discarded.count(), 1);
        QTRY_VERIFY(!QFile::exists(recording));
    }

    void overlayForVideoHasNoRegion()
    {
        QScreen *screen = QGuiApplication::primaryScreen();
        CaptureOverlay::Screen s;
        s.screen = screen;
        s.native = QRect(QPoint(0, 0), screen->geometry().size());
        s.picture = solid(s.native.size(), Qt::black);
        // Without windows a video starts in screen mode, not region mode.
        CaptureOverlay overlay({s}, {}, CaptureOverlay::Mode::Window, RegionSelector::Mode::Clicks);
        overlay.setForVideo(true);
        QCOMPARE(overlay.mode(), CaptureOverlay::Mode::Screen);
        overlay.show();
        QWidget *window = overlay.windows().first();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::keyClick(window, Qt::Key_R);
        QCOMPARE(overlay.mode(), CaptureOverlay::Mode::Screen);
        QSignalSpy selected(&overlay, &CaptureOverlay::screenSelected);
        QTest::keyClick(window, Qt::Key_Return);
        QCOMPARE(selected.count(), 1);
    }
};

QTEST_MAIN(TestRecording)
#include "tst_recording.moc"
