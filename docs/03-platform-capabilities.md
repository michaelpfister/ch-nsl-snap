# Platform capabilities

What each platform lets an application do, and the way we use. "Extension" means our GNOME Shell extension (D-003).

## Capture

macOS (ScreenCaptureKit, macOS 14+) supports everything in the Windows column except scrolling capture, after the user has granted the "Screen Recording" permission (D-002).

| Capability | Windows | GNOME / Wayland | Hyprland (D-031) | Other wlroots (Sway, ...) | Other Wayland (KDE, ...) |
|---|---|---|---|---|---|
| Full screen, one monitor | Windows API | Extension or Screenshot portal | wlr-screencopy | wlr-screencopy | Screenshot portal (D-032) |
| Free area | Freeze full screen, select in overlay | Same, via extension or portal image | Same, app's own overlay | Same, app's own overlay | Same, from the portal picture |
| Single window, clean edges | DWM extended frame bounds + Windows.Graphics.Capture (D-022) | **Extension** (offscreen render of the window actor) | **hyprland-toplevel-export** + window list from the control socket | not possible (no window list for apps) | not possible (KWin ScreenShot2 only for KDE's own tools) |
| Single window, fallback | — | ScreenCast portal, window source (extra dialog every time) | — | ScreenCast portal (dialog every time, not used) | Same |
| Window under mouse (hover highlight) | yes | **Extension only** | yes, from the window list | no | no |
| Transparent rounded corners | Windows 11: yes (DWM) | yes, the client draws them with alpha | client-drawn corners yes; Hyprland's own rounding is expected not to be in the buffer (Q-21, Q-24) | client-drawn only | — |
| Hidden or overlapped window | yes (capture API) | yes, offscreen render ignores overlaps | yes, the export renders the window alone | — | — |
| Menus / tooltips of a window | yes | not in the PoC (they are separate surfaces) | no (separate surfaces) | — | — |
| Scrolling capture | possible (input injection) | practically impossible (no input injection) | practically impossible | practically impossible | practically impossible |
| Mouse cursor in the image | yes | yes (extension can composite it) | yes (protocol option, not used) | yes | portal option |
| Overlay above all windows | top-most window | extension draws it | wlr-layer-shell via LayerShellQt, else full screen | same | full screen |

## System integration

| Capability | Windows | GNOME / Wayland | Hyprland |
|---|---|---|---|
| Global hotkey (e.g. Print), F-42 | `RegisterHotKey`; Windows 11 binds Print to the Snipping Tool by default (Q-15) | Extension keybinding; GNOME binds Print to its own screenshot UI (Q-14) | `bind` in hyprland.conf running the app (not built yet) |
| Tray icon | yes | Only with the AppIndicator extension (installed on Zorin by default) | with a bar that has a tray (Waybar) |
| Clipboard with PNG | yes | yes (Wayland clipboard, also from the extension via `St.Clipboard`) | yes (Wayland clipboard) |
| Start with session | Autostart / registry | XDG autostart `.desktop` file | `exec-once` in hyprland.conf |

## Known limits on Hyprland and other wlroots desktops

- Wayland itself forbids nothing; each compositor decides what it offers apps. Hyprland offers more than GNOME (window contents and a window list), Sway only monitor pictures.
- Hyprland's permission system (`ecosystem:enforce_permissions`, off by default) asks before the first capture or needs a `permission = <binary>, screencopy, allow` rule; on NixOS the binary path is in the Nix store.
- No stacking order in Hyprland's window list; the app approximates it (D-031).
- Without LayerShellQt the overlay is a full-screen window; compositors may animate it.
- Global keys: apps get none; the countdown is cancelled by clicking it, not with Escape.

## Known limits on GNOME / Wayland

- Everything window-related depends on the extension. Without it, only the portal paths remain.
- The JavaScript API of GNOME Shell changes between major versions. The extension declares the versions it was tested with in `metadata.json`.
- Fractional scaling: the window texture has the window's resource scale. Crop coordinates are converted from logical to texture pixels (see the PoC).
