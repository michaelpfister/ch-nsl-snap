# Decisions

Each entry: context, decision, rationale, consequences. Entries marked **(assumption)** were taken provisionally and are listed again in [05-open-questions.md](05-open-questions.md).

---

## D-001 Build our own tool instead of using an existing one

**Context:** On Linux there are capable tools (Flameshot, Ksnip, GNOME's built-in screenshot UI, Spectacle on KDE), but none captures a single window as cleanly and with as many options as WinSnap does on Windows.
**Decision:** Build a new tool, but first prove with a PoC that clean window capture works on GNOME/Wayland (D-002).
**Rejected:**
- GNOME's built-in screenshot UI: has a window mode, but includes the client-side shadow, offers no effects, no annotation, no naming scheme.
- Flameshot/Ksnip: no real window capture on GNOME/Wayland (they only get the full screen through the portal).
**Consequences:** If the feature list later turns out to be close to Flameshot or Ksnip, a fork remains an option for the editor part.

## D-002 Target platforms: Windows, macOS and GNOME on Wayland

**Decision:** Windows 10/11, macOS 14 or later, and GNOME (46 and later) on Wayland are first-class. Other Wayland desktops only get the portal fallback. X11 is not a goal.
**Rationale:** Windows and GNOME are in daily use. macOS was added on 2026-10-25 so that screenshots look the same on all platforms the team uses. KDE would allow more (KWin ScreenShot2), but nobody uses it here.
**Consequences (macOS):**
- Capture via ScreenCaptureKit: windows can be listed and captured individually, also when covered, without system shadow and with transparent corners. No extension needed. macOS 14 is required for `SCScreenshotManager`.
- The user grants the "Screen Recording" permission once; macOS 15 asks again periodically.
- The app must be signed and notarised (Apple Developer account, about 99 USD per year). The permission is tied to the signature, so unsigned builds lose it on every update.
- A Mac for building and testing is available in the office.

## D-003 Window capture on GNOME via our own Shell extension

**Context:** On Wayland, normal applications cannot see other windows. GNOME's own D-Bus screenshot interface (`org.gnome.Shell.Screenshot`) has been restricted to an allowlist since GNOME 41. The ScreenCast portal can stream a single window, but asks the user to pick it in a system dialog every time.
**Decision:** A small GNOME Shell extension does the picking and capturing and offers it over D-Bus.
**Rationale:**
- Only code inside GNOME Shell has access to window geometry and window contents.
- It allows the WinSnap workflow: hover, window is highlighted, click, done.
**Security:** Any process in the user session can call a D-Bus method. Therefore every capture method requires an **interactive pick** by the user, and there is no method that captures a window or lists windows without user interaction. This keeps the protection GNOME introduced in version 41.
**Rejected:**
- `org.gnome.Shell.Screenshot` in "unsafe mode": a debugging hack, resets on restart.
- ScreenCast portal as the main path: works without an extension, but the extra dialog for each capture is too clumsy. It remains the fallback.
**Consequences:**
- The extension must be installed once, and adapted when GNOME changes its JavaScript API (roughly once a year).
- It only works on GNOME.

## D-004 No shadows or backgrounds in the image

**Context:** The screenshots are used on instruction pages (e.g. knowit.unibe.ch), which add a shadow via CSS. A shadow baked into the image would double it.
**Decision:** A window capture is the bare window with an alpha channel: no system shadow, and the app adds no shadow, background or margin either. Transparent rounded corners are kept so the CSS shadow follows the window shape.
**Rationale:** System shadows differ per platform, toolkit and theme, and on Wayland they are part of the client's buffer (GTK). The page decides how the image is presented.
**Rejected:** An app-drawn shadow/background effect (former F-43): not needed, and harmful for the pages' own styling (2026-10-25).

## D-005 Crop to the frame rectangle

**Context:** A window's buffer can be larger than the visible window: GTK and libadwaita draw their shadow into transparent margins around the window (client-side decorations). GNOME's own window screenshot keeps those margins.
**Decision:** The extension crops the captured texture to the window's **frame rectangle** (`MetaWindow.get_frame_rect()`). For comparison, the PoC can also return the uncropped buffer (`crop: "buffer"`).
**Rationale:** The frame rectangle is exactly the visible window. Rounded corners inside it stay transparent because the client draws them with alpha.
**Consequences:** Windows that draw square corners (some Qt or Electron apps) and maximised windows come out square. That is correct, not a bug.

## D-006 The extension returns PNG bytes, it does not write files

**Decision:** `PickWindow` returns the PNG as a byte array over D-Bus. Saving, naming and copying to the clipboard is the app's job.
**Rationale:** Keeps the extension small and free of settings. Even a 4K window as PNG stays far below D-Bus message limits.

## D-007 Identifiers (assumption)

**Decision:** Extension UUID `nsl-snap@netslash.ch`, D-Bus name `ch.netslash.Snap.Shell`, interface `ch.netslash.Snap1`, object path `/ch/netslash/Snap`.
**Consequences:** Renaming the product later (Q-03) does not require changing these, but it is cheaper before the first public release.

## D-008 PoC client in Python

**Decision:** The PoC command-line client `tools/nsl-snap-pick` is written in Python with PyGObject, which is already present on GNOME systems.
**Rationale:** It only exists to test the extension. The technology of the real app is still open (Q-01) and does not have to be decided for the PoC.

## D-009 Clean the corners in the app

**Context:** The client-side shadow lies under the window. Inside the frame rectangle it shows through the transparent corner cut-outs with alpha up to about 25 % (PoC finding 1). Cropping cannot remove it.
**Decision:** After capture, the app clears shadow residue in each corner: starting at the corner pixel, a flood fill makes every connected pixel with alpha below 80 fully transparent, limited to a square of 24 logical px. Pixels at or above the threshold (the anti-aliased window edge) are kept. On by default; the PoC client has `--no-clean-corners` to compare.
**Rationale:** Simple, needs no knowledge of the corner radius, and leaves square corners untouched (an opaque corner pixel stops the fill at once). It is image processing and therefore belongs in the app, not in the extension (D-003, D-006).
**Rejected:** A geometric mask with a fixed radius: radii differ between toolkits and versions, and a wrong radius cuts into the window.
**Consequences:**
- The edge pixels kept still contain a little shadow. Not visible in practice.
- Windows with their own transparency near the corners (e.g. a translucent terminal) could lose low-alpha pixels in the corner squares. Accepted for now.

## D-010 Crop to the opaque core (hard edges)

**Context:** Toolkits draw a soft outline of about 1 logical px around the window. With Files it straddles the frame rectangle on three sides but lies completely outside it on the right, so a frame crop has three soft edges and one hard edge (PoC finding 2).
**Decision:** The app crops to the window's **opaque core**: for each side it takes the outermost line within 4 logical px of the frame edge whose middle half is fully opaque. All four edges are then hard; the rounded corners keep their anti-aliasing and are cleaned by D-009. If no opaque core is found (translucent windows), it falls back to the frame rectangle.
**Rationale:** Symmetric, clean edges regardless of how a toolkit aligns its outline. The soft outline is part of the system look, like the shadow, which the instruction pages add via CSS (D-004).
**Consequences:**
- The app requests the uncropped buffer from the extension (`crop: "buffer"`) and does all cropping itself. The extension's `frame` crop (D-005) stays available but the app does not use it.
- The image loses up to 1 logical px per side compared with the frame rectangle.
- The PoC client offers `--crop core` (default), `frame` and `buffer`.

## D-011 Freeze first, then select, in one capture overlay

**Context:** Screen and region capture (F-01, F-03), delayed capture (F-04) and global hotkeys (F-42, re-added on 2026-10-25 after first being dropped) also need the extension on GNOME. The user wants the screen to be frozen **at the moment of the capture** in every mode, so nothing changes while selecting (2026-10-25).
**Decision:**
- Every capture starts with a **freeze**: the extension takes a picture of the whole screen and, at the same moment, renders every window offscreen (`paint_to_content`, as GNOME's own screenshot UI does). Without delay this happens immediately; with a delay, the extension shows a countdown, hides it, and freezes when it reaches zero.
- Then the extension shows one overlay over the frozen picture, with three modes: **window** (hover and click; the image comes from the frozen window render, so it shows the window as it was at the freeze), **region** (drag a rectangle) and **screen** (click a monitor, or Enter for all monitors). Esc cancels at any time.
- Captures are started from the app or by a **global hotkey** (F-42). The app hides its own window before the freeze, so it does not appear in screen and region captures.
- The **global hotkeys** are registered by the extension itself (`Main.wm.addKeybinding`), never by a D-Bus caller. A key press is genuine user interaction, so a hotkey may capture without the overlay: Shift + Print captures the screen and Alt + Print the active window directly (after the countdown, if a timer is set). A D-Bus caller can still only open the overlay.
- After a hotkey capture the extension hands the image to the app over D-Bus and starts it if it is not running (D-Bus activation).
**Rationale:**
- Freezing first means open menus, hover states and tooltips are kept for region and screen captures, and a window looks exactly as at the moment of the capture.
- A D-Bus caller can only open the overlay; the image is returned only after the user's selection. D-003 holds.
**Consequences:**
- The extension grows from a picker to a small capture UI, but still contains no editing logic.
- A screen capture started from the app needs one click (or Enter); started by Shift + Print it needs none.
- Window captures still contain no popup menus or tooltips: they are separate surfaces (Q-07). With region capture they are included.
- The pointer is never composited (F-05).

## D-012 Object-based editor with a command stack

**Context:** Highlights must stay movable, resizable and restylable (F-21–F-24), callouts renumber themselves (F-24), the dim effect follows the highlights (F-27), and undo/redo must cover many steps (F-11).
**Decision:** The editor keeps the captured image unchanged as the base layer. Every annotation (frame, callout, text, marker, pixelate/blur area, replaced text, dim effect, crop) is an object with properties. Every change is a command on an undo stack. Pixels are only computed when rendering and exporting.
**Rationale:** Editing a pixel image directly would make later moves and restyling impossible and undo memory-hungry.
**Consequences:**
- Exported PNGs contain only the rendered result. Pixelated or blurred areas cannot be recovered from the export.
- The toolkit must offer a good 2D scene graph with hit testing, handles and blur effects. This argues for Qt (QGraphicsScene) in Q-01.
- An editable project format becomes possible later (Q-13).

## D-013 Qt 6 with C++ for the app

**Context:** Three platforms (D-002) with native capture APIs (Win32/WinRT, ScreenCaptureKit, D-Bus) and an object-based editor (D-012). The team knows web technology (TypeScript, React), not C++.
**Decision:** The app is written in **C++20 with Qt 6 Widgets**, built with CMake. The editor uses `QGraphicsScene`/`QGraphicsView`. Platform backends: C++/WinRT and DWM on Windows, Objective-C++ (`.mm`) with ScreenCaptureKit on macOS, QtDBus to the extension on GNOME. All platforms use the same Qt LTS version (6.8).
**Rationale:**
- One renderer on all platforms, so exported images are identical everywhere.
- `QGraphicsScene` provides selectable, movable, resizable items, hit testing and effects out of the box, which D-012 needs.
- C++ reaches all three native capture APIs directly, without bridges.
- The development is handed over to Claude; the team accepts learning C++ for its own changes later (2026-10-25).
**Rejected:**
- Tauri 2 with a TypeScript/React editor: familiar to the team, but three different web views (WebKitGTK on Linux is the weakest) and a Rust layer in between.
- Qt with QML: a JavaScript-like UI language, but still a new world for the team, and the editor's needs fit Widgets/`QGraphicsScene` better.
- PySide6 (Python): packaging and notarising Python bundles on three platforms is cumbersome, and the native capture APIs are only reachable via bridges.
- Transpiling TypeScript to C++: no usable tool exists.
**Consequences:**
- Qt 6.4 from the Ubuntu/Zorin packages is too old; Qt 6.8 LTS is installed separately (Qt online installer or `aqtinstall`) on every platform.
- Qt is used under the LGPL v3, linked dynamically.
- Code must stay readable for C++ newcomers: modern C++, Qt idioms, no template tricks, comments where Qt or platform APIs are not obvious.
