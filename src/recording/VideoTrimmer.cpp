#include "recording/VideoTrimmer.h"

#include "recording/VideoEncoder.h"
#include "recording/VideoFrames.h"

#include <QFile>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>

namespace {

// The player pauses with this many frames waiting at the encoder and goes
// on again below kResumeAt; a full-HD frame is 8 MB.
constexpr int kHoldBackAt = 24;
constexpr int kResumeAt = 6;

} // namespace

VideoTrimmer::VideoTrimmer(QObject *parent)
    : QObject(parent)
{
}

VideoTrimmer::~VideoTrimmer()
{
    cleanUp();
}

void VideoTrimmer::start(const QString &input, const QString &output, qint64 startMs, qint64 endMs)
{
    if (m_running)
        return;
    m_running = true;
    m_ending = false;
    m_heldBack = false;
    m_output = output;
    m_startUs = startMs * 1000;
    m_endUs = endMs * 1000;
    m_lastUs = -1;
    m_size = QSize();
    m_percent = -1;

    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoTrimmer::onFrame);
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &message) { fail(message); });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia && m_player->playbackState() == QMediaPlayer::StoppedState) {
            // The player starts at the frame that holds the position and
            // skips the ones before (tried with Qt 6.8.3).
            m_player->setPosition(m_startUs / 1000);
            m_player->setPlaybackRate(kSpeed);
            m_player->play();
        } else if (status == QMediaPlayer::EndOfMedia) {
            endOfPart();
        } else if (status == QMediaPlayer::InvalidMedia) {
            fail(tr("Die Aufnahme lässt sich nicht lesen."));
        }
    });
    m_player->setSource(QUrl::fromLocalFile(input));
}

void VideoTrimmer::onFrame(const QVideoFrame &frame)
{
    if (!m_running || m_ending || !frame.isValid() || m_player->playbackState() != QMediaPlayer::PlayingState)
        return;
    const qint64 start = frame.startTime();
    // Before the part, or the same frame again after a pause.
    if (start < m_startUs || start <= m_lastUs)
        return;
    if (start >= m_endUs) {
        endOfPart();
        return;
    }
    QImage image = frame.toImage();
    if (image.isNull())
        return;
    if (!m_encoder) {
        // The size of the recording (already even, VideoFrames::evenSize).
        m_size = VideoFrames::evenSize(image.size());
        m_encoder = new VideoEncoder(this);
        m_encoder->setVideoCodec(m_codec);
        m_encoder->setKeepAllFrames(true);
        connect(m_encoder, &VideoEncoder::frameSent, this, &VideoTrimmer::onEncoderFrameSent);
        connect(m_encoder, &VideoEncoder::finished, this, [this] {
            cleanUp();
            emit progress(100);
            emit finished();
        });
        connect(m_encoder, &VideoEncoder::failed, this, &VideoTrimmer::fail);
        m_encoder->start(m_output, m_size);
    }
    m_lastUs = start;
    m_encoder->addFrame(VideoFrames::fit(image, m_size), start - m_startUs);

    const int percent = int((start - m_startUs) * 100 / std::max<qint64>(1, m_endUs - m_startUs));
    if (percent != m_percent) {
        m_percent = percent;
        emit progress(percent);
    }
    if (m_encoder->queuedFrames() >= kHoldBackAt && !m_heldBack) {
        m_heldBack = true;
        m_player->pause();
    }
}

void VideoTrimmer::onEncoderFrameSent()
{
    if (m_heldBack && m_encoder->queuedFrames() < kResumeAt && !m_ending) {
        m_heldBack = false;
        m_player->play();
    }
}

void VideoTrimmer::endOfPart()
{
    if (!m_running || m_ending)
        return;
    m_ending = true;
    m_player->stop();
    if (!m_encoder) {
        fail(tr("Im gewählten Ausschnitt ist kein Bild."));
        return;
    }
    m_encoder->finish();
}

void VideoTrimmer::cancel()
{
    if (!m_running)
        return;
    cleanUp();
    QFile::remove(m_output);
}

void VideoTrimmer::fail(const QString &message)
{
    if (!m_running)
        return;
    cleanUp();
    QFile::remove(m_output);
    emit failed(message.isEmpty() ? tr("Der Ausschnitt konnte nicht geschrieben werden.") : message);
}

void VideoTrimmer::cleanUp()
{
    m_running = false;
    if (m_player) {
        m_player->disconnect(this);
        m_player->stop();
        m_player->deleteLater();
        m_player = nullptr;
    }
    if (m_sink) {
        m_sink->disconnect(this);
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    if (m_encoder) {
        m_encoder->disconnect(this);
        m_encoder->deleteLater();
        m_encoder = nullptr;
    }
}
