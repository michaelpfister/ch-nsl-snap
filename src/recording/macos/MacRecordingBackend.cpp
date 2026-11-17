#include "recording/macos/MacRecordingBackend.h"

#include "capture/macos/MacCapture.h"

#include <QPainter>
#include <QtDebug>

namespace {

// How long ScreenCaptureKit may take to find the window or monitor.
constexpr int kStartTimeoutMs = 5000;

} // namespace

MacRecordingBackend::MacRecordingBackend(QObject *parent)
    : RecordingBackend(parent)
    , m_picker(new MacBackend(this))
{
    connect(m_picker, &MacBackend::videoPicked, this, &MacRecordingBackend::onPicked);
    connect(m_picker, &CaptureBackend::cancelled, this, &RecordingBackend::cancelled);
    connect(m_picker, &CaptureBackend::failed, this, &RecordingBackend::failed);
}

MacRecordingBackend::~MacRecordingBackend() = default;

void MacRecordingBackend::pick(Source source)
{
    m_picker->pickForVideo(source == Source::Window);
}

void MacRecordingBackend::onPicked(const MacBackend::VideoPick &pick)
{
    m_pick = pick;
    if (pick.windowId)
        emit picked(Source::Window, pick.title, pick.size);
    else
        emit picked(Source::Screen, tr("Bildschirm"), pick.size);
}

void MacRecordingBackend::startStream()
{
    stopStream();
    m_ended = false;
    QString diagnostic;
    if (m_pick.windowId) {
        m_window = MacCapture::Stream::forWindow(m_pick.windowId, kStartTimeoutMs, &diagnostic);
    } else {
        for (QScreen *screen : std::as_const(m_pick.screens)) {
            auto stream = MacCapture::Stream::forScreen(screen, kStartTimeoutMs, &diagnostic);
            if (!stream) {
                m_monitors.clear();
                break;
            }
            m_monitors.push_back(std::move(stream));
            m_monitorImages.append(QImage());
        }
    }
    if (!m_window && m_monitors.empty()) {
        qWarning("Stream failed: %s", qPrintable(diagnostic));
        end(tr("Die Aufnahme liess sich nicht starten: ScreenCaptureKit verweigert sie."));
    }
}

void MacRecordingBackend::requestFrame()
{
    if (m_ended)
        return;
    QImage image;
    if (m_window) {
        if (m_window->hasStopped()) {
            end(tr("Das Fenster ist nicht mehr da; die Aufnahme wurde beendet."));
            return;
        }
        // macOS sends frames only when the window changes; without a new
        // one the controller shows the last one again.
        if (m_window->takeLatest(image))
            emit frameReady(image);
        return;
    }

    bool complete = true;
    for (std::size_t i = 0; i < m_monitors.size(); ++i) {
        if (m_monitors[i]->hasStopped()) {
            end(tr("Die Aufnahme des Bildschirms wurde von macOS beendet."));
            return;
        }
        if (m_monitors[i]->takeLatest(image))
            m_monitorImages[qsizetype(i)] = image;
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
    for (const QRect &native : std::as_const(m_pick.natives))
        area = area.united(native);
    QImage frame(area.size(), QImage::Format_RGB32);
    frame.fill(Qt::black);
    QPainter painter(&frame);
    for (qsizetype i = 0; i < m_monitorImages.size(); ++i)
        painter.drawImage(QRect(m_pick.natives[i].topLeft() - area.topLeft(), m_pick.natives[i].size()),
                          m_monitorImages[i]);
    painter.end();
    emit frameReady(frame);
}

void MacRecordingBackend::stopStream()
{
    m_window.reset();
    m_monitors.clear();
    m_monitorImages.clear();
}

void MacRecordingBackend::end(const QString &reason)
{
    if (m_ended)
        return;
    m_ended = true;
    stopStream();
    emit streamEnded(reason);
}
