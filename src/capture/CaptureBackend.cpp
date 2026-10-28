#include "capture/CaptureBackend.h"

#ifdef NSL_HAVE_GNOME_BACKEND
#include "capture/gnome/GnomeShellBackend.h"
#endif
#ifdef NSL_HAVE_WAYLAND_BACKEND
#include "capture/portal/PortalBackend.h"
#include "capture/wayland/WaylandBackend.h"
#endif
#ifdef NSL_HAVE_WINDOWS_BACKEND
#include "capture/windows/WindowsBackend.h"
#endif

namespace {

// Used on platforms whose backend is not written yet (macOS).
class UnsupportedBackend : public CaptureBackend
{
public:
    using CaptureBackend::CaptureBackend;

    bool isAvailable() const override { return false; }
    QString unavailableReason() const override
    {
        return tr("Aufnehmen wird auf dieser Plattform noch nicht unterstützt.");
    }
    void captureWindow() override { emit failed(unavailableReason()); }
    void captureRegion() override { emit failed(unavailableReason()); }
    void captureScreen() override { emit failed(unavailableReason()); }
    bool supportsRegion() const override { return false; }
};

#ifdef NSL_HAVE_GNOME_BACKEND
// GNOME, also under another name (Zorin sets "zorin:GNOME"); on GNOME the
// app keeps its own backend even while the extension is not running, so it
// can say what is missing (D-003).
bool onGnome(const GnomeShellBackend &gnome)
{
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'));
    return desktops.contains(QLatin1String("GNOME"), Qt::CaseInsensitive) || gnome.isAvailable();
}
#endif

} // namespace

std::unique_ptr<CaptureBackend> createCaptureBackend(QObject *parent)
{
#if defined(NSL_HAVE_GNOME_BACKEND)
    // Linux: chosen at start, by what the desktop offers (D-031, D-032).
    auto gnome = std::make_unique<GnomeShellBackend>(parent);
    if (onGnome(*gnome))
        return gnome;
#ifdef NSL_HAVE_WAYLAND_BACKEND
    if (WaylandBackend::isSupported())
        return std::make_unique<WaylandBackend>(parent);
    return std::make_unique<PortalBackend>(parent);
#else
    return gnome;
#endif
#elif defined(NSL_HAVE_WINDOWS_BACKEND)
    return std::make_unique<WindowsBackend>(parent);
#else
    return std::make_unique<UnsupportedBackend>(parent);
#endif
}
