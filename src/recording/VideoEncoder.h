#pragma once

#include <QImage>
#include <QMediaFormat>
#include <QObject>
#include <QString>

#include <deque>

class QMediaCaptureSession;
class QMediaRecorder;
class QVideoFrameInput;

// Writes the frames of a recording into an MP4 file with H.264 (F-50,
// D-039), through Qt Multimedia: the app hands each frame to a
// QVideoFrameInput, a QMediaCaptureSession passes them to a QMediaRecorder,
// which encodes and writes them on its own threads. The same code serves
// every platform; which H.264 encoder Qt finds depends on the machine
// (Q-28: on Linux, Qt's own FFmpeg only has the graphics card's, VA-API or
// NVENC).
class VideoEncoder : public QObject
{
    Q_OBJECT

public:
    static constexpr int kFramesPerSecond = 30;

    explicit VideoEncoder(QObject *parent = nullptr);
    ~VideoEncoder() override;

    // Whether Qt Multimedia can encode H.264 into MP4 on this machine.
    static bool canEncodeH264();

    // For the tests, which run where no H.264 encoder exists: another
    // codec in the same MP4 container. The app always uses H.264.
    void setVideoCodec(QMediaFormat::VideoCodec codec) { m_codec = codec; }

    // Starts a file at `path` for frames of `size` (even, VideoFrames::evenSize).
    // Errors come later through failed().
    void start(const QString &path, const QSize &size);

    // One frame, shown from `timestamp` (microseconds from the start) until
    // the next one. Frames must have the size given to start(). When the
    // encoder is still busy with earlier frames, only the newest waiting
    // frame is kept: a slow machine gets a jerkier video, not a growing
    // pile of frames in memory.
    void addFrame(const QImage &frame, qint64 timestamp);

    // Trimming (F-51): every frame counts, so waiting frames queue up
    // instead of replacing each other. The caller keeps the queue short
    // (queuedFrames(), frameSent()).
    void setKeepAllFrames(bool keep) { m_keepAll = keep; }
    int queuedFrames() const { return int(m_queue.size()); }

    // Ends the file; finished() comes when it is complete on disk.
    void finish();

    bool isRunning() const { return m_running; }
    // Frames handed to the encoder so far.
    int framesWritten() const { return m_written; }

signals:
    // A waiting frame went to the encoder.
    void frameSent();
    void finished();
    void failed(const QString &message);

private:
    struct Frame
    {
        QImage image;
        qint64 timestamp = 0;
    };

    bool send(const Frame &frame);
    void sendPending();
    void fail(const QString &message);

    QMediaFormat::VideoCodec m_codec = QMediaFormat::VideoCodec::H264;
    QMediaCaptureSession *m_session = nullptr;
    QMediaRecorder *m_recorder = nullptr;
    QVideoFrameInput *m_input = nullptr;
    QSize m_size;
    std::deque<Frame> m_queue; // frames waiting for room in the input
    bool m_keepAll = false;
    bool m_running = false;
    bool m_finishing = false;
    int m_written = 0;
};
