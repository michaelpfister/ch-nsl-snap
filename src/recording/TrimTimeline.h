#pragma once

#include <QWidget>

// The timeline of the trim window (F-51): the whole recording as a bar,
// two handles for the start and the end of the part to keep, and the
// current position. Everything is counted in frames of 1/30 s, so a handle
// always sits on a frame.
//
// Mouse: drag a handle, or click elsewhere to move the position. Keys:
// Left/Right move the selected handle by one frame, with Shift by one
// second; Up/Down switch between the handles.
class TrimTimeline : public QWidget
{
    Q_OBJECT

public:
    enum class Handle { Start, End };

    explicit TrimTimeline(QWidget *parent = nullptr);

    // The length of the recording in frames; resets both handles.
    void setFrameCount(int frames);
    int frameCount() const { return m_frames; }

    // The part to keep: frames [start, end). At least one frame.
    int startFrame() const { return m_start; }
    int endFrame() const { return m_end; }
    void setRange(int start, int end);
    bool isWhole() const { return m_start == 0 && m_end == m_frames; }

    Handle selectedHandle() const { return m_selected; }
    void selectHandle(Handle handle);

    int position() const { return m_position; }
    void setPosition(int frame);

    // Frames to milliseconds and back (30 fps).
    static qint64 frameToMs(int frame);
    static int msToFrame(qint64 ms);

    QSize sizeHint() const override;

signals:
    void rangeChanged(int start, int end);
    // A handle was moved (drag or key): show this frame.
    void handleMoved(int frame);
    // A click on the bar: go to this frame.
    void positionRequested(int frame);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QRectF track() const;
    double xOf(int frame) const;
    int frameAt(double x) const;
    void moveHandle(Handle handle, int frame);

    int m_frames = 1;
    int m_start = 0;
    int m_end = 1;
    int m_position = 0;
    Handle m_selected = Handle::Start;
    bool m_dragging = false;
};
