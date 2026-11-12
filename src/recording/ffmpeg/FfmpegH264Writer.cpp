#include "recording/ffmpeg/FfmpegH264Writer.h"

#include <iterator>
#include <vector>

// FFmpeg is C; its headers need C linkage in C++.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace {

// FFmpeg's error codes as text (av_err2str is a C-only macro).
QString errorText(int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, text, sizeof text);
    return QString::fromUtf8(text);
}

struct Candidate
{
    const char *codec;
    bool vaapi;
    bool lowPower;
    const char *label;
};

// In the order tried (see the header).
constexpr Candidate kCandidates[] = {
    {"h264_vaapi", true, false, "h264_vaapi"},
    {"h264_vaapi", true, true, "h264_vaapi (low power)"},
    {"h264_nvenc", false, false, "h264_nvenc"},
};

} // namespace

FfmpegH264Writer::FfmpegH264Writer() = default;

FfmpegH264Writer::~FfmpegH264Writer()
{
    release();
}

bool FfmpegH264Writer::canEncode()
{
    // Probed once: opening an encoder takes a moment (the graphics driver).
    static const bool answer = [] {
        FfmpegH264Writer probe;
        return probe.openEncoder(QSize(320, 240), 30);
    }();
    return answer;
}

bool FfmpegH264Writer::fail(const QString &what, int code)
{
    m_error = code ? QStringLiteral("%1: %2").arg(what, errorText(code)) : what;
    return false;
}

bool FfmpegH264Writer::openEncoder(const QSize &size, int framesPerSecond)
{
    QString lastError = QStringLiteral("No H.264 encoder (VA-API or NVENC) in FFmpeg");
    // Tests, where no graphics card encodes: a software codec of FFmpeg's
    // own (e.g. "mpeg4") in the same MP4 container.
    const QByteArray testCodec = qgetenv("NSL_FFMPEG_TEST_CODEC");
    const Candidate testCandidate{testCodec.constData(), false, false, testCodec.constData()};
    const std::vector<Candidate> candidates = testCodec.isEmpty()
                                                  ? std::vector<Candidate>(std::begin(kCandidates), std::end(kCandidates))
                                                  : std::vector<Candidate>{testCandidate};
    for (const Candidate &candidate : candidates) {
        const AVCodec *codec = avcodec_find_encoder_by_name(candidate.codec);
        if (!codec)
            continue;
        if (candidate.vaapi) {
            // The graphics card's device (/dev/dri/renderD128 by default).
            if (!m_device) {
                const int status = av_hwdevice_ctx_create(&m_device, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
                if (status < 0) {
                    lastError = QStringLiteral("VA-API device: %1").arg(errorText(status));
                    continue;
                }
            }
            // A pool of frames on the card, in NV12, the encoder's input.
            if (!m_frames) {
                m_frames = av_hwframe_ctx_alloc(m_device);
                auto *frames = reinterpret_cast<AVHWFramesContext *>(m_frames->data);
                frames->format = AV_PIX_FMT_VAAPI;
                frames->sw_format = AV_PIX_FMT_NV12;
                frames->width = size.width();
                frames->height = size.height();
                frames->initial_pool_size = 16;
                const int status = av_hwframe_ctx_init(m_frames);
                if (status < 0) {
                    av_buffer_unref(&m_frames);
                    lastError = QStringLiteral("VA-API frames: %1").arg(errorText(status));
                    continue;
                }
            }
        }

        AVCodecContext *context = avcodec_alloc_context3(codec);
        context->width = size.width();
        context->height = size.height();
        // Milliseconds: frames come at irregular times (a frame is only
        // sent when the screen changed), so the file has a variable rate.
        context->time_base = AVRational{1, 1000};
        context->framerate = AVRational{framesPerSecond, 1};
        context->gop_size = framesPerSecond * 2;
        context->max_b_frames = 0;
        // About 0.1 bit per pixel and frame: sharp text in screen videos.
        context->bit_rate = qint64(size.width()) * size.height() * framesPerSecond / 10;
        if (m_format && (m_format->oformat->flags & AVFMT_GLOBALHEADER))
            context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        if (candidate.vaapi) {
            context->pix_fmt = AV_PIX_FMT_VAAPI;
            context->hw_frames_ctx = av_buffer_ref(m_frames);
        } else {
            context->pix_fmt = AV_PIX_FMT_YUV420P;
        }
        AVDictionary *options = nullptr;
        if (candidate.lowPower)
            av_dict_set(&options, "low_power", "1", 0);
        if (!candidate.vaapi && testCodec.isEmpty())
            av_dict_set(&options, "preset", "p4", 0); // NVENC: balanced speed and quality
        const int status = avcodec_open2(context, codec, &options);
        av_dict_free(&options);
        if (status < 0) {
            lastError = QStringLiteral("%1: %2").arg(QString::fromLatin1(candidate.label), errorText(status));
            avcodec_free_context(&context);
            continue;
        }
        if (!candidate.vaapi)
            av_buffer_unref(&m_frames); // not used by NVENC
        m_codec = context;
        m_encoderName = QString::fromLatin1(candidate.label);
        return true;
    }
    return fail(lastError);
}

bool FfmpegH264Writer::open(const QString &path, const QSize &size, int framesPerSecond)
{
    release();
    m_size = size;
    m_lastPts = -1;
    const QByteArray file = path.toUtf8();
    int status = avformat_alloc_output_context2(&m_format, nullptr, "mp4", file.constData());
    if (status < 0)
        return fail(QStringLiteral("MP4 container"), status);
    if (!openEncoder(size, framesPerSecond)) {
        const QString error = m_error;
        release();
        m_error = error;
        return false;
    }

    m_stream = avformat_new_stream(m_format, nullptr);
    avcodec_parameters_from_context(m_stream->codecpar, m_codec);
    m_stream->time_base = m_codec->time_base;
    status = avio_open(&m_format->pb, file.constData(), AVIO_FLAG_WRITE);
    if (status < 0)
        return fail(QStringLiteral("Opening the file"), status);
    // The index at the front, so the video starts playing at once on a page.
    AVDictionary *options = nullptr;
    av_dict_set(&options, "movflags", "+faststart", 0);
    status = avformat_write_header(m_format, &options);
    av_dict_free(&options);
    if (status < 0)
        return fail(QStringLiteral("Writing the MP4 header"), status);
    m_headerWritten = true;

    // The converted frame, in the encoder's software format.
    const AVPixelFormat target = m_frames ? AV_PIX_FMT_NV12 : AV_PIX_FMT_YUV420P;
    m_software = av_frame_alloc();
    m_software->format = target;
    m_software->width = size.width();
    m_software->height = size.height();
    status = av_frame_get_buffer(m_software, 0);
    if (status < 0)
        return fail(QStringLiteral("Frame buffer"), status);
    // QImage's 32-bit formats lie in memory as B, G, R, A (little endian).
    m_scale = sws_getContext(size.width(), size.height(), AV_PIX_FMT_BGRA, size.width(), size.height(), target,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_scale)
        return fail(QStringLiteral("Colour conversion"));
    m_packet = av_packet_alloc();
    return true;
}

bool FfmpegH264Writer::write(const QImage &frame, qint64 timestampUs)
{
    if (!m_codec || !m_scale)
        return fail(QStringLiteral("The writer is not open"));
    if (frame.size() != m_size)
        return fail(QStringLiteral("Frame size %1×%2 instead of %3×%4")
                        .arg(frame.width()).arg(frame.height()).arg(m_size.width()).arg(m_size.height()));
    const QImage image = (frame.format() == QImage::Format_RGB32 || frame.format() == QImage::Format_ARGB32)
                             ? frame
                             : frame.convertToFormat(QImage::Format_RGB32);

    // The encoder may still hold the last frame: then it gets a copy.
    int status = av_frame_make_writable(m_software);
    if (status < 0)
        return fail(QStringLiteral("Frame buffer"), status);
    const uint8_t *source[1] = {image.constBits()};
    const int stride[1] = {int(image.bytesPerLine())};
    sws_scale(m_scale, source, stride, 0, image.height(), m_software->data, m_software->linesize);

    // Milliseconds, strictly increasing.
    qint64 pts = timestampUs / 1000;
    if (pts <= m_lastPts)
        pts = m_lastPts + 1;
    m_lastPts = pts;

    AVFrame *input = m_software;
    AVFrame *onCard = nullptr;
    if (m_frames) {
        // VA-API: up to the graphics card first.
        onCard = av_frame_alloc();
        status = av_hwframe_get_buffer(m_frames, onCard, 0);
        if (status >= 0)
            status = av_hwframe_transfer_data(onCard, m_software, 0);
        if (status < 0) {
            av_frame_free(&onCard);
            return fail(QStringLiteral("Uploading the frame"), status);
        }
        input = onCard;
    }
    input->pts = pts;
    status = avcodec_send_frame(m_codec, input);
    av_frame_free(&onCard);
    if (status < 0)
        return fail(QStringLiteral("Encoding"), status);
    return drain(false);
}

bool FfmpegH264Writer::drain(bool flush)
{
    if (flush) {
        const int status = avcodec_send_frame(m_codec, nullptr);
        if (status < 0 && status != AVERROR_EOF)
            return fail(QStringLiteral("Flushing the encoder"), status);
    }
    for (;;) {
        int status = avcodec_receive_packet(m_codec, m_packet);
        if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
            return true;
        if (status < 0)
            return fail(QStringLiteral("Encoding"), status);
        av_packet_rescale_ts(m_packet, m_codec->time_base, m_stream->time_base);
        m_packet->stream_index = m_stream->index;
        status = av_interleaved_write_frame(m_format, m_packet); // takes the packet's data
        if (status < 0)
            return fail(QStringLiteral("Writing the file"), status);
    }
}

bool FfmpegH264Writer::close()
{
    if (!m_codec || !m_headerWritten) {
        release();
        return fail(QStringLiteral("The writer is not open"));
    }
    bool ok = drain(true);
    const int status = av_write_trailer(m_format);
    if (status < 0)
        ok = fail(QStringLiteral("Completing the file"), status);
    m_headerWritten = false;
    release();
    return ok;
}

void FfmpegH264Writer::release()
{
    if (m_format) {
        if (m_format->pb)
            avio_closep(&m_format->pb);
        avformat_free_context(m_format);
        m_format = nullptr;
    }
    m_stream = nullptr;
    avcodec_free_context(&m_codec);
    av_frame_free(&m_software);
    av_packet_free(&m_packet);
    sws_freeContext(m_scale);
    m_scale = nullptr;
    av_buffer_unref(&m_frames);
    av_buffer_unref(&m_device);
    m_headerWritten = false;
}
