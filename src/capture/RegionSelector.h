#pragma once

#include <QPoint>
#include <QRect>

// The region selection of the capture overlay (F-03, D-023), without UI.
// Two ways, chosen in the settings:
// - Clicks (default): a first click sets one corner, the region follows
//   the pointer, a second click sets the opposite corner. A drag works too.
// - Drag: press, drag, release. A plain click starts nothing.
// Points are whole pixels of the frozen picture (physical pixels); both
// corner pixels belong to the region.
class RegionSelector
{
public:
    enum class Mode { Clicks, Drag };
    enum class State { Idle, Pressed, Started, Dragging, Done };

    explicit RegionSelector(Mode mode = Mode::Clicks) : m_mode(mode) {}
    Mode mode() const { return m_mode; }

    // A press moved farther than this (pixels) is a drag, not a click.
    static constexpr int kDragThreshold = 4;
    // A region must be at least this large; a second click closer to the
    // first one is ignored (an accidental double click).
    static constexpr int kMinSize = 2;

    State state() const { return m_state; }
    bool hasStart() const { return m_state != State::Idle; }
    QPoint start() const { return m_start; }
    QPoint current() const { return m_current; }

    // The region from the first corner to the current point, both pixels
    // included; empty before the first click.
    QRect region() const;

    void press(const QPoint &point);
    void move(const QPoint &point);
    void release(const QPoint &point);
    // Enter or Space at the current point: the same as a click.
    void click(const QPoint &point);
    // Backspace / Delete: forget the first corner (F-47).
    void discard();

private:
    bool finish(const QPoint &point);

    Mode m_mode;
    State m_state = State::Idle;
    QPoint m_start;
    QPoint m_current;
    QPoint m_pressedAt;
};
