#pragma once

#include <QImage>
#include <QSize>
#include <QString>

struct AVBufferRef;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVPacket;
struct AVStream;
struct SwsContext;

// Writes H.264 into an MP4 file with FFmpeg directly, Linux only (Q-28,
// D-039). Qt Multimedia uses the same FFmpeg libraries but cannot set the
// encoder's options: many Intel graphics chips (Tiger Lake and newer)
// encode H.264 only in VA-API's low-power mode (VAEntrypointEncSliceLP),
// which FFmpeg uses only when asked (low_power=1). This writer asks.
//
// It tries, in this order: VA-API, VA-API in low-power mode, NVENC
// (NVIDIA). Frames come as QImage; they are converted to the encoder's
// pixel format (NV12 or YUV 4:2:0) with libswscale.
//
// Plain and blocking: VideoEncoder runs it on a thread of its own.
class FfmpegH264Writer
{
public:
    FfmpegH264Writer();
    ~FfmpegH264Writer();
    FfmpegH264Writer(const FfmpegH264Writer &) = delete;
    FfmpegH264Writer &operator=(const FfmpegH264Writer &) = delete;

    // Whether one of the encoders opens on this machine (tried once at a
    // small size, then remembered).
    static bool canEncode();

    // Starts `path` for frames of `size` (even width and height).
    bool open(const QString &path, const QSize &size, int framesPerSecond);
    // One frame from `timestampUs` microseconds on.
    bool write(const QImage &frame, qint64 timestampUs);
    // Flushes the encoder and completes the file.
    bool close();

    // Why the last call failed, in English (FFmpeg's own words).
    QString error() const { return m_error; }
    // The encoder that is open, e.g. "h264_vaapi (low power)".
    QString encoderName() const { return m_encoderName; }

private:
    bool openEncoder(const QSize &size, int framesPerSecond);
    bool drain(bool flush);
    void release();
    bool fail(const QString &what, int code = 0);

    AVBufferRef *m_device = nullptr; // VA-API device
    AVBufferRef *m_frames = nullptr; // VA-API frame pool
    AVCodecContext *m_codec = nullptr;
    AVFormatContext *m_format = nullptr;
    AVStream *m_stream = nullptr;
    AVFrame *m_software = nullptr; // the converted frame (NV12 or YUV 4:2:0)
    AVPacket *m_packet = nullptr;
    SwsContext *m_scale = nullptr;
    QSize m_size;
    qint64 m_lastPts = -1;
    bool m_headerWritten = false;
    QString m_error;
    QString m_encoderName;
};
