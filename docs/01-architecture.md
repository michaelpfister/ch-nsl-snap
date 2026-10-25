# Architecture

## Purpose

A screenshot tool for daily work on Windows, macOS and Linux with GNOME (Wayland). The reference is WinSnap, whose strongest feature is capturing **one window** cleanly: nothing from neighbouring windows or the desktop, clean edges and transparent rounded corners. No shadow is added: the instruction pages add it via CSS (D-004). Captures can be annotated, copied to the clipboard and saved with a naming scheme.

The development machine is Zorin OS with GNOME Shell 46 on Wayland.

## Guiding principle: capture is platform-specific, everything else is shared

Capturing is the only part that differs fundamentally between platforms. Wayland in particular does not let normal applications see other windows (see [03-platform-capabilities.md](03-platform-capabilities.md)). The app is therefore split into a **capture backend** per platform and a **shared core** that never knows where an image came from.

| Concern | Windows | macOS | GNOME (Wayland) | Fallback (any Wayland desktop) |
|---|---|---|---|---|
| Window capture | Windows API (DWM/Graphics Capture) | ScreenCaptureKit | NSL Snap Shell extension (D-Bus) | ScreenCast portal, window source |
| Full screen / area | Windows API | ScreenCaptureKit | Shell extension, or Screenshot portal | Screenshot portal |
| Window list / geometry | Windows API | ScreenCaptureKit (`SCShareableContent`) | Shell extension | not available |

The app is C++20 with Qt 6.8 Widgets (D-013).

## Components

```
                ┌──────────────────────── NSL Snap app (shared core) ───────────────────────┐
 user ─button─▶ │ Capture service ──▶ Crop & clean (D-009, D-010) ──────────▶ Editor ──▶ Export│
                │       │                                                    (annotate)  (file, clipboard)
                │       ▼                                                                    │
                │  CaptureBackend interface                                                   │
                └───────┬───────────────────────────────┬──────────────────────┬─────────────┘
                        │                               │                      │
                 Windows backend               GNOME backend            Portal backend
               (Win32 / DWM APIs)        D-Bus ─▶ Shell extension    xdg-desktop-portal
                                              (runs inside           (Screenshot, ScreenCast
                                               gnome-shell)           via PipeWire)
```

### Capture backends

Each backend delivers a **capture result**: a PNG/RGBA image with alpha, plus metadata (window title, application id, geometry in logical pixels, scale factor). Shadows are never part of the image, neither from the system nor from the app (D-004).

### GNOME Shell extension

- A small extension (`gnome-extension/nsl-snap@netslash.ch/`) runs inside GNOME Shell. Only code there can enumerate windows and read their contents on Wayland.
- It owns the D-Bus name `ch.netslash.Snap.Shell` and exports `ch.netslash.Snap1` at `/ch/netslash/Snap`.
- Every capture method is **interactive**: the user must pick the window with a click. No caller can silently grab a window's contents (D-003).
- The capture uses the same path as GNOME's own screenshot UI: `MetaWindowActor.paint_to_content()` renders the window offscreen into a texture with alpha. Overlapping windows therefore do not matter. The extension can crop the texture to the window's frame rectangle (D-005), but the app requests the whole buffer and crops to the window's opaque core itself (D-010).
- Planned (D-011): freeze the screen and all windows at the moment of capture (after the countdown, if a delay is set), then one overlay to select window, region or screen on the frozen picture. A D-Bus caller can only open the overlay.
- The extension stays as thin as possible: pick, capture, return PNG bytes. All logic belongs in the app, because extension code has to be adapted to new GNOME versions.

### Shared core

Not built yet. Planned: annotation editor, export (file with naming scheme, clipboard), settings, tray icon. Feature scope in [04-features.md](04-features.md). The technology is open (Q-01).

The editor is object-based (D-012): the captured image stays unchanged, annotations are objects with properties, every change is an undoable command, pixels are only computed on render and export.

## Repository layout

| Path | Contents |
|---|---|
| `docs/` | This documentation |
| `gnome-extension/nsl-snap@netslash.ch/` | GNOME Shell extension (capture backend for GNOME) |
| `tools/nsl-snap-pick` | PoC command-line client for the extension |
| `scripts/` | Development helpers (install the extension, nested test shell) |
