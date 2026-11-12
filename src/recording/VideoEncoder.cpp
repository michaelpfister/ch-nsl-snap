#include "recording/VideoEncoder.h"

#ifdef NSL_HAVE_FFMPEG_ENCODER
#include "recording/ffmpeg/FfmpegH264Writer.h"
#endif

#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QUrl>
#include <QVideoFrame>
#include <QThread>
#include <QVideoFrameInput>

namespace {

// How long one frame is shown, in microseconds.
constexpr qint64 kFrameDuration = 1000000 / VideoEncoder::kFramesPerSecond;
// Frames handed to the FFmpeg writer at once, as Qt's input queue: more
// wait in m_queue (recording: only the newest).
constexpr int kMaxInFlight = 2;

} // namespace

VideoEncoder::VideoEncoder(QObject *parent)
    : QObject(parent)
{
}

VideoEncoder::~VideoEncoder()
{
    stopThread();
    // The recorder must stop before the session and input go.
    if (m_recorder && m_recorder->recorderState() != QMediaRecorder::StoppedState)
        m_recorder->stop();
    delete m_recorder;
    delete m_session;
    delete m_input;
}

bool VideoEncoder::canEncodeH264()
{
#ifdef NSL_HAVE_FFMPEG_ENCODER
    if (FfmpegH264Writer::canEncode())
        return true;
#endif
    QMediaFormat mp4(QMediaFormat::MPEG4); // supportedVideoCodecs() is not const
    return mp4.supportedVideoCodecs(QMediaFormat::Encode).contains(QMediaFormat::VideoCodec::H264);
}

void VideoEncoder::start(const QString &path, const QSize &size)
{
    if (m_running)
        return;
    m_size = size;
    m_written = 0;
    m_queue.clear();
    m_finishing = false;

    if (useFfmpeg()) {
        startFfmpeg(path);
        return;
    }

    // Without a format given here the input takes it from the first frame
    // (Qt 6.8: with a fixed format, frames were not accepted in a test).
    m_input = new QVideoFrameInput;
    m_session = new QMediaCaptureSession;
    m_recorder = new QMediaRecorder;
    m_session->setVideoFrameInput(m_input);
    m_session->setRecorder(m_recorder);

    QMediaFormat format(QMediaFormat::MPEG4);
    format.setVideoCodec(m_codec);
    // No audio (F-50).
    format.setAudioCodec(QMediaFormat::AudioCodec::Unspecified);
    m_recorder->setMediaFormat(format);
    m_recorder->setVideoResolution(size);
    m_recorder->setVideoFrameRate(kFramesPerSecond);
    m_recorder->setQuality(QMediaRecorder::HighQuality);
    m_recorder->setOutputLocation(QUrl::fromLocalFile(path));

    // The input's queue has room again.
    connect(m_input, &QVideoFrameInput::readyToSendVideoFrame, this, &VideoEncoder::sendPending);
    connect(m_recorder, &QMediaRecorder::errorOccurred, this,
            [this](QMediaRecorder::Error, const QString &message) { fail(message); });
    connect(m_recorder, &QMediaRecorder::recorderStateChanged, this, [this](QMediaRecorder::RecorderState state) {
        if (state != QMediaRecorder::StoppedState || !m_running)
            return;
        m_running = false;
        if (m_recorder->error() == QMediaRecorder::NoError)
            emit finished();
    });

    m_running = true;
    m_recorder->record();
}

void VideoEncoder::addFrame(const QImage &frame, qint64 timestamp)
{
    if (!m_running || m_finishing || frame.size() != m_size)
        return;
    Frame next{frame, timestamp};
    if (m_writer) {
        // Room at the writer: straight on; else it waits, as below.
        if (m_queue.empty() && m_inFlight < kMaxInFlight) {
            sendToWriter(std::move(next));
            return;
        }
        if (!m_keepAll)
            m_queue.clear();
        m_queue.push_back(std::move(next));
        return;
    }
    if (m_queue.empty() && send(next))
        return;
    // Earlier frames still wait: while recording, the newest replaces the
    // waiting one; while trimming, it queues up.
    if (!m_keepAll)
        m_queue.clear();
    m_queue.push_back(std::move(next));
}

bool VideoEncoder::send(const Frame &frame)
{
    QVideoFrame video(frame.image);
    video.setStartTime(frame.timestamp);
    video.setEndTime(frame.timestamp + kFrameDuration);
    if (!m_input->sendVideoFrame(video))
        return false;
    ++m_written;
    return true;
}

void VideoEncoder::sendPending()
{
    bool sent = false;
    while (!m_queue.empty() && send(m_queue.front())) {
        m_queue.pop_front();
        sent = true;
    }
    if (sent)
        emit frameSent();
    if (m_finishing && m_queue.empty() && m_recorder->recorderState() == QMediaRecorder::RecordingState)
        m_recorder->stop();
}

void VideoEncoder::finish()
{
    if (!m_running || m_finishing)
        return;
    m_finishing = true;
    if (m_writer) {
        pumpWriter(); // closes once the waiting frames are written
        return;
    }
    // Stops once the waiting frames are in (sendPending).
    sendPending();
}

void VideoEncoder::fail(const QString &message)
{
    if (!m_running)
        return;
    m_running = false;
    m_queue.clear();
    emit failed(message.isEmpty() ? tr("Das Video konnte nicht geschrieben werden.") : message);
}

// --- Our own FFmpeg writer (Linux) -----------------------------------------

bool VideoEncoder::useFfmpeg() const
{
#ifdef NSL_HAVE_FFMPEG_ENCODER
    const bool testCodec = qEnvironmentVariableIsSet("NSL_FFMPEG_TEST_CODEC");
    return (m_codec == QMediaFormat::VideoCodec::H264 || testCodec) && FfmpegH264Writer::canEncode();
#else
    return false;
#endif
}

void VideoEncoder::startFfmpeg(const QString &path)
{
#ifdef NSL_HAVE_FFMPEG_ENCODER
    // The writer blocks (conversion, upload to the graphics card), so it
    // works on a thread of its own; the work is queued to m_worker there,
    // and each result comes back here, again queued.
    m_writer = std::make_shared<FfmpegH264Writer>();
    m_thread = new QThread(this);
    m_worker = new QObject;
    m_worker->moveToThread(m_thread);
    m_thread->start();
    m_inFlight = 0;
    m_closing = false;
    m_running = true;

    const std::shared_ptr<FfmpegH264Writer> writer = m_writer;
    const QSize size = m_size;
    QMetaObject::invokeMethod(m_worker, [this, writer, path, size] {
        const bool ok = writer->open(path, size, kFramesPerSecond);
        const QString error = writer->error();
        QMetaObject::invokeMethod(this, [this, ok, error] {
            if (!ok)
                fail(tr("Das Video konnte nicht geschrieben werden: %1").arg(error));
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
#else
    Q_UNUSED(path);
#endif
}

void VideoEncoder::sendToWriter(Frame frame)
{
#ifdef NSL_HAVE_FFMPEG_ENCODER
    ++m_inFlight;
    ++m_written;
    const std::shared_ptr<FfmpegH264Writer> writer = m_writer;
    QMetaObject::invokeMethod(m_worker, [this, writer, frame = std::move(frame)] {
        const bool ok = writer->write(frame.image, frame.timestamp);
        const QString error = writer->error();
        QMetaObject::invokeMethod(this, [this, ok, error] {
            --m_inFlight;
            if (!ok) {
                fail(tr("Das Video konnte nicht geschrieben werden: %1").arg(error));
                return;
            }
            pumpWriter();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
#else
    Q_UNUSED(frame);
#endif
}

void VideoEncoder::pumpWriter()
{
#ifdef NSL_HAVE_FFMPEG_ENCODER
    if (!m_running)
        return;
    bool sent = false;
    while (!m_queue.empty() && m_inFlight < kMaxInFlight) {
        Frame next = std::move(m_queue.front());
        m_queue.pop_front();
        sendToWriter(std::move(next));
        sent = true;
    }
    if (sent)
        emit frameSent();
    // All written: complete the file, then finished().
    if (m_finishing && !m_closing && m_queue.empty() && m_inFlight == 0) {
        m_closing = true;
        const std::shared_ptr<FfmpegH264Writer> writer = m_writer;
        QMetaObject::invokeMethod(m_worker, [this, writer] {
            const bool ok = writer->close();
            const QString error = writer->error();
            QMetaObject::invokeMethod(this, [this, ok, error] {
                stopThread();
                if (!ok) {
                    fail(tr("Das Video konnte nicht geschrieben werden: %1").arg(error));
                    return;
                }
                m_running = false;
                emit finished();
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    }
#endif
}

void VideoEncoder::stopThread()
{
    if (!m_thread)
        return;
    // Waits for the work queued so far; the writer goes with the last reference.
    m_thread->quit();
    m_thread->wait();
    delete m_worker; // its thread has ended: safe to delete from here
    m_worker = nullptr;
    delete m_thread;
    m_thread = nullptr;
    m_writer.reset();
}
