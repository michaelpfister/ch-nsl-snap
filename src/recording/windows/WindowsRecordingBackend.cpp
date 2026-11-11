#include "recording/windows/WindowsRecordingBackend.h"

#include "capture/PickGeometry.h"

#include <QPainter>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

QRect toQRect(const WgcCapture::Rect &r)
{
    return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

// BGRA with premultiplied alpha is Qt's ARGB32_Premultiplied on
// little-endian machines (all Windows machines). Wraps the bytes without
// copying: the caller copies.
QImage wrap(const WgcCapture::Pixels &pixels)
{
    return QImage(pixels.bgra.data(), pixels.width, pixels.height, pixels.width * 4,
                  QImage::Format_ARGB32_Premultiplied);
}

} // namespace

WindowsRecordingBackend::WindowsRecordingBackend(QObject *parent)
    : RecordingBackend(parent)
    , m_picker(new WindowsBackend(this))
{
    connect(m_picker, &WindowsBackend::videoPicked, this, &WindowsRecordingBackend::onPicked);
    connect(m_picker, &CaptureBackend::cancelled, this, &RecordingBackend::cancelled);
    connect(m_picker, &CaptureBackend::failed, this, &RecordingBackend::failed);
}

WindowsRecordingBackend::~WindowsRecordingBackend() = default;

bool WindowsRecordingBackend::isAvailable() const
{
    return m_picker->isAvailable();
}

QString WindowsRecordingBackend::unavailableReason() const
{
    return m_picker->unavailableReason();
}

void WindowsRecordingBackend::pick(Source source)
{
    if (m_preset) {
        const WindowsBackend::VideoPick preset = *m_preset;
        QMetaObject::invokeMethod(this, [this, preset] { onPicked(preset); }, Qt::QueuedConnection);
        return;
    }
    m_picker->pickForVideo(source == Source::Window);
}

void WindowsRecordingBackend::onPicked(const WindowsBackend::VideoPick &pick)
{
    m_pick = pick;
    if (pick.hwnd)
        emit picked(Source::Window, pick.title, pick.size);
    else
        emit picked(Source::Screen, tr("Bildschirm"), pick.size);
}

void WindowsRecordingBackend::startStream()
{
    stopStream();
    m_ended = false;
    if (m_pick.hwnd) {
        m_window = WgcCapture::Stream::forWindow(m_pick.hwnd);
    } else {
        for (const QRect &monitor : std::as_const(m_pick.monitors)) {
            const QPoint centre = monitor.center();
            auto stream = WgcCapture::Stream::forMonitorAt(centre.x(), centre.y());
            if (!stream) {
                m_monitors.clear();
                break;
            }
            m_monitors.push_back(std::move(stream));
            m_monitorImages.append(QImage());
        }
    }
    if (!m_window && m_monitors.empty())
        end(tr("Die Aufnahme liess sich nicht starten: Windows.Graphics.Capture verweigert sie."));
}

void WindowsRecordingBackend::requestFrame()
{
    if (m_ended)
        return;
    if (m_window) {
        if (m_window->isClosed() || !IsWindow(m_pick.hwnd)) {
            end(tr("Das Fenster ist nicht mehr da; die Aufnahme wurde beendet."));
            return;
        }
        // Windows sends frames only when the window changes; without a new
        // one the controller shows the last one again.
        if (!m_window->takeLatest(m_pixels))
            return;
        WgcCapture::Rect windowRect, frame;
        const QImage whole = wrap(m_pixels);
        // The visible frame, as for a window screenshot: the capture also
        // holds the invisible resize borders.
        QRect part = whole.rect();
        if (WgcCapture::windowRects(m_pick.hwnd, windowRect, frame))
            part = PickGeometry::frameInCapture(whole.size(), toQRect(windowRect), toQRect(frame));
        emit frameReady(whole.copy(part));
        return;
    }

    bool complete = true;
    for (std::size_t i = 0; i < m_monitors.size(); ++i) {
        if (m_monitors[i]->takeLatest(m_pixels))
            m_monitorImages[qsizetype(i)] = wrap(m_pixels).copy();
        complete = complete && !m_monitorImages[qsizetype(i)].isNull();
    }
    if (!complete)
        return;
    if (m_monitorImages.size() == 1) {
        emit frameReady(m_monitorImages.first());
        return;
    }
    // All monitors side by side, as in the overlay; gaps stay black.
    QRect area;
    for (const QRect &monitor : std::as_const(m_pick.monitors))
        area = area.united(monitor);
    QImage frame(area.size(), QImage::Format_RGB32);
    frame.fill(Qt::black);
    QPainter painter(&frame);
    for (qsizetype i = 0; i < m_monitorImages.size(); ++i)
        painter.drawImage(m_pick.monitors[i].topLeft() - area.topLeft(), m_monitorImages[i]);
    painter.end();
    emit frameReady(frame);
}

void WindowsRecordingBackend::stopStream()
{
    m_window.reset();
    m_monitors.clear();
    m_monitorImages.clear();
}

void WindowsRecordingBackend::end(const QString &reason)
{
    if (m_ended)
        return;
    m_ended = true;
    stopStream();
    emit streamEnded(reason);
}
