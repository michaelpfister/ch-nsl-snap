#include "recording/TrimTimeline.h"

#include "recording/VideoEncoder.h"
#include "ui/Theme.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kHandleWidth = 12;
constexpr int kTrackHeight = 40;
constexpr int kGrabDistance = 10; // px around a handle that still grab it

} // namespace

TrimTimeline::TrimTimeline(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(kTrackHeight + 16);
    setAccessibleName(tr("Zeitleiste: Anfang und Ende des Videos"));
    setCursor(Qt::PointingHandCursor);
}

QSize TrimTimeline::sizeHint() const
{
    return QSize(600, kTrackHeight + 16);
}

qint64 TrimTimeline::frameToMs(int frame)
{
    return qint64(frame) * 1000 / VideoEncoder::kFramesPerSecond;
}

int TrimTimeline::msToFrame(qint64 ms)
{
    return int(std::llround(double(ms) * VideoEncoder::kFramesPerSecond / 1000.0));
}

void TrimTimeline::setFrameCount(int frames)
{
    m_frames = std::max(1, frames);
    m_start = 0;
    m_end = m_frames;
    m_position = 0;
    update();
    emit rangeChanged(m_start, m_end);
}

void TrimTimeline::setRange(int start, int end)
{
    start = std::clamp(start, 0, m_frames - 1);
    end = std::clamp(end, start + 1, m_frames);
    if (start == m_start && end == m_end)
        return;
    m_start = start;
    m_end = end;
    update();
    emit rangeChanged(m_start, m_end);
}

void TrimTimeline::selectHandle(Handle handle)
{
    m_selected = handle;
    update();
}

void TrimTimeline::setPosition(int frame)
{
    frame = std::clamp(frame, 0, m_frames);
    if (frame == m_position)
        return;
    m_position = frame;
    update();
}

QRectF TrimTimeline::track() const
{
    // Room for half a handle on each side.
    return QRectF(kHandleWidth / 2.0, (height() - kTrackHeight) / 2.0, width() - kHandleWidth, kTrackHeight);
}

double TrimTimeline::xOf(int frame) const
{
    const QRectF bar = track();
    return bar.left() + bar.width() * frame / m_frames;
}

int TrimTimeline::frameAt(double x) const
{
    const QRectF bar = track();
    return std::clamp(int(std::lround((x - bar.left()) / bar.width() * m_frames)), 0, m_frames);
}

void TrimTimeline::moveHandle(Handle handle, int frame)
{
    if (handle == Handle::Start)
        setRange(std::min(frame, m_end - 1), m_end);
    else
        setRange(m_start, std::max(frame, m_start + 1));
    // The end handle shows the last frame that is kept.
    emit handleMoved(handle == Handle::Start ? m_start : m_end - 1);
}

void TrimTimeline::paintEvent(QPaintEvent *)
{
    const Theme theme = Theme::current();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bar = track();

    // The whole recording, the kept part brighter with the accent frame.
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme.hover);
    painter.drawRoundedRect(bar, 8, 8);
    const QRectF kept(QPointF(xOf(m_start), bar.top()), QPointF(xOf(m_end), bar.bottom()));
    painter.setBrush(theme.soft);
    painter.drawRect(kept);
    painter.setPen(QPen(theme.accent, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(kept.topLeft(), kept.topRight());
    painter.drawLine(kept.bottomLeft(), kept.bottomRight());

    // The handles: the selected one filled, the other outlined.
    for (const Handle handle : {Handle::Start, Handle::End}) {
        const double x = xOf(handle == Handle::Start ? m_start : m_end);
        const QRectF grip(x - kHandleWidth / 2.0, bar.top() - 4, kHandleWidth, bar.height() + 8);
        const bool selected = handle == m_selected;
        painter.setPen(QPen(theme.accent, 2));
        painter.setBrush(selected ? theme.accent : theme.panel);
        painter.drawRoundedRect(grip, 4, 4);
        painter.setPen(QPen(selected ? QColor(Qt::white) : theme.accent, 1.5));
        painter.drawLine(QPointF(x, grip.top() + 12), QPointF(x, grip.bottom() - 12));
    }

    // The current position.
    const double x = xOf(m_position);
    painter.setPen(QPen(theme.text, 2));
    painter.drawLine(QPointF(x, bar.top() - 6), QPointF(x, bar.bottom() + 6));

    if (hasFocus()) {
        painter.setPen(QPen(theme.accent, 1, Qt::DotLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 6, 6);
    }
}

void TrimTimeline::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;
    const double x = event->position().x();
    const double startDistance = std::abs(x - xOf(m_start));
    const double endDistance = std::abs(x - xOf(m_end));
    if (std::min(startDistance, endDistance) <= kHandleWidth / 2.0 + kGrabDistance) {
        // The nearer handle; on top of each other the end one when right of it.
        const Handle handle = endDistance < startDistance || (endDistance == startDistance && x > xOf(m_end))
            ? Handle::End
            : Handle::Start;
        selectHandle(handle);
        m_dragging = true;
        return;
    }
    emit positionRequested(std::min(frameAt(x), m_frames - 1));
}

void TrimTimeline::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging)
        moveHandle(m_selected, frameAt(event->position().x()));
}

void TrimTimeline::mouseReleaseEvent(QMouseEvent *)
{
    m_dragging = false;
}

void TrimTimeline::keyPressEvent(QKeyEvent *event)
{
    const int step = (event->modifiers() & Qt::ShiftModifier) ? VideoEncoder::kFramesPerSecond : 1;
    const int current = m_selected == Handle::Start ? m_start : m_end;
    switch (event->key()) {
    case Qt::Key_Left: moveHandle(m_selected, current - step); return;
    case Qt::Key_Right: moveHandle(m_selected, current + step); return;
    case Qt::Key_Up:
    case Qt::Key_Down:
        selectHandle(m_selected == Handle::Start ? Handle::End : Handle::Start);
        emit handleMoved(m_selected == Handle::Start ? m_start : m_end - 1);
        return;
    default: QWidget::keyPressEvent(event);
    }
}
