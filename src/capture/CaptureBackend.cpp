#include "capture/CaptureBackend.h"

#ifdef NSL_HAVE_GNOME_BACKEND
#include "capture/gnome/GnomeShellBackend.h"
#endif

namespace {

// Used on platforms whose backend is not written yet (Windows, macOS).
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
};

} // namespace

std::unique_ptr<CaptureBackend> createCaptureBackend(QObject *parent)
{
#ifdef NSL_HAVE_GNOME_BACKEND
    return std::make_unique<GnomeShellBackend>(parent);
#else
    return std::make_unique<UnsupportedBackend>(parent);
#endif
}
