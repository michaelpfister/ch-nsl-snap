#pragma once

#include <QMediaFormat>
#include <QObject>
#include <QSize>
#include <QString>

class QMediaPlayer;
class QVideoFrame;
class QVideoSink;
class VideoEncoder;

// Writes a part of a video into a new MP4 file (F-51, D-039). The cut is
// exact to the frame, so the part is encoded anew (Q-29): Qt 6.8 has no
// decoder that hands out frame after frame, but a QMediaPlayer playing
// into a QVideoSink delivers every frame with its time stamp, also faster
// than real time. The trimmer plays the part at kSpeed and gives each frame
// to a VideoEncoder; when the encoder falls behind, it pauses the player
// until the encoder has caught up, so no frame is lost.
class VideoTrimmer : public QObject
{
    Q_OBJECT

public:
    // Times faster than real time. A test showed every frame arriving up
    // to 16 times faster; 4 leaves room for large videos on slow machines.
    static constexpr double kSpeed = 4.0;

    explicit VideoTrimmer(QObject *parent = nullptr);
    ~VideoTrimmer() override;

    // For the tests, as VideoEncoder::setVideoCodec().
    void setVideoCodec(QMediaFormat::VideoCodec codec) { m_codec = codec; }

    // Writes the frames of `input` that start in [startMs, endMs) to
    // `output`. Answers with finished() or failed(); progress() between.
    void start(const QString &input, const QString &output, qint64 startMs, qint64 endMs);
    void cancel();
    bool isRunning() const { return m_running; }

signals:
    void progress(int percent);
    void finished();
    void failed(const QString &message);

private:
    void onFrame(const QVideoFrame &frame);
    void onEncoderFrameSent();
    void endOfPart();
    void fail(const QString &message);
    void cleanUp();

    QMediaFormat::VideoCodec m_codec = QMediaFormat::VideoCodec::H264;
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_sink = nullptr;
    VideoEncoder *m_encoder = nullptr;
    QString m_output;
    qint64 m_startUs = 0;
    qint64 m_endUs = 0;
    qint64 m_lastUs = -1;    // start time of the last frame taken
    QSize m_size;
    bool m_running = false;
    bool m_ending = false;
    bool m_heldBack = false; // the player waits for the encoder
    int m_percent = -1;
};
