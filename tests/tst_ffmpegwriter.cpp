// Our own H.264 writer through FFmpeg (Linux, Q-28): conversion, time
// stamps and the MP4 file, here with FFmpeg's software "mpeg4" codec, as
// no graphics card encodes in CI; and VideoEncoder taking that path.

#include "recording/VideoEncoder.h"
#include "recording/ffmpeg/FfmpegH264Writer.h"

#include <QFile>
#include <QPainter>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

QImage frame(int i, const QSize &size)
{
    QImage image(size, QImage::Format_RGB32);
    image.fill(QColor::fromHsv((i * 12) % 360, 200, 220));
    QPainter painter(&image);
    painter.fillRect(QRect(i * 4 % size.width(), 20, 40, 40), Qt::black);
    return image;
}

// An MP4 file: "ftyp" at byte 4, and more than a header.
bool isMp4(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = file.read(12);
    return head.mid(4, 4) == "ftyp" && file.size() > 2000;
}

} // namespace

class TestFfmpegWriter : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("NSL_FFMPEG_TEST_CODEC", "mpeg4");
        QVERIFY(FfmpegH264Writer::canEncode());
    }

    void writesAnMp4()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("video.mp4"));
        const QSize size(320, 240);
        FfmpegH264Writer writer;
        QVERIFY2(writer.open(path, size, 30), qPrintable(writer.error()));
        // Irregular times, and two frames at the same millisecond.
        for (int i = 0; i < 40; ++i)
            QVERIFY2(writer.write(frame(i, size), i * 33333 + (i % 5 == 0 ? 20000 : 0)), qPrintable(writer.error()));
        QVERIFY2(writer.write(frame(40, size), 39 * 33333), qPrintable(writer.error()));
        QVERIFY2(writer.close(), qPrintable(writer.error()));
        QVERIFY(isMp4(path));
    }

    void rejectsAFrameOfAnotherSize()
    {
        QTemporaryDir dir;
        FfmpegH264Writer writer;
        QVERIFY(writer.open(dir.filePath(QStringLiteral("v.mp4")), QSize(320, 240), 30));
        QVERIFY(!writer.write(QImage(100, 100, QImage::Format_RGB32), 0));
        QVERIFY(!writer.error().isEmpty());
    }

    void videoEncoderTakesThisPath()
    {
        // The recording's path: frames on the GUI thread, written on the
        // encoder's thread, finished() when the file is complete.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("recording.mp4"));
        const QSize size(320, 240);
        VideoEncoder encoder;
        encoder.setVideoCodec(QMediaFormat::VideoCodec::MPEG4);
        encoder.setKeepAllFrames(true);
        QSignalSpy finished(&encoder, &VideoEncoder::finished);
        QSignalSpy failed(&encoder, &VideoEncoder::failed);
        encoder.start(path, size);
        for (int i = 0; i < 30; ++i)
            encoder.addFrame(frame(i, size), i * 33333);
        encoder.finish();
        QVERIFY(finished.wait(10000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(encoder.framesWritten(), 30);
        QVERIFY(isMp4(path));
    }
};

QTEST_GUILESS_MAIN(TestFfmpegWriter)
#include "tst_ffmpegwriter.moc"
