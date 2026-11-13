#include "recording/RecordingBackend.h"

#include <QCoreApplication>

#ifdef NSL_HAVE_WAYLAND_BACKEND
#include "capture/wayland/WaylandBackend.h"
#include "recording/wayland/WaylandRecordingBackend.h"
#endif
#ifdef NSL_HAVE_WINDOWS_BACKEND
#include "recording/windows/WindowsRecordingBackend.h"
#endif
#ifdef NSL_HAVE_PIPEWIRE
#include "recording/gnome/GnomeRecordingBackend.h"
#endif
#ifdef NSL_HAVE_MACOS_BACKEND
#include "recording/macos/MacRecordingBackend.h"
#endif

namespace {

// Platforms whose video backend is not written yet (D-039: the portal
// follows), or a build without what it needs.
class UnsupportedRecordingBackend : public RecordingBackend
{
public:
    explicit UnsupportedRecordingBackend(const QString &reason, QObject *parent = nullptr)
        : RecordingBackend(parent)
        , m_reason(reason)
    {
    }

    bool isAvailable() const override { return false; }
    QString unavailableReason() const override { return m_reason; }
    bool supportsWindow() const override { return false; }
    void pick(Source) override { emit failed(unavailableReason()); }
    void startStream() override {}
    void requestFrame() override {}
    void stopStream() override {}

private:
    QString m_reason;
};

// No Q_OBJECT in this file, so the context is named for lupdate (D-038).
QString notYet()
{
    return QCoreApplication::translate("UnsupportedRecordingBackend",
                                       "Videos lassen sich auf diesem Desktop noch nicht aufnehmen. Bisher gehen "
                                       "GNOME, Hyprland, andere wlroots-Desktops, Windows und macOS.");
}

#ifdef NSL_HAVE_GNOME_BACKEND
// GNOME, also under another name (Zorin sets "zorin:GNOME").
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
#elif defined(NSL_HAVE_MACOS_BACKEND)
    return std::make_unique<MacRecordingBackend>(parent);
#else
#ifdef NSL_HAVE_GNOME_BACKEND
    if (onGnome()) {
#ifdef NSL_HAVE_PIPEWIRE
        return std::make_unique<GnomeRecordingBackend>(parent);
#else
        return std::make_unique<UnsupportedRecordingBackend>(
            QCoreApplication::translate("UnsupportedRecordingBackend",
                                        "Diese Snapslash-Version wurde ohne PipeWire gebaut; unter GNOME gibt es "
                                        "damit keine Videos."),
            parent);
#endif
    }
#endif
#ifdef NSL_HAVE_WAYLAND_BACKEND
    if (WaylandBackend::isSupported())
        return std::make_unique<WaylandRecordingBackend>(parent);
#endif
    return std::make_unique<UnsupportedRecordingBackend>(notYet(), parent);
#endif
}
