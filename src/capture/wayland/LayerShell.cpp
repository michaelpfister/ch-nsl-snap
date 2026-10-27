#include "capture/wayland/LayerShell.h"

#include "capture/wayland/WaylandConnection.h"

#include <QScreen>
#include <QWidget>
#include <QWindow>

#ifdef NSL_HAVE_LAYER_SHELL_QT
#include <LayerShellQt/window.h>
#endif

namespace LayerShell {

namespace {

#ifdef NSL_HAVE_LAYER_SHELL_QT
LayerShellQt::Window *layerFor(QWidget *widget, QScreen *screen)
{
    if (!isAvailable())
        return nullptr;
    if (screen)
        widget->setScreen(screen);
    // Creates the native window, so there is a QWindow to configure; the
    // layer surface itself is created when the widget is shown.
    widget->winId();
    auto *layer = LayerShellQt::Window::get(widget->windowHandle());
    if (!layer)
        return nullptr;
    layer->setScope(QStringLiteral("nsl-snap"));
    layer->setLayer(LayerShellQt::Window::LayerOverlay);
    // On the monitor of the widget, not where the compositor would put it.
#ifdef NSL_LAYER_SHELL_QT_SET_SCREEN
    layer->setWantsToBeOnActiveScreen(false);
    layer->setScreen(widget->screen());
#else
    layer->setScreenConfiguration(LayerShellQt::Window::ScreenFromQWindow);
#endif
    // -1: do not move aside for panels; cover them.
    layer->setExclusiveZone(-1);
    return layer;
}
#endif

} // namespace

bool isAvailable()
{
#ifdef NSL_HAVE_LAYER_SHELL_QT
    WaylandConnection *connection = WaylandConnection::instance();
    return connection && connection->hasGlobal("zwlr_layer_shell_v1");
#else
    return false;
#endif
}

bool makeOverlay(QWidget *widget, QScreen *screen)
{
#ifdef NSL_HAVE_LAYER_SHELL_QT
    LayerShellQt::Window *layer = layerFor(widget, screen);
    if (!layer)
        return false;
    // Anchored to all four edges: the compositor makes it as large as the monitor.
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                    | LayerShellQt::Window::AnchorLeft
                                                    | LayerShellQt::Window::AnchorRight));
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    return false;
#endif
}

bool makeNotification(QWidget *widget, QScreen *screen)
{
#ifdef NSL_HAVE_LAYER_SHELL_QT
    LayerShellQt::Window *layer = layerFor(widget, screen);
    if (!layer)
        return false;
    // No anchor: centred on the monitor, at the widget's size.
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorNone));
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    return false;
#endif
}

} // namespace LayerShell
