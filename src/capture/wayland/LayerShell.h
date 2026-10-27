#pragma once

class QScreen;
class QWidget;

// Places the app's own overlay windows on Wayland (D-031). Wayland lets no
// app position its windows or keep them above others; a tiling compositor
// such as Hyprland would even tile the overlay next to the other windows.
// The wlr-layer-shell protocol is the way out: a "layer surface" sits on a
// layer above all windows, covers a whole monitor if asked and may take
// the keyboard. Qt itself does not speak it; KDE's LayerShellQt library
// does, and the app uses it when it was built with it
// (NSL_HAVE_LAYER_SHELL_QT). Without it the overlay is shown full screen.
namespace LayerShell {

// Built with LayerShellQt, on Wayland, and the compositor offers layer-shell.
bool isAvailable();

// Makes `widget` a layer surface above all windows covering `screen`,
// with the keyboard. Call before the widget is shown for the first time;
// false (and nothing changed) where layer-shell is not available.
bool makeOverlay(QWidget *widget, QScreen *screen);

// Makes `widget` a layer surface above all windows in the middle of
// `screen` that never takes the keyboard (the countdown, F-04).
bool makeNotification(QWidget *widget, QScreen *screen);

} // namespace LayerShell
