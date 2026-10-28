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

## D-002 Target platforms: Windows, macOS, GNOME and Hyprland on Wayland

**Decision:** Windows 10/11, macOS 14 or later, GNOME (46 and later) on Wayland and, since 2026-10-27, **Hyprland** on Wayland (D-031) are first-class. Other wlroots-based Wayland desktops (Sway, river, Wayfire, ...) get region and screen capture through wlr-screencopy (D-031), all others the portal (D-032). X11 is not a goal.
**Rationale:** Windows and GNOME are in daily use. macOS was added on 2026-10-25 so that screenshots look the same on all platforms the team uses. Hyprland was added on 2026-10-27 because a team member uses it on NixOS (issue #11); it offers window capture to normal apps, so it gets the full feature set without an extension. KDE would allow more (KWin ScreenShot2), but nobody uses it here.
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

**Decision:** `Capture` (formerly `PickWindow`, D-021) returns the PNG as a byte array over D-Bus. Saving, naming and copying to the clipboard is the app's job.
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

## D-014 Own look: Fusion, a token-based style sheet and line icons

**Context:** The skeleton used Qt's default look. The team approved a design mockup (2026-10-25: top bar with capture modes, tool rail on the left, properties panel on the right, light and dark).
**Decision:**
- Qt's **Fusion** style on all platforms, overridden by one style sheet generated from colour tokens (`src/ui/Theme`). Light and dark follow the system setting and switch live.
- **Line icons** as SVG files in `resources/icons/`, drawn with `currentColor`; `src/ui/Icons` recolours them per theme and state.
- **Inter** as UI font where installed, otherwise the system font.
- Widgets are selected in the style sheet by object name (`#TopBar`, `#ToolRail`, `#PropertiesPanel`) or by a `role` property (`primary`, `secondary`, `segment`, `icon`, `section`, …), never by class alone, so new widgets do not change look by accident.
**Rationale:** The same look on Windows, macOS and GNOME, and a single place to change colours. The mockup can be followed closely without custom painting.
**Rejected:** Native styles per platform (three different looks, and GTK/libadwaita cannot be matched from Qt anyway); QML (D-013).
**Consequences:**
- The design mockup is the reference for later UI work: https://claude.ai/artifact/8EMjZ49MFf7EebwCPkaywZ (private, shared by the owner as needed).
- Inter is not bundled yet (Q-17).

## D-015 Annotation sizes in logical pixels; scale stored in the PNG

**Context:** HiDPI captures have 2 image pixels per logical pixel. A 3 px frame drawn in image pixels would look half as thick on such a capture as on a normal one.
**Decision:**
- Line widths, radii and later font sizes are set in **logical pixels** and multiplied by the image scale when drawn.
- The app writes the scale into the PNG's resolution (96 dpi × scale, e.g. 192 dpi for scale 2) and reads it back when opening a file. Files without it, or with odd values, count as scale 1.
**Rationale:** Frames look the same on every capture, and a saved HiDPI capture opens with the right scale again. Browsers ignore the PNG resolution, so the instruction pages are not affected.
**Consequences:** Images from other tools open as scale 1; their frames are then thinner relative to the content. Acceptable.

## D-016 Dim background: one switch and three blur levels; sliders in the advanced mode

**Context:** The first version of F-27 had a switch plus sliders for strength and blur. Blurring a HiDPI capture took about half a second in the debug build, on every slider step, so dimming felt slow (user feedback, 2026-10-25).
**Decision:**
- The switch in the tool rail always dims with **45 %** and **no blur**.
- While dimmed, three small buttons **above** the switch choose **no blur, light (1 px) or strong (3 px)**. The switch keeps its place, so dimming can be toggled without moving the mouse.
- The sliders for strength and blur are only shown in the **advanced mode**, a setting in the new settings dialog (F2, gear button in the top bar). Their changes apply when the slider is released.
- Performance: the blur reads memory in order (columns via a transposed copy) and is compiled with `-O2` even in debug builds; the blurred image is cached per radius and the finished dimmed layer per setting.
**Rationale:** Three fixed levels cover the instruction pages and keep them consistent; fine-tuning is rarely needed.
**Consequences:** Measured on a 2292 × 1318 capture: dimming 17 ms, the first use of a blur level about 100 ms (with a wait cursor), every later switch under 10 ms.

## D-017 Zoom: fit mode that follows the window, steps, crisp pixels

**Context:** New images were shown at most at 100 % and never grew with the window, so details of a capture were hard to read on a laptop screen (user feedback, 2026-10-25).
**Decision:**
- **Fit mode** ("Einpassen", Ctrl+0, and a button next to the zoom controls that stays pressed while active): the image fills the editor area, also beyond 100 %, and follows every window resize. New images start in fit mode.
- Any manual zoom leaves fit mode: zoom buttons and steps (10 % … 800 %), Ctrl+Plus / Ctrl+Minus, Ctrl+wheel and touchpad pinch (proportional, around the pointer), the zoom menu (50/100/200/400 %, Ctrl+Alt+0 = 100 %).
- From two screen pixels per image pixel on, the image is drawn with **crisp pixels** (nearest neighbour) instead of a blurry upscale.
- Panning: scroll (Shift for sideways), middle button, or **Space + drag** for touchpads.
**Rationale:** Reading details needs large, sharp pixels; a window that is enlarged should show the image larger without an extra click.
**Rejected:** Capping fit mode at 100 %: that was the cause of the complaint.
**Consequences:** Small captures (e.g. a little dialog) appear magnified in fit mode; Ctrl+Alt+0 shows them at their real size. 100 % means logical size (D-015): a scale-2 capture at 100 % looks as on the screen it was taken from.

## D-018 Pixelate and blur replace the image pixels

**Context:** F-26 hides personal data (names, e-mail addresses, codes). The result must not be reversible from the exported image.
**Decision:**
- A pixelated or blurred area (`RedactItem`) is an object like a frame (movable, resizable, undoable), but it **replaces** the image pixels under it when drawn (composition mode "Source"), so nothing of the original shows through or remains in the PNG.
- Its rectangle and position are kept on **whole image pixels**, so no half-covered original pixel shows at the edges.
- Kinds: pixelate (block average) or blur (box blur, with some surrounding image so the edge is as soft as the middle). Three levels: light / medium / strong = 6 / 10 / 16 px blocks or 4 / 8 / 14 px blur radius (logical px, D-015). Default: pixelate, medium. Tool P; Alt while drawing gives blur.
- It lies above the dimmed background and darkens itself outside the frames, so the dim layer (computed from the original image) never paints over it.
**Rationale:** Covering the pixels is the only safe way; lightly pixelated text can stay readable, hence medium as default and a hint in the panel.
**Consequences:** The dim layer's blur can spread a few pixels of heavily blurred original just outside a pixelated area. Not readable; accepted.

## D-019 Replace text: fill with the background, draw the new text on the original's baseline

**Context:** F-29 version 1, without OCR (Q-12): replace names, e-mail addresses or codes in forms and dialogs.
**Decision:**
- Tool E: the user marks the text. The area (`ReplaceItem`) is filled with the **background colour** and so replaces the original pixels, like a pixelated area (D-018), whole pixels, above the dim layer.
- `imaging/TextSample` recognises from the marked area: background = most common colour along its edge; text colour = most common colour of the strongest text pixels; left edge of the text; **baseline** = the row where the number of text pixels drops most in the lower half (below it only descenders); top of the capitals with a low threshold; **font size** = (baseline − cap top) / 0.70.
- The new text is drawn with the **system UI font** by default (screenshots mostly show applications of the same system), left-aligned at the original's left edge, on its baseline. Font, size, bold, text and background colour can be changed in the panel; "Original neu erkennen" repeats the recognition.
- Right after drawing, the cursor is in the text field with the last text used (initially "Firstname Lastname"); typing merges into one undo step.
- If the area cuts the original text, the status bar says so, because the cut parts stay visible.
- The recognition is only trusted on a **plain background** (at least 60 % of the edge pixels share one colour) with text that stands out (under 45 % of the text box). Otherwise (photo, gradient) the area is filled with its main colour, the text is centred in a contrasting colour at the current size, and the status bar says so.
- The new text always stays **inside the filled area**: its baseline and left edge are clamped to the area, so letters never stand on the unchanged image (found when testing on a photo background, 2026-10-25).
**Rationale:** Flat backgrounds (forms, lists, dialogs) are the common case; there the result is indistinguishable from the original. Matching the baseline matters more for the look than the exact font.
**Consequences:** On gradients or images the fill is visible; blurring instead of filling would be an option there (Q-19). The exact font of the screenshot is not detected; the system font is a good default on the same system.

## D-020 Crop: a view on the unchanged image, confirmed with Enter

**Context:** F-12: crop the image, but keep the annotations outside so that undoing the crop restores them (D-012).
**Decision:**
- The crop is a **rectangle in image pixels** stored in the scene (`EditorScene::cropRect`), changed only through `SetCropCommand`. The base image is never cut: annotations keep their image coordinates, and pixelated areas or replaced texts still sample the original pixels around them.
- The crop is the **scene rect**. So the export contains exactly the cropped part, annotations stay inside it like at the image edge, and number badges move to a corner that is visible in the crop.
- Annotations **completely outside** the crop are hidden (not deleted): they disappear from the object list and from the numbering, and come back with undo or a larger crop. Partly covered ones stay and are cut at the edge.
- Tool C: the **whole image** is shown with the part to be cut away darkened. A drag outside the draft draws a new one, inside it moves it, the eight handles resize it; always whole pixels and inside the image. The panel shows the size and has "Zuschneiden" and "Ganzes Bild".
- **Enter**, a double click in the draft or "Zuschneiden" confirm it as one undo step and return to the select tool; **Escape** discards the draft. Switching to another tool keeps the draft, as most users expect.
- Outside the crop the editor shows the canvas colour, so the view matches the export. The fit mode (D-017) fits the crop.
**Rationale:** A crop is just another object in the object-based model (D-012); cutting pixels would make undo expensive and lose the annotations outside.
**Consequences:** While the crop tool is active, annotations cannot be selected, and hidden ones are not shown. Selection handles of an annotation at the crop edge are partly covered by the canvas.

## D-021 Region capture: drag, then confirm; one overlay for window and region

**Context:** F-03 on GNOME, the first part of the capture overlay of D-011 besides window capture (F-02).
**Decision:**
- The extension's new D-Bus method **`Capture(a{sv} options)`** (API version 2) freezes the screen and opens the overlay in the mode the app asks for (`mode`: `window` or `region`). The user can switch with **W** and **R**; the reply says which mode was used (`info.mode`), and the app takes that as the last used mode for Ctrl+N. `PickWindow` stays as `Capture` in window mode for the PoC client.
- **Region:** dragging draws the region on the frozen picture, the rest is darkened. A **click into the region, Enter or Space** captures it; dragging again starts a new one, **Backspace/Delete** discards it (F-47). A drag under 4 logical px counts as a click. A hint bar at the top of the primary monitor names the keys; F1 hides it. *(Replaced on 2026-10-26 by D-023: two clicks or a drag, captured without a confirmation step.)*
- **Escape or a right click** cancels in both modes.
- The region is cut from the frozen stage picture in physical pixels (logical × scale) and returned as PNG with the scale; the app opens it as it is, without the window cleanup of D-009/D-010.
- If the extension that is running is still version 1 (updated but not logged out yet), window capture uses `PickWindow` and region capture says that a new login is needed.
**Rationale:** Confirming separately (instead of capturing on release) lets the user check the region on the frozen picture first, as WinSnap does with Enter. One method with a mode keeps D-003 simple: every call ends in the overlay.
**Consequences:**
- Tested in a headless GNOME Shell 46 with injected input (2026-10-25): window and region, the freeze (a window moved during the overlay is captured where it was), Escape, right click, mode switch, Backspace, `PickWindow`, and the whole chain from the app (Ctrl+5 → region in the editor).
- The handles of the region cannot be dragged; a new drag replaces it. Arrow keys for pixel-exact positioning (F-47) are still open.

## D-022 Window capture on Windows: freeze every window with Windows.Graphics.Capture, pick in the app's own overlay

**Context:** F-02 on Windows. On Windows the app may enumerate and capture windows itself; there is no shell extension. The result must match GNOME: only the window, clean edges, transparent corners, as it was at the freeze (D-011), even if other windows cover it.
**Decision:**
- **Freeze:** when the capture starts (the app has hidden itself), `src/capture/windows/WgcCapture` lists the pickable top-level windows like Alt+Tab does (visible, not minimised, not cloaked, i.e. not on another virtual desktop, with a title, no tool windows, no desktop or taskbar), topmost first, and takes **one Windows.Graphics.Capture frame of each, all in parallel**. The frames stay on the GPU; only the picked one is read back. No pointer (F-05), no yellow capture border where Windows allows switching it off (Windows 11). The monitors are grabbed as pictures (GDI `BitBlt`) for the overlay's background.
- **Pick:** the app shows its own overlay (`WindowPicker`), one frameless top-most window per monitor with the frozen picture, at the monitor's physical pixels. The window under the pointer gets the same blue highlight as on GNOME; click, Enter or Space picks it, Escape or a right click cancels, F1 hides the hint.
- **Result:** the picked window's frozen frame, cut to its **DWM extended frame bounds** (the visible frame without the invisible resize borders and without the shadow, `PickGeometry::frameInCapture`), scale = the window's DPI / 96, then the shared cleanup of D-009/D-010.
- **Corners:** the capture is square, because DWM rounds the corners of Windows 11 windows only when composing the screen (on the Windows 11 runner the corner pixel had alpha 108 from the 1 px window border). The app therefore rounds them itself after the core crop, anti-aliased, with DWM's radius: 8 px, 4 px for "round small", none for maximised windows, windows that ask for square corners, and before Windows 11 (`CaptureResult::cornerRadius`, `WindowImage::roundCorners`). Snapped windows, which Windows 11 shows square, still get round corners (accepted for now).
- The WinRT code is C++/WinRT in its own file without Qt (Qt's keyword macros and C++/WinRT do not mix); geometry that can be tested anywhere lives in `src/capture/PickGeometry`.
- Region capture (F-03) on Windows uses the same overlay and the frozen monitors (D-023).
**Rationale:** Windows.Graphics.Capture renders each window on its own, with alpha, also when it is covered, which is what the GNOME extension's offscreen render does. Capturing all windows at the freeze keeps hover states that disappear once the overlay takes the pointer.
**Rejected:** Cutting the window out of a screenshot of the desktop (wrong when covered, background in the corners); `PrintWindow` (many apps, e.g. browsers, draw black or nothing); capturing only after the pick (the window may have changed, hover states are gone).
**Consequences:**
- Needs Windows 10 version 1903 or later (`GraphicsCaptureSession::IsSupported`); before Windows 11 a yellow border may flash around the windows during the freeze.
- The freeze takes one frame time plus the readback; tested on the GitHub runners (Windows Server 2022 x64, Windows 11 ARM64) with real windows: frame size, colours, a covered window, stacking order (`tests/tst_wincapture.cpp`). The overlay itself (multi-monitor, keyboard focus) is tested by hand.
- Test programs are console programs on Windows, so their output reaches ctest.

## D-023 Region: two clicks or a drag (setting), with a magnifier, to the physical pixel

**Context:** The user wants regions without dragging: the first click sets one corner, the second the opposite one, with a magnifier for pixel-exact selection like WinSnap (2026-10-26). Dragging stays available as a setting. On Windows, GNOME and later macOS alike.
**Decision:**
- **Setting** "Bereich auswählen" (F2): **with two clicks** (default) or **by dragging**. The app passes it with every capture: as `region-select` (`clicks`, `drag`) to the GNOME extension (API version 3), as `RegionSelector::Mode` to its own overlay.
- **Two clicks:** the first click sets a corner, the region then follows the pointer, the second click closes it and captures right away. A drag works too. A second click less than 2 px away from the first is ignored (double click).
- **Drag:** press, drag, release captures. A plain click starts nothing.
- **Both corner pixels belong to the region.** Points are **physical pixels** of the frozen picture, so the selection is exact on HiDPI screens too.
- **Crosshair lines** across the whole screen through the selection point (a light and a dark line, visible on any background) and a **magnifier** below right of the pointer (flipped at the monitor edge): 15 × 15 pixels as 11 px squares, the middle row and column marked, the selection pixel framed; underneath the coordinates and, after the first corner, the size.
- **Keyboard (F-47):** arrow keys move the selection point by one physical pixel, Ctrl + arrow by 10, and move the real pointer along; Enter or Space acts as a click (in both modes); Backspace or Delete discards the first corner; W and R switch between window and region; Escape or a right click cancels.
- The same behaviour in two places: `RegionSelector` and `CaptureOverlay` in the app (Windows, later macOS; tested on every platform, `tests/tst_overlay.cpp` on a simulated HiDPI monitor) and the extension's overlay (`overlay.js`, which mirrors `RegionSelector`).
**Rationale:** Two clicks are easier on a touchpad than holding a button down, and the magnifier makes the click land on the intended pixel. Capturing on the second click needs no extra confirmation.
**Consequences:**
- Replaces the confirmation step of D-021 (click into the region or Enter): the second click captures.
- Tested in the headless GNOME Shell 46 with injected input, also at scale 2 (`scale-monitor-framebuffer`): two clicks (200 × 150 px exactly), arrow keys across a colour boundary (3 red and 3 blue pixels), drag mode (a click starts nothing), Backspace, and 4 × 2 physical pixels at scale 2 that the mouse alone cannot reach.
- On Windows the region may span several monitors; it is put together from the frozen monitors in physical pixels, with the scale of the monitor that holds its middle.

## D-024 Screen capture: click a monitor, Enter for all

**Context:** F-01, the third mode of the capture overlay of D-011, on GNOME and Windows.
**Decision:**
- **Screen mode** in the same overlay as window and region (start with "Bildschirm" or Ctrl+1; S switches to it, W and R away). The monitor under the pointer gets a square blue frame; **a click** captures that monitor, **Enter or Space all monitors** together. With a single monitor both do the same, and the hint says so.
- All monitors: the rectangle around them; **gaps** between monitors of different sizes **stay transparent**. The scale is that of the monitor holding the middle (else the primary one's).
- GNOME: `mode: "screen"` in the extension's `Capture` (API version 4); the result is cut from the frozen stage picture in physical pixels like a region. The app (Windows, later macOS): `CaptureOverlay::Mode::Screen`; `CaptureOverlay::compose` puts the frozen monitors together, also for regions across monitors.
- Ctrl+N repeats the last mode, also screen.
**Rationale:** One overlay for all three modes keeps the keys and the look the same; capturing all monitors with Enter needs no extra button.
**Consequences:** Tested in the headless GNOME Shell 46 with two monitors (1280 × 800 and 800 × 600) at scale 2: one monitor (800 × 600 px), all (2080 × 800 px, transparent gap), switching from window mode with S; on every platform with `tst_overlay` (click, Enter, composing two monitors with a gap).

## D-025 Delayed capture: a countdown that leaves the desktop usable, Escape through a temporary global key

**Context:** F-04: with a timer the user wants to open a menu or hover something before the freeze, so the countdown must not take the input; Escape must still cancel it.
**Decision:**
- The **timer** in the top bar (Aus, 3 s, 5 s, 10 s) is remembered and passed with every capture. The app hides itself, then the **countdown** runs: a small dark box with the seconds in the middle of the primary monitor ("Esc oder Klick: Abbrechen"). Only the box takes clicks (a click cancels); everything else stays usable, and it never takes the focus.
- At zero the box disappears, and after 150 ms (a frame without it) the screen is frozen and the overlay opens in the chosen mode (D-011).
- **Escape** goes to the window that has the focus, not to the shell or the app. So while the countdown runs, Escape is a global key: on GNOME a keybinding of the extension (`cancel-countdown` in its new settings schema, `Main.wm.addKeybinding`), on Windows `RegisterHotKey`. Both are removed as soon as the countdown ends. Meanwhile Escape does not reach applications (e.g. to close a menu); accepted.
- GNOME: option `delay` (seconds) of `Capture`, extension API version 5 (`countdown.js`). The app (Windows, later macOS): `Countdown` widget.
**Rationale:** A grab of all input would make the timer useless; the temporary global key is the least intrusive way to keep Escape.
**Consequences:**
- The extension now has a settings schema, compiled by `scripts/dev-install.sh` (the compiled file is not in the repository); CI checks that it compiles. The global hotkeys (F-42) will use the same schema.
- Tested in the headless GNOME Shell 46: the countdown is shown while a window keeps the focus, Escape sent to that window cancels, after running out the overlay opens and the countdown is not in the frozen picture; the app's backend with a 2 s delay and screen mode. `Countdown` tested offscreen on every platform (running out, click, Escape); the global Escape on Windows by hand.

## D-026 Free text: typed on the canvas, one undo step per edit

**Context:** F-25: labels and short explanations in instruction pages ("Hier klicken"), editable afterwards.
**Decision:**
- Tool T: a click on the image starts a **new text at the click** (its first line centred on it) and the user types right there; a click on an existing text, or a **double click with any tool**, edits it. Enter starts a new line; **Escape or a click elsewhere** ends the edit. A click that leaves nothing typed adds nothing; emptying a text deletes it.
- Each edit is **one undo step** ("Text schreiben", "Text ändern"), pushed when the edit ends; inside the edit Ctrl+Z undoes the typing. Saving, copying, undo/redo and changing the tool end the edit first, so the export never shows a cursor.
- `TextItem` is a `RectItem` without handles: a child `QGraphicsTextItem` lays out, draws and edits the text, the item draws the box and the outline, and its rectangle follows the text. A text that grows beyond the image is moved back into it when the edit ends.
- Look in the panel: the frame colours plus black, size in logical px (default 16), bold (default on), background **Ohne / Weiss / Farbig** (white box with coloured text, or a box in the colour with white text, dark on light colours). The **system UI font**, like D-019. Changes apply to all selected texts and become the default for new ones.
- Texts lie above the frames and, like them, are not dimmed (F-27).
**Rationale:** Typing on the canvas is what users know from WinSnap and other editors, and shows the result while typing. One step per edit keeps undo meaningful.
**Consequences:** No font choice and no resizing by handles in version 1 (size comes from the panel). With Inter bundled (Q-17) the font will be the same on all platforms.

## D-027 Colours: standard colours plus a colour wheel, hex code and eyedropper

**Context:** Frames and texts need colours beyond the presets (house colours, a colour taken from the screenshot), and texts need white (feedback 2026-10-26).
**Decision:**
- Every colour row (`ColorRow`: frames, numbered frames, texts) shows the same **eight standard colours** (red, orange, yellow, green, blue, violet, black, white) and, last, a **custom button** (rainbow ring). A colour that is not a standard one is shown inside that ring.
- The custom button opens the **colour popup** (`ColorPopup`): a **ring with a triangle** (`ColorWheel`, HSV; the ring picks the hue, the triangle turns towards it and picks saturation and brightness, its corners being the hue, white and black), the **hex code** and an **eyedropper**. The colours of replaced texts (D-019) open the same popup directly.
- Every change applies right away, also while dragging; style commands of the same objects merge, so a drag is one undo step. A click outside closes the popup.
- The **eyedropper** works on the image in the editor, not on the whole screen: the popup closes, a click on the image takes the colour of that pixel of the captured image (without annotations and dimming), and the popup opens again with it. A loupe next to the cursor shows 11 × 11 pixels and the hex code; Escape or the right button cancel.
**Rationale:** The ring and triangle follow the model the user knows (Affinity, GTK); the hex code is how web teams exchange colours. Picking on the whole desktop is not possible on Wayland without a portal round trip, and the colours that matter are in the screenshot anyway.
**Consequences:** Recent colours and remembering across restarts followed in D-029.

## D-028 Text marker and arrows

**Context:** F-28 and F-44, the last annotation tools of the editor.
**Decision:**
- **Marker** (tool M): a rectangle dragged over a text, filled with the marker colour in **multiply** mode, so black text stays black and white turns into the colour. Its own light palette (yellow, orange, green, cyan, pink, violet) plus a custom colour (D-027). It lies on the image, under the dim layer: dimmed with the background like ink on paper. On dark backgrounds it shows little, like a real highlighter; the panel says so.
- **Arrow** (tool A): dragged from the start to the tip, Shift snaps to steps of 45°. `ArrowItem` keeps the line with its direction; its `RectItem` rectangle is the box around the line, so moving keeps it inside the image. Instead of eight handles it has one at each end, which can be dragged (one undo step each). Only the line and the head grab, not the empty box. Colour (standard palette) and line width (2–12 px, default 4) in the panel; the filled head grows with the width but takes at most 60 % of a short arrow. Arrows lie above the frames, below the texts.
- The panel now shows its groups through `PanelSection`, one selection title for every kind.
**Rationale:** A rectangle marker covers the common case (lines of text in dialogs) and stays exact; multiply is how a real highlighter looks. Arrows with two end handles are what users know from other editors.
**Consequences:** No freehand marker strokes and no curved or double-headed arrows in version 1.

## D-029 Styles for new objects and recent own colours are remembered

**Context:** F-22: "the last used colour is remembered". Users of one team work with the same few colours and sizes every day.
**Decision:**
- The styles for new objects are stored in the settings whenever the panel changes them and loaded at start: frames (colour, width, corners, the number tool's corner), texts (colour, size, bold, box), arrows (colour, width), markers (colour), pixelating (kind, strength) and the text for replaced texts. Not the dim settings: dimming is a property of one image.
- Invalid stored values (edited by hand, older versions) fall back to the defaults, sizes are clamped to the panel's ranges.
- The colour popup shows the **last 8 own colours**, newest first, shared by all objects. A colour joins the list when the popup closes after applying it; while dragging in the wheel only the last colour counts, and standard colours are not added.
**Rationale:** Remembering the panel is what users expect after "last used"; a shared list of own colours makes a house colour one click away everywhere.
- **Reset:** "Auf Standard zurücksetzen" in the settings (F2), after a question, forgets the stored styles and the recent colours; the editor and the panel take the defaults at once. Objects already drawn keep their look, and the other settings stay.
**Consequences:** Each new setting for objects needs its pair in `Settings` and a line in `MainWindow::loadDefaultStyles()`.

## D-030 Saving with a dialog, feedback on the buttons, clearing, tray icon

**Context:** Testing showed that "Kopieren" and "Speichern" worked but gave no noticeable feedback, and that saving without asking surprised users; a visible way to clear the editor was missing (2026-10-26).
**Decision:**
- **Feedback:** buttons get darker while pressed; after copying or saving, the button shows a check mark and "Kopiert" / "Gespeichert" in green for 1.6 s, with the same width, plus the message in the status bar.
- **Saving:** the "Speichern" button (and Ctrl+Shift+S) opens the save dialog with the folder and a name from the pattern filled in; it starts in the folder of the last save of the session, else in the folder from the settings. **Ctrl+S** keeps saving directly into the folder from the settings, like WinSnap (F-46).
- **Settings** (now in tabs: Allgemein, Speichern, Farben und Stile): save folder (default `~/Pictures/NSL Snap`) and file name pattern with `{Datum}`, `{Zeit}` and `{Titel}` (default `{Datum}_{Zeit}_{Titel}`, as before) with a live example. Characters that are not allowed in file names on any platform become `_` (`FileNames`).
- **Clearing:** a bin button in the top bar next to undo/redo (Ctrl+Delete as before). It asks only if the image was neither saved nor copied since the last change (undo stack clean state); opened files count as saved. Quitting asks the same way.
- **Tray icon** (F-45): menu with the three capture modes, "Editor öffnen", "Einstellungen" and "Beenden"; a click opens the editor. Closing the window or Escape (with nothing left to cancel) hides it to the tray, once with a notice; a setting turns this off. A capture started from the tray and cancelled leaves the window hidden. Without a tray (GNOME without the AppIndicator extension) the app behaves as before.
**Rationale:** Visible confirmation is what makes the two most used buttons trustworthy; the dialog is the expected behaviour of a "Speichern" button, while the shortcut stays fast for experienced users.
**Consequences:** On GNOME the tray needs the AppIndicator extension; the settings say so when there is no tray.

## D-031 Wayland without GNOME: freeze with wlr-screencopy, windows from Hyprland, pick in the app's own overlay

**Context:** A team member uses Hyprland on NixOS without GNOME (issue #11). The GNOME extension (D-003) does not exist there, and neither does the portal fallback D-002 promised. "Wayland does not allow capturing" is not quite right: Wayland itself forbids nothing; each compositor decides what it offers to apps. GNOME offers nothing, hence the extension. wlroots-based compositors offer **wlr-screencopy** (pictures of whole monitors) to every app; Hyprland additionally offers **hyprland-toplevel-export** (the contents of one window, also when covered) and its control socket (the `hyprctl` one), which lists every window with its position, size, monitor and workspace in logical pixels.
**Decision:**
- **Backend choice at start** (`createCaptureBackend`): GNOME (by `XDG_CURRENT_DESKTOP` or a running extension) keeps the extension. Otherwise, if the compositor offers wlr-screencopy, `WaylandBackend`; else the portal (D-032).
- **Freeze** (`src/capture/wayland/WaylandFreeze`): one wlr-screencopy frame per monitor and, on Hyprland, one toplevel-export frame per visible window, all requested together, copied into shared memory (wl_shm, no dmabuf), without the pointer. The app talks Wayland itself through Qt's connection but on its own event queue (`WaylandConnection`), with the protocol files in `protocols/` and C code from `wayland-scanner`. Waits up to 20 s, because Hyprland may first ask the user (below).
- **Windows** (`HyprlandIpc`): `j/clients` and `j/monitors` from the control socket, right before the freeze. Visible are the mapped, not hidden windows on each monitor's active workspace, its open special workspace, and pinned ones. Hyprland reports no stacking order; it is approximated: special workspace, pinned, full screen, floating, tiled, each group by how recently it had the focus. The handle for the export is the lower 32 bits of the window's address. Hyprland's border and rounding are drawn by the compositor and are not in the picture (D-004); a client's own shadow margins are cut off at the bounding box of the fully opaque pixels.
- **Pick:** the same overlay as on Windows (`CaptureOverlay`, D-022, D-023, D-024), one window per monitor. Wayland lets no app position its windows or keep them on top, and a tiling compositor would tile the overlay: with KDE's **LayerShellQt** (optional at build time, in the Nix package) the overlay is a wlr-layer-shell surface on the overlay layer with the keyboard; without it, it is shown full screen. The countdown (F-04) is a centred layer surface, or a fixed-size window, which tiling compositors float.
- **Coordinates:** Wayland has only logical coordinates. The app makes its own "native" ones (`PickGeometry::nativeLayout`): each monitor as large as its picture, placed per axis so that monitors never overlap and monitors side by side touch, also with mixed scales.
- Every capture stays interactive (D-003): the user picks on the frozen picture in the app's overlay.
**Rationale:** The same workflow and result as on GNOME and Windows, without an extension and without a system dialog per capture.
**Rejected:**
- An external tool (`grim`, `slurp`, `hyprshot`) with its output piped into the editor: no freeze before the selection (D-011), no window list, and one more dependency. It was the fallback considered first.
- ext-image-copy-capture (the newer, compositor-neutral protocol): not yet offered by every Hyprland version in use; worth a look later (Q-22).
- Window capture on other wlroots desktops: they offer no window list to apps.
**Consequences:**
- Hyprland with `ecosystem:enforce_permissions = true` asks once whether NSL Snap may capture (`screencopy`), or needs a `permission` rule for its binary; a refused permission gives a black picture with "permission denied".
- Tested in a headless Sway (2026-10-27): freeze of two monitors with scales 1 and 2, region and screen capture through the overlay with keyboard input, the countdown, Escape. Hyprland's window list and window export have unit tests with recorded `hyprctl` output; on a real Hyprland (NixOS, Nix package from the flake) a user reported that everything works, saving included (2026-10-27, issue #11, Q-21).
- Rotated monitors (output transform) are not handled yet (Q-23).

## D-032 Other Wayland desktops: the Screenshot portal as the freeze

**Context:** Wayland desktops that offer neither the GNOME extension nor wlr-screencopy, e.g. KDE Plasma. D-002 promised a portal fallback.
**Decision:** `PortalBackend` calls `org.freedesktop.portal.Screenshot.Screenshot` with `interactive = false`: the portal returns one picture of the whole desktop without a dialog of its own (some desktops ask once for permission). The app cuts it into the monitors (`splitDesktop`) and lets the user select a region or a monitor in its own overlay, so the freeze comes first (D-011). A picture the portal left in a temporary folder is deleted; one it saved elsewhere stays. There is no window capture: the portal lists no windows, and the ScreenCast portal's window picker asks every time (D-003).
**Consequences:** Tested in a headless Sway with xdg-desktop-portal-wlr (2026-10-27). The frontend only offers the Screenshot portal when an Access portal (e.g. xdg-desktop-portal-gtk) is installed.

## D-033 No GTK platform theme where GTK would abort the app

**Context:** Issue #11: on NixOS the app aborted when saving: `GLib-GIO-ERROR: No GSettings schemas are installed on the system`. Qt uses its GTK platform theme when `QT_QPA_PLATFORMTHEME=gtk3` (common on NixOS) or by default on GTK-based desktops. GTK reads its settings through GSettings; a binary that is not wrapped with the schema paths finds none, and GLib aborts instead of reporting an error. Reproduced in the headless Sway: Qt's own dialog (`DontUseNativeDialog`) aborts too, because the GTK theme also reads settings for icons.
**Decision:** Before the `QApplication` is created, `PlatformTheme::avoidBrokenGtk()` checks whether Qt would load the GTK theme and none of GLib's schema folders holds GTK's file chooser schema. Then it sets `QT_QPA_PLATFORMTHEME=xdgdesktopportal`: file dialogs come from the desktop portal if one offers them, otherwise from Qt itself. The Nix package wraps the binary with the GTK schema paths (`wrapGAppsHook3`), so there the GTK theme works as configured.
**Rationale:** A missing schema is a packaging fault outside the app, but it must not cost the user their screenshot.
**Consequences:** A user who asked for the GTK theme gets the portal's theme in this case; a note on stderr says so.

## D-034 Command line for captures; one app per user

**Context:** Issue #13: on Hyprland the global keys (F-42) belong to the compositor; the user wants `bind` lines in hyprland.conf that start a capture, and to choose between "just save" and "open in the editor".
**Decision:**
- **Options** (`CommandLine`, F-48): `--capture-window`, `--capture-region`, `--capture-screen` (at most one), `--open`, `--delay <seconds>` (0-60, default 0: the timer in the top bar is not used, the command line says what it wants), `--help`, `--version`. Help and errors work without a display. Exit code 0 saved or opened, 1 cancelled, 2 failed; the saved path goes to stdout, errors to stderr.
- **Without `--open`:** the capture goes straight into the save folder with the naming pattern (F-41), as Ctrl+S would save it, with the window crop and corner cleanup (D-009, D-010) and the scale in the PNG (D-015). The editor's image stays untouched and a hidden window stays hidden; the tray shows a message where there is one. Errors still show a message box, since a key binding has no terminal.
- **With `--open`:** the capture appears in the editor, as from the capture buttons.
- **One app per user** (`SingleInstance`): the first start listens on a local socket (`$XDG_RUNTIME_DIR/ch.netslash.Snap.sock`, a named pipe on Windows), which only the same user may open. A later start sends its arguments there and waits for the answer, so a key binding never opens a second editor or tray icon, and the app's tray setting and window state stay as they are. Started for one capture without `--open`, the app quits after it; with `--open` it stays as the editor. A second capture while one runs is refused (exit 2). A socket left by a crashed app is replaced.
- Every capture stays interactive (D-003, D-011): the command line only starts the overlay; the user picks on the frozen screen.
**Rationale:** The desktop's key bindings are the only global keys there are on Hyprland and Sway, and the usual way users wire up screenshot tools (grim, hyprshot) there. Handing requests to the running app keeps its settings, tray icon and the editor's unsaved work in one place.
**Rejected:** D-Bus activation and a D-Bus interface for the app: not on Windows and macOS, and a socket gives the same with less code. A second process per capture: two tray icons, two editors.
**Consequences:**
- Qt's own GUI options (`-platform` and the like) are not accepted on the command line; use the environment variables (`QT_QPA_PLATFORM`).
- Tested in a headless Sway (2026-10-27): save without a running app (path printed, exit 0, process ends), hand-over to a running app with an image in the editor (image untouched), `--open` into the running editor and as first start, Escape (exit 1), a second capture while one runs (exit 2).

## D-035 Global hotkeys on Windows: RegisterHotKey, a note about the Print key, taken keys shown in the settings

**Context:** F-42 on Windows. Windows 11 opens the Snipping Tool on Print by default (setting "Use the Print screen key to open screen capture", registry `HKCU\Control Panel\Keyboard\PrintScreenKeyForSnippingEnabled`; without the value it is on since Windows 11 22H2), so Print never reaches other programs. Other programs (OneDrive, WinSnap, Greenshot) may hold the other keys. Resolves Q-15 (2026-10-27).
**Decision:**
- The app registers the hotkeys itself with `RegisterHotKey` (`GlobalHotkeys`, no window: `WM_HOTKEY` comes through a native event filter), with `MOD_NOREPEAT`. **No low-level keyboard hook.**
- Keys after WinSnap: Print (last mode), Shift + Print (screen), Alt + Print (active window), Ctrl + Alt + Print (region). Each can be changed or switched off in the new settings tab "Tastenkürzel"; "Standard" restores it.
- **Print key:** if a key is plain Print and the Windows setting is on, the app says so **once** at start, with a button that opens the Windows settings page (`ms-settings:easeofaccess-keyboard`). The app never changes the system setting itself. The settings tab shows the same note as long as the setting is on.
- **Taken keys:** if `RegisterHotKey` fails, the key stays assigned and the settings tab shows "Belegt durch ein anderes Programm oder durch Windows" under it. Keys that cannot be global (e.g. two keys in a row) are marked too.
- While a key field in the settings has the focus, all hotkeys are unregistered, so the new key reaches the field; leaving it registers again and shows whether the key is free.
- **Alt + Print** captures the window that has the focus at the freeze (after the countdown, if set), without the overlay (D-011: a key press is user interaction). If it is not a capturable window (desktop, taskbar), the user picks as usual. `CaptureBackend::captureActiveWindow`; other backends fall back to picking.
- Hotkeys are ignored while a capture runs or a dialog of the app is open.
**Rationale:** `RegisterHotKey` is the documented way and needs no rights; a keyboard hook would see every key press in the system, is removed silently by Windows after one slow answer (`LowLevelHooksTimeout`) and makes virus scanners suspicious. Changing the user's system setting without asking is not acceptable.
**Rejected:** a low-level keyboard hook (`WH_KEYBOARD_LL`) to take Print from the Snipping Tool; writing the registry value ourselves.
**Consequences:**
- With the Windows setting on, plain Print does not work until the user switches it off; `RegisterHotKey` for Print still succeeds, so the app can only tell from the setting, not from the registration.
- On the development machine (2026-10-27) Alt + Print was already taken by another process (OneDrive or the Snipping Tool were running); the settings show it.
- Hotkeys work only while the app runs (tray, F-45); starting with Windows is a separate step (packaging).
- Tested: key conversion, registration, a taken key, suspending (`tests/tst_hotkeys.cpp`). The key presses themselves, the note and Alt + Print by hand.
