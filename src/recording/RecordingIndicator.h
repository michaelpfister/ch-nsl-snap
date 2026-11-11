#pragma once

#include <QWidget>

class QScreen;

// The recording indicator (F-50, D-039): a small dark box with a red dot,
// the elapsed time and a stop symbol, visible as long as a video is being
// recorded (D-003). A click on it stops the recording. It never takes the
// focus, so the recorded app keeps it.
//
// It should stay out of the video (Q-31): on Windows the system leaves it
// out (WDA_EXCLUDEFROMCAPTURE); on Wayland it sits in the top right corner
// of a monitor that is not recorded where there is one, else of the
// recorded one, where it is part of a monitor recording.
class RecordingIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingIndicator(QWidget *parent = nullptr);

    // Shows it in the top right corner of `screen` (nullptr: the primary).
    void showOn(QScreen *screen);
    void setElapsed(qint64 ms, qint64 maxMs);

    // "1:05" for 65 000 ms; "1:02:03" from an hour on.
    static QString formatTime(qint64 ms);

signals:
    void stopClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    qint64 m_elapsed = 0;
    qint64 m_max = 0;
    bool m_shown = false;
};
