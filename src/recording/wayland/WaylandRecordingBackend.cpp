#include "recording/wayland/WaylandRecordingBackend.h"

#include "recording/wayland/WaylandStream.h"

#include <QGuiApplication>

WaylandRecordingBackend::WaylandRecordingBackend(QObject *parent)
    : RecordingBackend(parent)
    , m_picker(new WaylandBackend(this))
    , m_stream(new WaylandStream(this))
{
    connect(m_picker, &WaylandBackend::videoPicked, this, &WaylandRecordingBackend::onPicked);
    connect(m_picker, &CaptureBackend::cancelled, this, &RecordingBackend::cancelled);
    connect(m_picker, &CaptureBackend::failed, this, &RecordingBackend::failed);
    connect(m_stream, &WaylandStream::frameReady, this, &RecordingBackend::frameReady);
    connect(m_stream, &WaylandStream::ended, this, [this] {
        emit streamEnded(m_pick.window ? tr("Das Fenster ist nicht mehr da; die Aufnahme wurde beendet.")
                                       : tr("Der Bildschirm liefert keine Bilder mehr; die Aufnahme wurde beendet."));
    });
}

WaylandRecordingBackend::~WaylandRecordingBackend() = default;

bool WaylandRecordingBackend::isAvailable() const
{
    return m_picker->isAvailable();
}

QString WaylandRecordingBackend::unavailableReason() const
{
    return m_picker->unavailableReason();
}

bool WaylandRecordingBackend::supportsWindow() const
{
    return m_picker->canPickWindowsForVideo();
}

void WaylandRecordingBackend::pick(Source source)
{
    if (source == Source::Window && !supportsWindow()) {
        emit failed(tr("Einzelne Fenster lassen sich als Video nur unter Hyprland aufnehmen. Ein ganzer "
                       "Bildschirm geht auch hier."));
        return;
    }
    m_picker->pickForVideo(source == Source::Window);
}

void WaylandRecordingBackend::onPicked(const WaylandBackend::VideoPick &pick)
{
    m_pick = pick;
    if (pick.window)
        emit picked(Source::Window, pick.title, pick.size);
    else
        emit picked(Source::Screen, tr("Bildschirm"), pick.size);
}

void WaylandRecordingBackend::startStream()
{
    const bool started = m_pick.window ? m_stream->startWindow(m_pick.handle, m_pick.margins)
                                       : m_stream->startScreens(m_pick.screens, m_pick.native);
    if (!started)
        emit streamEnded(tr("Die Aufnahme liess sich nicht starten: der Desktop bietet die nötigen "
                            "Wayland-Protokolle nicht mehr an."));
}

void WaylandRecordingBackend::requestFrame()
{
    m_stream->requestFrame();
}

void WaylandRecordingBackend::stopStream()
{
    m_stream->stop();
}

QScreen *WaylandRecordingBackend::indicatorScreen() const
{
    // A window recording never contains the indicator (only the window's
    // own surface is copied): next to it, on the primary monitor. A
    // monitor recording: a monitor that is not recorded, if there is one
    // (Q-31).
    if (m_pick.window)
        return nullptr;
    for (QScreen *screen : QGuiApplication::screens()) {
        if (!m_pick.screens.contains(screen))
            return screen;
    }
    return m_pick.screens.isEmpty() ? nullptr : m_pick.screens.first();
}
