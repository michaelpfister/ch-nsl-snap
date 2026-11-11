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
    layer->setScope(QStringLiteral("snapslash"));
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

bool makeNotification(QWidget *widget, QScreen *screen, CountdownPosition position, int margin)
{
#ifdef NSL_HAVE_LAYER_SHELL_QT
    LayerShellQt::Window *layer = layerFor(widget, screen);
    if (!layer)
        return false;
    // No anchor: centred on the monitor, at the widget's size. A corner:
    // anchored to its two edges, `margin` away from them.
    using Window = LayerShellQt::Window;
    Window::Anchors anchors = Window::Anchors(Window::AnchorNone);
    switch (position) {
    case CountdownPosition::TopLeft: anchors = Window::Anchors(Window::AnchorTop | Window::AnchorLeft); break;
    case CountdownPosition::TopRight: anchors = Window::Anchors(Window::AnchorTop | Window::AnchorRight); break;
    case CountdownPosition::BottomLeft: anchors = Window::Anchors(Window::AnchorBottom | Window::AnchorLeft); break;
    case CountdownPosition::BottomRight: anchors = Window::Anchors(Window::AnchorBottom | Window::AnchorRight); break;
    case CountdownPosition::Center: break;
    }
    layer->setAnchors(anchors);
    if (position != CountdownPosition::Center) {
        layer->setMargins(QMargins(margin, margin, margin, margin));
        // 0: keep clear of panels, like the recording indicator.
        layer->setExclusiveZone(0);
    }
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    Q_UNUSED(position);
    Q_UNUSED(margin);
    return false;
#endif
}

bool makeCornerNotification(QWidget *widget, QScreen *screen, int margin)
{
#ifdef NSL_HAVE_LAYER_SHELL_QT
    LayerShellQt::Window *layer = layerFor(widget, screen);
    if (!layer)
        return false;
    layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorRight));
    layer->setMargins(QMargins(0, margin, margin, 0));
    // 0: keep clear of panels (unlike the overlay, which covers them).
    layer->setExclusiveZone(0);
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    return true;
#else
    Q_UNUSED(widget);
    Q_UNUSED(screen);
    Q_UNUSED(margin);
    return false;
#endif
}

} // namespace LayerShell
