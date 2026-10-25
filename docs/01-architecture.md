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
- Planned (D-011): freeze the screen and all windows at the moment of capture (after the countdown, if a delay is set), then one overlay to select window, region or screen on the frozen picture. The extension also owns the global hotkeys (F-42) and hands hotkey captures to the app. A D-Bus caller can only open the overlay.
- The extension stays as thin as possible: pick, capture, return PNG bytes. All logic belongs in the app, because extension code has to be adapted to new GNOME versions.

### Shared core

Skeleton since 2026-10-25, with the approved design (D-014): top bar with capture modes, timer, undo/redo, copy and save; tool rail; properties panel; status bar; the shortcuts of F-46 that already have a function; editor view with drag & drop and paste; GNOME backend, window crop and corner cleanup in C++ (`src/imaging/WindowImage`, same result as the PoC client, pixel for pixel). Still to come: the annotation tools, screen and region capture with freeze (D-011), settings, tray icon. Feature scope in [04-features.md](04-features.md).

The editor is object-based (D-012): the captured image stays unchanged, annotations are objects with properties, every change is an undoable command, pixels are only computed on render and export.

- `EditorScene` (a `QGraphicsScene`) is the document: base image, annotation items, the undo stack, and `renderImage()` for export.
- `EditorView` turns mouse input into commands: drawing with the active tool, moving (one `MoveItemsCommand` per drag). It also owns the zoom (D-017): fit mode that follows resizes, steps, Ctrl + wheel and pinch around the pointer, crisp pixels at high zoom, panning with Space + drag or the middle button.
- Annotation items derive from `RectItem` (rectangle, eight handles, moving, staying inside the image, creation order, label for the object list): `FrameItem` (frames, numbered frames) and `RedactItem` (pixelate/blur, D-018). They paint themselves and push their own resize commands. A numbered frame is a `FrameItem` with a badge position; its number is computed by the scene from the creation order (`FrameItem::order`), so it renumbers itself and survives undo.
- The dimmed background (F-27) is a document setting, not an object: `DimItem` sits between the base image and the annotations and paints a blurred copy of the image (`imaging/Blur`, cached per radius) plus a dark veil, clipped to everything outside the frames. The scene caches the blurred image per radius and the finished layer per setting (D-016). It paints with `SourceAtop`, so transparent window corners stay transparent.
- `Commands.h` holds all undo commands; commands that take items out of the scene own them.
- The properties panel shows the style of the selected frames, or the style for new ones, and pushes `SetFrameStyleCommand`s (slider drags merge into one step).

## Repository layout

| Path | Contents |
|---|---|
| `docs/` | This documentation |
| `CMakeLists.txt`, `src/` | The app (C++20, Qt 6.8, D-013) |
| `src/app/` | Main window, top bar, tool rail, properties panel, shortcuts, settings (`Settings`, `SettingsDialog`, stored with QSettings) |
| `src/capture/` | `CaptureBackend` interface and the platform backends (`gnome/`; `windows/` and `macos/` later) |
| `src/editor/` | Editor view (`QGraphicsView`), later the annotation items (D-012) |
| `src/ui/` | Theme (colour tokens, style sheet, light/dark) and SVG icons (D-014) |
| `resources/icons/` | Line icons, drawn with `currentColor` |
| `src/imaging/` | Image processing without UI: crop to the opaque core (D-010), corner cleanup (D-009) |
| `tests/` | Qt Test unit tests, run with `ctest` |
| `gnome-extension/nsl-snap@netslash.ch/` | GNOME Shell extension (capture backend for GNOME) |
| `tools/nsl-snap-pick` | PoC command-line client for the extension |
| `scripts/` | Development helpers (install the extension, nested test shell) |
