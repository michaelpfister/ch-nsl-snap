#include "recording/VideoEncoder.h"

#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoFrameInput>

namespace {

// How long one frame is shown, in microseconds.
constexpr qint64 kFrameDuration = 1000000 / VideoEncoder::kFramesPerSecond;

} // namespace

VideoEncoder::VideoEncoder(QObject *parent)
    : QObject(parent)
{
}

VideoEncoder::~VideoEncoder()
{
    // The recorder must stop before the session and input go.
    if (m_recorder && m_recorder->recorderState() != QMediaRecorder::StoppedState)
        m_recorder->stop();
    delete m_recorder;
    delete m_session;
    delete m_input;
}

bool VideoEncoder::canEncodeH264()
{
    QMediaFormat mp4(QMediaFormat::MPEG4); // supportedVideoCodecs() is not const
    return mp4.supportedVideoCodecs(QMediaFormat::Encode).contains(QMediaFormat::VideoCodec::H264);
}

void VideoEncoder::start(const QString &path, const QSize &size)
{
    if (m_running)
        return;
    m_size = size;
    m_written = 0;
    m_pending.reset();
    m_finishing = false;

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
    // Earlier frames still wait: the newest replaces the waiting one.
    if (m_pending || !send(next))
        m_pending = std::move(next);
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
    if (m_pending && send(*m_pending))
        m_pending.reset();
    if (m_finishing && !m_pending && m_recorder->recorderState() == QMediaRecorder::RecordingState)
        m_recorder->stop();
}

void VideoEncoder::finish()
{
    if (!m_running || m_finishing)
        return;
    m_finishing = true;
    // Stops once the waiting frame is in (sendPending).
    if (m_pending && send(*m_pending))
        m_pending.reset();
    if (!m_pending)
        m_recorder->stop();
}

void VideoEncoder::fail(const QString &message)
{
    if (!m_running)
        return;
    m_running = false;
    emit failed(message.isEmpty() ? tr("Das Video konnte nicht geschrieben werden.") : message);
}
