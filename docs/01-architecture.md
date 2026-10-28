# Architecture

## Purpose

A screenshot tool for daily work on Windows, macOS and Linux with GNOME or Hyprland (Wayland). The reference is WinSnap, whose strongest feature is capturing **one window** cleanly: nothing from neighbouring windows or the desktop, clean edges and transparent rounded corners. No shadow is added: the instruction pages add it via CSS (D-004). Captures can be annotated, copied to the clipboard and saved with a naming scheme.

The development machine is Zorin OS with GNOME Shell 46 on Wayland.

## Guiding principle: capture is platform-specific, everything else is shared

Capturing is the only part that differs fundamentally between platforms. Wayland in particular does not let normal applications see other windows (see [03-platform-capabilities.md](03-platform-capabilities.md)). The app is therefore split into a **capture backend** per platform and a **shared core** that never knows where an image came from.

| Concern | Windows | macOS | GNOME (Wayland) | Hyprland and other wlroots (D-031) | Fallback (any Wayland desktop, D-032) |
|---|---|---|---|---|---|
| Window capture | Windows API (DWM/Graphics Capture) | ScreenCaptureKit | NSL Snap Shell extension (D-Bus) | hyprland-toplevel-export (Hyprland only) | not available |
| Full screen / area | Windows API | ScreenCaptureKit | Shell extension, or Screenshot portal | wlr-screencopy | Screenshot portal |
| Window list / geometry | Windows API | ScreenCaptureKit (`SCShareableContent`) | Shell extension | Hyprland's control socket (`hyprctl`) | not available |
| Overlay | the app's own | the app's own | the extension's | the app's own, as a layer surface | the app's own |

On Linux the backend is chosen at start (`createCaptureBackend`): GNOME keeps the extension; otherwise `WaylandBackend` if the compositor offers wlr-screencopy, else `PortalBackend`.

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
                 Windows backend               GNOME backend            Wayland backend           Portal backend
               (Win32 / DWM APIs)        D-Bus ─▶ Shell extension    wlr-screencopy,           xdg-desktop-portal
                                              (runs inside           hyprland-toplevel-export, (Screenshot)
                                               gnome-shell)           Hyprland socket
```

### Capture backends

Each backend delivers a **capture result**: a PNG/RGBA image with alpha, plus metadata (window title, application id, geometry in logical pixels, scale factor). Shadows are never part of the image, neither from the system nor from the app (D-004).

### GNOME Shell extension

- A small extension (`gnome-extension/nsl-snap@netslash.ch/`) runs inside GNOME Shell. Only code there can enumerate windows and read their contents on Wayland.
- It owns the D-Bus name `ch.netslash.Snap.Shell` and exports `ch.netslash.Snap1` at `/ch/netslash/Snap`.
- Every capture method is **interactive**: the user must select a window or a region in the overlay. No caller can silently grab screen contents (D-003).
- The capture uses the same path as GNOME's own screenshot UI: `MetaWindowActor.paint_to_content()` renders the window offscreen into a texture with alpha. Overlapping windows therefore do not matter. The extension can crop the texture to the window's frame rectangle (D-005), but the app requests the whole buffer and crops to the window's opaque core itself (D-010).
- Freeze first (D-011, since 2026-10-25): `Capture` (API version 2; 3 adds `region-select`, D-023; 4 the screen mode, D-024; 5 `delay` with a countdown, D-025) takes a picture of the whole stage (`Shell.Screenshot.screenshot_stage_to_content()`) and renders every pickable window offscreen at the same moment (`overlay.js`, `freeze()`). The overlay then shows the frozen picture; the user picks a window (W) or drags a region (R) on it (D-021). Window captures come from the frozen window render, region captures are cut from the frozen stage picture. `PickWindow` (version 1) remains as `Capture` in window mode for the PoC client.
- Planned (D-011): the global hotkeys (F-42), which the extension will own and hand to the app. A D-Bus caller can only open the overlay.
- The extension stays as thin as possible: pick, capture, return PNG bytes. All logic belongs in the app, because extension code has to be adapted to new GNOME versions.

### Windows backend

- `src/capture/windows/` (D-022). `WgcCapture` (C++/WinRT, no Qt) lists the pickable windows and freezes them: one Windows.Graphics.Capture frame per window, in parallel, kept on the GPU. `src/capture/CaptureOverlay` is the overlay the app draws itself (one window per monitor, frozen picture, window highlight or region with magnifier, D-023; shared with macOS later), driven by `RegionSelector`. `WindowsBackend` ties both together and crops the picked window to its DWM frame.
- The app is per-monitor DPI aware (Qt's default), so Win32 reports physical pixels; `PickGeometry` converts between them and the overlay's logical pixels.

### Wayland backend (Hyprland, Sway & co.) and portal backend

- `src/capture/wayland/` (D-031). `WaylandConnection` uses Qt's Wayland connection with an event queue of its own, so the app can bind the compositor's protocols and wait for their answers without Qt's event loop. `WaylandFreeze` copies every monitor (wlr-screencopy) and every visible window (hyprland-toplevel-export) into shared memory at the same moment. `HyprlandIpc` reads the window list from Hyprland's control socket. `WaylandBackend` ties them together with the shared `CaptureOverlay`, like the Windows backend; `LayerShell` makes the overlay and the countdown layer surfaces when the app is built with LayerShellQt.
- The protocol files are in `protocols/`; CMake runs `wayland-scanner` on them.
- `src/capture/portal/PortalBackend` (D-032): the Screenshot portal's picture as the freeze, region and screen in the same overlay.
- Wayland has only logical coordinates; `PickGeometry::nativeLayout` builds physical "native" ones for the overlay.

### Shared core

Skeleton since 2026-10-25, with the approved design (D-014): top bar with capture modes, timer, undo/redo, copy and save; tool rail; properties panel; status bar; the shortcuts of F-46 that already have a function; editor view with drag & drop and paste; GNOME backend, window crop and corner cleanup in C++ (`src/imaging/WindowImage`, same result as the PoC client, pixel for pixel). Since then: crop (F-12), frames, numbered frames, dim background, pixelate/blur, replace text, free text (F-25), text marker (F-28) and arrows (F-44), and the colour popup with colour wheel, hex code and eyedropper (D-027, `src/ui/ColorWheel`, `src/ui/ColorPopup`, `src/app/ColorRow`). Region capture and window capture on a frozen screen (F-02, F-03, D-011) on GNOME. Window and region capture on Windows (D-022), region by two clicks with a magnifier (D-023), screen capture on GNOME and Windows (D-024), delayed capture with a countdown (D-025). Window, region and screen capture on Hyprland, region and screen on other Wayland desktops (D-031, D-032). Still to come: global hotkeys, the macOS backend, more settings, tray icon. Feature scope in [04-features.md](04-features.md).

The editor is object-based (D-012): the captured image stays unchanged, annotations are objects with properties, every change is an undoable command, pixels are only computed on render and export.

- `EditorScene` (a `QGraphicsScene`) is the document: base image, annotation items, the undo stack, and `renderImage()` for export.
- `EditorView` turns mouse input into commands: drawing with the active tool, moving (one `MoveItemsCommand` per drag). It also owns the zoom (D-017): fit mode that follows resizes, steps, Ctrl + wheel and pinch around the pointer, crisp pixels at high zoom, panning with Space + drag or the middle button.
- Annotation items derive from `RectItem` (rectangle, eight handles, moving, staying inside the image, creation order, label for the object list): `FrameItem` (frames, numbered frames), `RedactItem` (pixelate/blur, D-018) `ReplaceItem` (replaced text, D-019, with `imaging/TextSample` for recognising the original) `MarkerItem` (text marker, D-028), `ArrowItem` (arrow with two end handles, D-028) and `TextItem` (free text, D-026, edited on the canvas through a child `QGraphicsTextItem`; the view turns each edit into one undo step). They paint themselves and push their own resize commands. A numbered frame is a `FrameItem` with a badge position; its number is computed by the scene from the creation order (`FrameItem::order`), so it renumbers itself and survives undo.
- The dimmed background (F-27) is a document setting, not an object: `DimItem` sits between the base image and the annotations and paints a blurred copy of the image (`imaging/Blur`, cached per radius) plus a dark veil, clipped to everything outside the frames. The scene caches the blurred image per radius and the finished layer per setting (D-016). It paints with `SourceAtop`, so transparent window corners stay transparent.
- `Commands.h` holds all undo commands; commands that take items out of the scene own them.
- The properties panel shows the style of the selected frames, or the style for new ones, and pushes `SetFrameStyleCommand`s (slider drags merge into one step).

## Repository layout

| Path | Contents |
|---|---|
| `docs/` | This documentation |
| `CMakeLists.txt`, `src/` | The app (C++20, Qt 6.8, D-013) |
| `src/app/` | Main window, top bar, tool rail, properties panel, shortcuts, settings (`Settings`, `SettingsDialog`, stored with QSettings), the GTK theme check at start (`PlatformTheme`, D-033), the command line (`CommandLine`, F-48) and the hand-over to a running app (`SingleInstance`, D-034) |
| `src/capture/` | `CaptureBackend` interface, `PickGeometry`, and the platform backends (`gnome/`, `windows/`, `wayland/`, `portal/`; `macos/` later) |
| `protocols/` | Wayland protocol files for the Wayland backend |
| `flake.nix`, `nix/` | Nix package and development shell (NixOS) |
| `src/editor/` | Editor view (`QGraphicsView`), later the annotation items (D-012) |
| `src/ui/` | Theme (colour tokens, style sheet, light/dark) and SVG icons (D-014) |
| `resources/icons/` | Line icons, drawn with `currentColor` |
| `src/imaging/` | Image processing without UI: crop to the opaque core (D-010), corner cleanup (D-009) |
| `tests/` | Qt Test unit tests, run with `ctest` |
| `gnome-extension/nsl-snap@netslash.ch/` | GNOME Shell extension (capture backend for GNOME) |
| `tools/nsl-snap-pick` | PoC command-line client for the extension |
| `scripts/` | Development helpers (install the extension, nested test shell) |
