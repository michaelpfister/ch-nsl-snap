#include "capture/CaptureBackend.h"

#ifdef NSL_HAVE_GNOME_BACKEND
#include "capture/gnome/GnomeShellBackend.h"
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

} // namespace

std::unique_ptr<CaptureBackend> createCaptureBackend(QObject *parent)
{
#if defined(NSL_HAVE_GNOME_BACKEND)
    return std::make_unique<GnomeShellBackend>(parent);
#elif defined(NSL_HAVE_WINDOWS_BACKEND)
    return std::make_unique<WindowsBackend>(parent);
#else
    return std::make_unique<UnsupportedBackend>(parent);
#endif
}
