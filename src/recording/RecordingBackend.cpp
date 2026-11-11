#include "recording/RecordingBackend.h"

#include <QCoreApplication>

#ifdef NSL_HAVE_WAYLAND_BACKEND
#include "capture/wayland/WaylandBackend.h"
#include "recording/wayland/WaylandRecordingBackend.h"
#endif
#ifdef NSL_HAVE_WINDOWS_BACKEND
#include "recording/windows/WindowsRecordingBackend.h"
#endif

namespace {

// Platforms whose video backend is not written yet (D-039: GNOME, the
// portal and macOS follow).
class UnsupportedRecordingBackend : public RecordingBackend
{
public:
    using RecordingBackend::RecordingBackend;

    bool isAvailable() const override { return false; }
    QString unavailableReason() const override
    {
        // No Q_OBJECT here, so the context is named for lupdate (D-038).
        return QCoreApplication::translate("UnsupportedRecordingBackend",
                                           "Videos lassen sich auf diesem Desktop noch nicht aufnehmen. Bisher "
                                           "gehen Hyprland, andere wlroots-Desktops und Windows; GNOME folgt.");
    }
    bool supportsWindow() const override { return false; }
    void pick(Source) override { emit failed(unavailableReason()); }
    void startStream() override {}
    void requestFrame() override {}
    void stopStream() override {}
};

#ifdef NSL_HAVE_WAYLAND_BACKEND
// GNOME keeps its own path (Q-27), even though Mutter offers no
// wlr-screencopy anyway.
bool onGnome()
{
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'));
    return desktops.contains(QLatin1String("GNOME"), Qt::CaseInsensitive);
}
#endif

} // namespace

std::unique_ptr<RecordingBackend> createRecordingBackend(QObject *parent)
{
#if defined(NSL_HAVE_WINDOWS_BACKEND)
    return std::make_unique<WindowsRecordingBackend>(parent);
#else
#ifdef NSL_HAVE_WAYLAND_BACKEND
    if (!onGnome() && WaylandBackend::isSupported())
        return std::make_unique<WaylandRecordingBackend>(parent);
#endif
    return std::make_unique<UnsupportedRecordingBackend>(parent);
#endif
}
