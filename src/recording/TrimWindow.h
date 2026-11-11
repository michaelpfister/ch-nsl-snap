#pragma once

#include <QImage>
#include <QMediaFormat>
#include <QWidget>

class QLabel;
class QMediaPlayer;
class QProgressBar;
class QPushButton;
class QToolButton;
class QVideoSink;
class TrimTimeline;
class VideoPreview;
class VideoTrimmer;

// The trim window after a recording (F-51, D-039): a preview with play and
// pause, the timeline with the start and end handles, and three ways out:
// "Speichern" keeps the chosen part, "Ganz speichern" the whole recording,
// "Verwerfen" none. The window does not save into the save folder itself:
// saved() hands over the finished file (the recording, or the trimmed part
// in the temporary folder) and the main window moves it there (F-41).
//
// The recording stays in the temporary folder while the window is open
// and is deleted when it closes, unless saved() handed it over.
class TrimWindow : public QWidget
{
    Q_OBJECT

public:
    // `recording`: the temporary MP4 file; `title` names it in the title bar.
    TrimWindow(const QString &recording, const QString &title, QWidget *parent = nullptr);
    ~TrimWindow() override;

    // For the tests, as VideoEncoder::setVideoCodec().
    void setVideoCodec(QMediaFormat::VideoCodec codec) { m_codec = codec; }

    TrimTimeline *timeline() const { return m_timeline; }
    bool isLoaded() const { return m_loaded; }
    bool isPlaying() const;
    QImage currentFrame() const;

    // The buttons' actions, also for the tests.
    void saveRange();
    void saveWhole();
    void discard();
    void togglePlay();

signals:
    // The video to keep; the receiver moves it away.
    void saved(const QString &path);
    void discarded();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void onLoaded();
    void onPositionChanged(qint64 ms);
    void showFrame(int frame);
    void updateLabels();
    void setBusy(bool busy);
    void finishWith(QString path); // a copy: it may be one of the fields it clears

    QString m_recording;
    QString m_trimmed; // the part, once written
    QMediaFormat::VideoCodec m_codec = QMediaFormat::VideoCodec::H264;
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_sink = nullptr;
    VideoTrimmer *m_trimmer = nullptr;
    VideoPreview *m_preview = nullptr;
    TrimTimeline *m_timeline = nullptr;
    QToolButton *m_playButton = nullptr;
    QLabel *m_time = nullptr;
    QLabel *m_range = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_discardButton = nullptr;
    QPushButton *m_wholeButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    bool m_loaded = false;
    bool m_done = false; // saved or discarded
};
