#include "capture/RegionSelector.h"

#include <algorithm>
#include <cstdlib>

QRect RegionSelector::region() const
{
    if (m_state == State::Idle)
        return {};
    return QRect(QPoint(std::min(m_start.x(), m_current.x()), std::min(m_start.y(), m_current.y())),
                 QPoint(std::max(m_start.x(), m_current.x()), std::max(m_start.y(), m_current.y())));
}

bool RegionSelector::finish(const QPoint &point)
{
    m_current = point;
    const QRect r = region();
    if (r.width() < kMinSize || r.height() < kMinSize)
        return false;
    m_state = State::Done;
    return true;
}

void RegionSelector::press(const QPoint &point)
{
    switch (m_state) {
    case State::Idle:
        // Click or drag: decided by the next moves.
        m_state = State::Pressed;
        m_start = m_current = m_pressedAt = point;
        break;
    case State::Started:
        // The second click closes the region (or is ignored if too small).
        finish(point);
        break;
    case State::Pressed:
    case State::Dragging:
    case State::Done:
        break;
    }
}

void RegionSelector::move(const QPoint &point)
{
    if (m_state == State::Done)
        return;
    m_current = point;
    if (m_state == State::Pressed
        && (std::abs(point.x() - m_pressedAt.x()) > kDragThreshold
            || std::abs(point.y() - m_pressedAt.y()) > kDragThreshold))
        m_state = State::Dragging;
}

void RegionSelector::release(const QPoint &point)
{
    switch (m_state) {
    case State::Pressed:
        if (m_mode == Mode::Drag) {
            m_state = State::Idle; // a click starts nothing when dragging is the way
            break;
        }
        // A click: the first corner is set, the region follows the pointer.
        m_state = State::Started;
        m_current = point;
        break;
    case State::Dragging:
        if (!finish(point))
            m_state = m_mode == Mode::Clicks ? State::Started : State::Idle; // too small
        break;
    case State::Idle:
    case State::Started:
    case State::Done:
        break;
    }
}

void RegionSelector::click(const QPoint &point)
{
    // From the keyboard (Enter, Space) both modes work with two "clicks":
    // a drag cannot be done with keys.
    if (m_state == State::Idle) {
        m_state = State::Started;
        m_start = m_current = point;
    } else if (m_state == State::Started) {
        finish(point);
    }
}

void RegionSelector::discard()
{
    if (m_state != State::Done)
        m_state = State::Idle;
}
