#pragma once

#include "recording/RecordingBackend.h"

#include <QElapsedTimer>
#include <QImage>
#include <QMediaFormat>
#include <QObject>
#include <QPointer>
#include <QTimer>

class Countdown;
class RecordingIndicator;
class VideoEncoder;

// The flow of a video recording (F-50, D-039), the same on every platform:
// pick on the frozen screen (the backend), a countdown of at least 3 s,
// then the live recording at 30 frames per second with the recording
// indicator, until the user stops it or 10 minutes are up. The video goes
// into a temporary MP4 file; finished() hands it over for trimming and
// saving.
class RecordingController : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Picking, CountingDown, Recording, Finishing };

    struct Result
    {
        QString path;       // the temporary MP4 file; the receiver moves or deletes it
        QString title;      // for the file name (F-41)
        qint64 durationMs = 0;
        bool limitReached = false;
        QString endReason;  // not empty: the stream ended by itself (window closed)
    };

    static constexpr int kMinCountdownSeconds = 3;
    static constexpr qint64 kMaxDurationMs = 10 * 60 * 1000;

    explicit RecordingController(RecordingBackend *backend, QObject *parent = nullptr);
    ~RecordingController() override;

    RecordingBackend *backend() const { return m_backend; }
    State state() const { return m_state; }
    bool isBusy() const { return m_state != State::Idle; }
    bool isRecording() const { return m_state == State::Recording; }

    // Whether a video can be recorded here at all; whyNot() says why not.
    bool isAvailable() const;
    QString whyNot() const;

    // Starts with the pick. `delay`: the timer setting (F-04); the
    // countdown is never shorter than kMinCountdownSeconds.
    void start(RecordingBackend::Source source, int delay = 0);
    // Ends the live recording (indicator, tray, hotkey, command line); a
    // countdown that is still running is cancelled.
    void stop();

    // For the tests.
    void setVideoCodec(QMediaFormat::VideoCodec codec) { m_codec = codec; }
    void setMaxDuration(qint64 ms) { m_maxDurationMs = ms; }
    void setCountdownTickInterval(int ms) { m_countdownTick = ms; }
    void setTemporaryFolder(const QString &folder) { m_folder = folder; }
    RecordingIndicator *indicator() const { return m_indicator; }
    qint64 elapsedMs() const;

signals:
    // The live recording runs now (after the countdown).
    void recordingStarted();
    void finished(const RecordingController::Result &result);
    void cancelled();
    void failed(const QString &message);

private:
    void onPicked(RecordingBackend::Source source, const QString &title, const QSize &size);
    void startRecording();
    void tick();
    void onFrame(const QImage &frame);
    void stopRecording();
    void onEncoderFinished();
    void onEncoderFailed(const QString &message);
    void reset();
    QString temporaryPath() const;

    RecordingBackend *m_backend = nullptr;
    State m_state = State::Idle;
    int m_delay = 0;
    QMediaFormat::VideoCodec m_codec = QMediaFormat::VideoCodec::H264;
    qint64 m_maxDurationMs = kMaxDurationMs;
    int m_countdownTick = 1000;
    QString m_folder;

    QString m_title;
    QSize m_size;
    QString m_path;
    QPointer<Countdown> m_countdown;
    RecordingIndicator *m_indicator = nullptr;
    VideoEncoder *m_encoder = nullptr;
    QTimer m_ticker;
    QElapsedTimer m_clock;
    qint64 m_durationMs = 0;
    QImage m_latest;       // the newest frame, already in the video size
    qint64 m_lastIndex = -1; // the frame number last handed to the encoder
    bool m_limitReached = false;
    bool m_empty = false;      // stopped before the first frame
    QString m_endReason;
};

Q_DECLARE_METATYPE(RecordingController::Result)
