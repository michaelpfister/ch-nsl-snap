#include "capture/wayland/WaylandConnection.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QGuiApplication>
// Qt's native interface for Wayland: its wl_display.
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <poll.h>

#include <algorithm>
#include <cerrno>

namespace {

WaylandConnection *s_instance = nullptr;
bool s_quitting = false;

} // namespace

WaylandConnection *WaylandConnection::instance()
{
    if (s_instance || s_quitting)
        return s_instance;
    if (QGuiApplication::platformName() != QLatin1String("wayland"))
        return nullptr;
    auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!native || !native->display())
        return nullptr;
    s_instance = new WaylandConnection(native->display());
    // Our objects must go before Qt closes the connection.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
        s_quitting = true;
        delete s_instance;
        s_instance = nullptr;
    });
    return s_instance;
}

WaylandConnection::WaylandConnection(wl_display *display)
    : m_display(display)
    , m_queue(wl_display_create_queue(display))
{
    // A "wrapper" of the display that sends its new objects to our queue
    // instead of Qt's; the registry and everything bound from it follow.
    auto *wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), m_queue);
    m_registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);

    // libwayland calls plain functions; `this` comes back as `data`.
    static const wl_registry_listener listener = {&WaylandConnection::onGlobal, &WaylandConnection::onGlobalRemove};
    wl_registry_add_listener(m_registry, &listener, this);
    // The compositor answers with one "global" event per protocol.
    roundtrip();
}

WaylandConnection::~WaylandConnection()
{
    if (m_registry)
        wl_registry_destroy(m_registry);
    if (m_queue)
        wl_event_queue_destroy(m_queue);
}

void WaylandConnection::onGlobal(void *data, wl_registry *, uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandConnection *>(data);
    self->m_globals.append({name, QByteArray(interface), version});
}

void WaylandConnection::onGlobalRemove(void *data, wl_registry *, uint32_t name)
{
    auto *self = static_cast<WaylandConnection *>(data);
    self->m_globals.removeIf([name](const Global &global) { return global.name == name; });
}

bool WaylandConnection::hasGlobal(const char *interface) const
{
    for (const Global &global : m_globals) {
        if (global.interface == interface)
            return true;
    }
    return false;
}

void *WaylandConnection::bind(const wl_interface *interface, uint32_t maxVersion, uint32_t *version)
{
    for (const Global &global : std::as_const(m_globals)) {
        if (global.interface != interface->name)
            continue;
        const uint32_t bound = std::min(global.version, maxVersion);
        if (version)
            *version = bound;
        return wl_registry_bind(m_registry, global.name, interface, bound);
    }
    return nullptr;
}

bool WaylandConnection::roundtrip()
{
    return wl_display_roundtrip_queue(m_display, m_queue) >= 0;
}

bool WaylandConnection::dispatchUntil(const std::function<bool()> &done, int timeoutMs)
{
    // The usual way to wait on a queue while another thread (Qt's) reads
    // from the same connection: announce the read, wait until the socket
    // has data, then read. libwayland sorts every event into its queue,
    // whichever thread reads it.
    const QDeadlineTimer deadline(timeoutMs);
    while (!done()) {
        while (wl_display_prepare_read_queue(m_display, m_queue) != 0) {
            if (wl_display_dispatch_queue_pending(m_display, m_queue) < 0)
                return false;
            if (done())
                return true;
        }
        if (wl_display_flush(m_display) < 0 && errno != EAGAIN) {
            wl_display_cancel_read(m_display);
            return false;
        }
        const int left = int(std::max<qint64>(0, deadline.remainingTime()));
        pollfd fd = {wl_display_get_fd(m_display), POLLIN, 0};
        // Short slices: another thread may have read our events meanwhile.
        const int ready = poll(&fd, 1, std::min(left, 50));
        if (ready > 0) {
            if (wl_display_read_events(m_display) < 0)
                return false;
        } else {
            wl_display_cancel_read(m_display);
        }
        if (wl_display_dispatch_queue_pending(m_display, m_queue) < 0)
            return false;
        if (!done() && deadline.hasExpired())
            return false;
    }
    return true;
}
