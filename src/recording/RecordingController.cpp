#include "recording/RecordingController.h"

#include "capture/Countdown.h"
#include "recording/RecordingIndicator.h"
#include "recording/VideoEncoder.h"
#include "recording/VideoFrames.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <algorithm>

RecordingController::RecordingController(RecordingBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
    // A frame every 33 ms; precise, so the frames keep an even pace.
    m_ticker.setTimerType(Qt::PreciseTimer);
    m_ticker.setInterval(1000 / VideoEncoder::kFramesPerSecond);
    connect(&m_ticker, &QTimer::timeout, this, &RecordingController::tick);

    connect(m_backend, &RecordingBackend::picked, this, &RecordingController::onPicked);
    connect(m_backend, &RecordingBackend::cancelled, this, [this] {
        if (m_state != State::Picking)
            return;
        reset();
        emit cancelled();
    });
    connect(m_backend, &RecordingBackend::failed, this, [this](const QString &message) {
        if (m_state != State::Picking)
            return;
        reset();
        emit failed(message);
    });
    connect(m_backend, &RecordingBackend::frameReady, this, &RecordingController::onFrame);
    connect(m_backend, &RecordingBackend::streamEnded, this, [this](const QString &reason) {
        if (m_state != State::Recording)
            return;
        m_endReason = reason;
        stopRecording();
    });
}

RecordingController::~RecordingController()
{
    if (m_state == State::Recording)
        m_backend->stopStream();
    delete m_countdown;
    delete m_indicator;
}

bool RecordingController::isAvailable() const
{
    return whyNot().isEmpty();
}

QString RecordingController::whyNot() const
{
    if (!m_backend->isAvailable())
        return m_backend->unavailableReason();
    if (m_codec == QMediaFormat::VideoCodec::H264 && !VideoEncoder::canEncodeH264())
        return tr("Auf diesem Rechner findet Qt Multimedia keinen H.264-Encoder. Unter Linux braucht es dafür "
                  "die Hardware-Kodierung der Grafikkarte (VA-API oder NVENC).");
    return {};
}

void RecordingController::start(RecordingBackend::Source source, int delay)
{
    if (m_state != State::Idle)
        return;
    if (const QString reason = whyNot(); !reason.isEmpty()) {
        emit failed(reason);
        return;
    }
    m_delay = delay;
    m_state = State::Picking;
    m_backend->pick(source);
}

void RecordingController::onPicked(RecordingBackend::Source, const QString &title, const QSize &size)
{
    if (m_state != State::Picking)
        return;
    m_title = title;
    m_size = VideoFrames::evenSize(size);
    m_state = State::CountingDown;

    // After the pick, so the user can get the window ready (D-039).
    m_countdown = new Countdown;
    m_countdown->setTickInterval(m_countdownTick);
    connect(m_countdown, &Countdown::finished, this, [this] {
        m_countdown->deleteLater();
        startRecording();
    });
    connect(m_countdown, &Countdown::cancelled, this, [this] {
        m_countdown->deleteLater();
        reset();
        emit cancelled();
    });
    m_countdown->start(std::max(kMinCountdownSeconds, m_delay));
}

QString RecordingController::temporaryPath() const
{
    const QString folder =
        m_folder.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::TempLocation) : m_folder;
    QDir().mkpath(folder);
    const QString name = QStringLiteral("snapslash-video-%1-%2.mp4")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")))
                             .arg(QCoreApplication::applicationPid());
    return QDir(folder).filePath(name);
}

void RecordingController::startRecording()
{
    if (m_state != State::CountingDown)
        return;
    m_state = State::Recording;
    m_path = temporaryPath();
    m_latest = QImage();
    m_lastIndex = -1;
    m_durationMs = 0;
    m_limitReached = false;
    m_endReason.clear();

    m_encoder = new VideoEncoder(this);
    m_encoder->setVideoCodec(m_codec);
    connect(m_encoder, &VideoEncoder::finished, this, &RecordingController::onEncoderFinished);
    connect(m_encoder, &VideoEncoder::failed, this, &RecordingController::onEncoderFailed);
    m_encoder->start(m_path, m_size);
    if (m_state != State::Recording)
        return; // the encoder failed at once (onEncoderFailed)

    // Visible the whole time (D-003, D-039).
    m_indicator = new RecordingIndicator;
    connect(m_indicator, &RecordingIndicator::stopClicked, this, &RecordingController::stop);
    m_indicator->showOn(m_backend->indicatorScreen());

    m_clock.start();
    m_backend->startStream();
    if (m_state != State::Recording)
        return; // the stream could not start (streamEnded)
    m_backend->requestFrame();
    m_ticker.start();
    emit recordingStarted();
}

qint64 RecordingController::elapsedMs() const
{
    if (m_state == State::Recording)
        return m_clock.elapsed();
    return m_durationMs;
}

void RecordingController::tick()
{
    if (m_state != State::Recording)
        return;
    const qint64 elapsedUs = m_clock.nsecsElapsed() / 1000;
    m_indicator->setElapsed(elapsedUs / 1000, m_maxDurationMs);
    if (elapsedUs / 1000 >= m_maxDurationMs) {
        m_limitReached = true;
        stopRecording();
        return;
    }
    // The frame number this moment belongs to; a late tick skips numbers,
    // and the encoder then shows the frame before a little longer.
    const qint64 index = elapsedUs * VideoEncoder::kFramesPerSecond / 1000000;
    if (!m_latest.isNull() && index > m_lastIndex) {
        m_encoder->addFrame(m_latest, index * 1000000 / VideoEncoder::kFramesPerSecond);
        m_lastIndex = index;
    }
    m_backend->requestFrame();
}

void RecordingController::onFrame(const QImage &frame)
{
    if (m_state != State::Recording)
        return;
    m_latest = VideoFrames::fit(frame, m_size);
    // The first frame starts the video at 0, without waiting for a tick.
    if (m_lastIndex < 0 && !m_latest.isNull()) {
        m_encoder->addFrame(m_latest, 0);
        m_lastIndex = 0;
    }
}

void RecordingController::stop()
{
    switch (m_state) {
    case State::CountingDown:
        if (m_countdown)
            m_countdown->cancel();
        break;
    case State::Recording:
        stopRecording();
        break;
    case State::Idle:
    case State::Picking:
    case State::Finishing:
        break;
    }
}

void RecordingController::stopRecording()
{
    if (m_state != State::Recording)
        return;
    m_state = State::Finishing;
    m_durationMs = m_clock.elapsed();
    m_ticker.stop();
    m_backend->stopStream();
    if (m_indicator) {
        m_indicator->deleteLater();
        m_indicator = nullptr;
    }
    if (m_encoder->framesWritten() == 0 && m_latest.isNull()) {
        // Not a single frame came: nothing worth keeping.
        const QString reason = m_endReason;
        delete m_encoder;
        m_encoder = nullptr;
        QFile::remove(m_path);
        reset();
        emit failed(reason.isEmpty() ? tr("Es kam kein Bild vom Bildschirm an; das Video ist leer.") : reason);
        return;
    }
    m_encoder->finish();
}

void RecordingController::onEncoderFinished()
{
    if (m_state != State::Finishing)
        return;
    Result result;
    result.path = m_path;
    result.title = m_title;
    result.durationMs = m_durationMs;
    result.limitReached = m_limitReached;
    result.endReason = m_endReason;
    m_encoder->deleteLater();
    m_encoder = nullptr;
    reset();
    emit finished(result);
}

void RecordingController::onEncoderFailed(const QString &message)
{
    if (m_state != State::Recording && m_state != State::Finishing)
        return;
    if (m_state == State::Recording) {
        m_ticker.stop();
        m_backend->stopStream();
    }
    m_encoder->deleteLater();
    m_encoder = nullptr;
    QFile::remove(m_path);
    reset();
    emit failed(tr("Das Video konnte nicht geschrieben werden: %1").arg(message));
}

void RecordingController::reset()
{
    m_state = State::Idle;
    m_ticker.stop();
    if (m_indicator) {
        m_indicator->deleteLater();
        m_indicator = nullptr;
    }
    m_latest = QImage();
    m_lastIndex = -1;
}
