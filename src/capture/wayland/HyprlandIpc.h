#pragma once

#include <QByteArray>
#include <QList>
#include <QRect>
#include <QString>

#include <cstdint>

// Hyprland's control socket (the one `hyprctl` uses, D-031). Wayland gives
// normal apps no list of windows; Hyprland's socket does, with each
// window's position, size and monitor in logical pixels.
namespace HyprlandIpc {

// True when the app runs inside Hyprland (HYPRLAND_INSTANCE_SIGNATURE).
bool isRunning();

// Sends a command such as "j/clients" (j: answer in JSON) and returns the
// answer; empty if the socket is not there or does not answer in time.
QByteArray request(const QByteArray &command);

struct Window
{
    uint32_t handle = 0;  // for hyprland-toplevel-export: lower 32 bits of the address
    QRect rect;           // logical pixels, desktop coordinates, without Hyprland's border
    QString monitor;      // the monitor's name, e.g. "DP-1"
    QString title;
    QString appClass;     // e.g. "org.gnome.Nautilus"
};

// The windows the user can see right now, topmost first: those on each
// monitor's active workspace (and its open special workspace), mapped and
// not hidden. Hyprland reports no stacking order; it is approximated: an
// open special workspace above everything, then pinned, full-screen,
// floating and tiled windows, each group by how recently it had the focus.
QList<Window> visibleWindows(const QByteArray &clientsJson, const QByteArray &monitorsJson);

} // namespace HyprlandIpc
