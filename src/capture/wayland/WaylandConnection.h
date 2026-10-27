#pragma once

#include <QByteArray>
#include <QList>

#include <cstdint>
#include <functional>

struct wl_display;
struct wl_event_queue;
struct wl_interface;
struct wl_registry;

// The app's own view of the Wayland compositor (D-031), next to Qt's.
//
// Qt already has a connection to the compositor (wl_display). We reuse it,
// but read our events from a separate event queue, so our objects never
// mix with Qt's and we can wait for the compositor without running Qt's
// event loop. The registry on that queue lists the compositor's "globals":
// the protocols it offers, such as wlr-screencopy or layer-shell.
class WaylandConnection
{
public:
    // The connection, or nullptr when the app does not run on Wayland.
    // Created on first use; lives until the app quits.
    static WaylandConnection *instance();

    ~WaylandConnection();

    wl_display *display() const { return m_display; }

    // Whether the compositor offers a protocol, e.g. "zwlr_layer_shell_v1".
    bool hasGlobal(const char *interface) const;

    // Binds a global on our queue, at the lower of its version and
    // `maxVersion`; nullptr if the compositor does not offer it. Each call
    // binds anew; the caller owns the result. `version` receives the bound
    // version.
    void *bind(const wl_interface *interface, uint32_t maxVersion, uint32_t *version = nullptr);

    // Dispatches our queue until `done` returns true, or until `timeoutMs`
    // have passed (then false). Blocks the calling thread, so keep waits
    // short: the compositor answers within a frame or two.
    bool dispatchUntil(const std::function<bool()> &done, int timeoutMs);

    // Sends the requests made so far and waits until the compositor has
    // handled them; false if the connection broke.
    bool roundtrip();

private:
    struct Global
    {
        uint32_t name = 0;
        QByteArray interface;
        uint32_t version = 0;
    };

    explicit WaylandConnection(wl_display *display);
    static void onGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void onGlobalRemove(void *data, wl_registry *registry, uint32_t name);

    wl_display *m_display = nullptr;
    wl_event_queue *m_queue = nullptr;
    wl_registry *m_registry = nullptr;
    QList<Global> m_globals;
};
