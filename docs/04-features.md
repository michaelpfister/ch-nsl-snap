# Features

Feature scope for version 1, as defined on 2026-10-25. Each feature has a platform assessment so it is visible early what is realistic where (see [03-platform-capabilities.md](03-platform-capabilities.md)).

**Platform columns:** ✓ = straightforward · ◐ = possible with restrictions or extra work · ✗ = not possible. "Ext." = needs the GNOME Shell extension (D-003).
**Effort:** S = days · M = about a week · L = several weeks (one person, rough).

Most of the work is in the **editor**, which is platform-independent. Only capture (F-01 to F-05) differs per platform.

## Capture

| ID | Feature | Windows | GNOME | Effort | Notes |
|---|---|---|---|---|---|
| F-01 | **Screen capture**: one monitor or all monitors | ✓ | ✓ Ext. | S | The screen is frozen first; on GNOME one click on the monitor (or Enter for all) confirms (D-011). |
| F-02 | **Window capture** with clean edges and rounded corners | ✓ | ✓ Ext. | done (PoC) on GNOME, M on Windows | D-003, D-005, D-009, D-010. Shows the window as it was at the freeze (D-011). Popup menus and tooltips are separate surfaces and are not part of a window capture (Q-07); use F-03 for those. |
| F-03 | **Region capture**: drag a rectangle on the frozen screen | ✓ | ✓ Ext. | M | Open menus, tooltips and hover states are kept, because the freeze happens before the selection. |
| F-04 | **Delayed capture** in seconds, with a countdown shown on screen | ✓ | ✓ Ext. | S | The countdown is drawn by the extension (GNOME) or a topmost window (Windows). When it reaches zero it disappears, the screen is frozen, and the normal selection of F-01 to F-03 follows on the frozen picture (D-011). Esc cancels. |
| F-05 | **Never capture the mouse pointer** | ✓ | ✓ | – | Default and only behaviour. The extension already does not draw it. |
| F-06 | **Import an existing image**: file dialog or drag & drop onto the editor | ✓ | ✓ | S | PNG, JPEG, WebP. |

## Editor basics

| ID | Feature | Windows | GNOME | Effort | Notes |
|---|---|---|---|---|---|
| F-10 | **Clear editor**: remove the image and all annotations, start from scratch | ✓ | ✓ | S | Asks for confirmation if there are unsaved changes. |
| F-11 | **Undo / redo** with many steps | ✓ | ✓ | M | Every edit is a command on a stack (D-012). |
| F-12 | **Crop** the image | ✓ | ✓ | S | Annotations outside the crop are kept but hidden, so undoing the crop restores them. |

## Annotations

All annotations are objects on top of the image: they can be selected, moved, resized, restyled and deleted at any time until export (D-012).

| ID | Feature | Windows | GNOME | Effort | Notes |
|---|---|---|---|---|---|
| F-20 | **Highlight frame**: draw a rectangle around an element | ✓ | ✓ | M | Example: red and yellow frames around settings. As many as needed. **Done** (2026-10-25): tool R, Shift for a square. |
| F-21 | Highlights can be **moved and resized** afterwards | ✓ | ✓ | (in F-20) | Handles on corners and edges. **Done.** Frames stay inside the image when drawn, moved or resized. |
| F-22 | **Frame colour** per highlight | ✓ | ✓ | S | Preset palette (red, yellow, green, blue, …) plus custom colour; the last used colour is remembered. **Done** for the palette (custom colour and remembering across restarts still open). |
| F-23 | **Square or rounded** frame corners | ✓ | ✓ | S | Per highlight; default configurable. Line width configurable. **Done** (in logical px, D-015). |
| F-24 | **Numbered callouts**: a highlight with a number badge, numbered automatically 1, 2, 3, … | ✓ | ✓ | M | Like the A–D example, but with numbers. Deleting or reordering renumbers the rest. Badge in the frame colour, placed at a corner of the frame (choosable). Matches step lists in instructions ("1. Klicken Sie auf …"). **Done** (2026-10-25): tool N; numbers follow creation order and survive undo; deleting or switching a number off renumbers the rest; position or "Ohne Nummer" per frame. Reorder by dragging entries in the object list (Q-18), undoable. Near the image edge the number moves to the nearest corner where it is fully visible, and the "Nummer" field offers only such corners; the chosen corner returns when the frame moves away from the edge. |
| F-25 | **Text**: free text with colour and font size | ✓ | ✓ | M | Editable afterwards; optional background box for legibility. |
| F-26 | **Pixelate or blur** a selected area | ✓ | ✓ | S | Choice per area. Strength adjustable. The export contains only the obscured pixels, never the original. **Done** (2026-10-25, D-018): tool P, Alt for blur; kind and light/medium/strong in the panel; movable, resizable, undoable. |
| F-27 | **Dim the background**: darken and/or slightly blur everything except the highlights | ✓ | ✓ | M | Like the "Enter code" example: highlights keep full brightness. One global switch with strength sliders; the cut-outs follow the highlight frames automatically. **Done** (2026-10-25, D-016): switch in the tool rail (always 45 %, no blur); while on, three buttons above it for no, light (1 px) and strong (3 px) blur, so the switch never moves. Strength and blur sliders only in the advanced mode (settings, F2). Undoable; transparent window corners stay transparent. |
| F-28 | **Text marker** (highlighter): semi-transparent colour over text, like a felt pen | ✓ | ✓ | S | Like the WinSnap example (yellow, orange, green, cyan). Multiply blending so the text stays black. |
| F-29 | **Replace text**: overwrite a text in the screenshot with another text, e.g. a real name with "Firstname Lastname" | ✓ | ✓ | L | See below. |

### F-29 Replace text

The goal is to replace personal data (names, e-mail addresses, codes) in forms and dialogs without editing HTML before taking the screenshot.

**Version 1 (manual, reliable):**
1. Select the text with a rectangle.
2. The app fills the rectangle with the surrounding background colour (sampled from the border of the selection).
3. The app suggests font size and colour from the selection (text height, darkest/most frequent foreground colour). The font is the platform's UI font (Segoe UI on Windows, Cantarell on GNOME) or one chosen by the user.
4. The user types the replacement text, which is drawn in place and stays editable.

Works well on flat backgrounds (forms, dialogs, lists). On gradients or images the fill is visible.

**Later (OCR-assisted, Q-12):** recognise text in the image locally (Tesseract on both platforms, or Windows' built-in OCR), so that a click on a word selects it precisely, or "Michael Pfister" can be replaced by "Firstname Lastname" in all places at once.

## Basics

Not on the original list, added and confirmed on 2026-10-25 (Q-11).

| ID | Feature | Windows | GNOME |
|---|---|---|---|
| F-40 | Copy result to the clipboard | ✓ | ✓ |
| F-41 | Save as PNG with a naming scheme and default folder; "save as" | ✓ | ✓ |
| F-44 | Arrows (WinSnap has them) | ✓ | ✓ |
| F-45 | Tray icon | ✓ | ◐ needs the AppIndicator extension (installed on Zorin) |

## Keyboard shortcuts

Modelled on [WinSnap's shortcuts](https://www.ntwind.com/software/winsnap/shortcuts.html), so the team keeps its muscle memory. Decided on 2026-10-25, revising the earlier "no hotkeys" (D-011).

### F-42 Global hotkeys (work while the app is hidden)

| Key | Action | WinSnap |
|---|---|---|
| Print | Capture in the last used mode | same |
| Shift + Print | Screen | same |
| Alt + Print | Active window, without picking | same |
| Ctrl + Alt + Print | Region | same |

- Reassignable in the settings. All global captures respect the timer setting (F-04).
- **GNOME:** registered by the extension (D-011). GNOME itself uses Print, Shift + Print and Alt + Print for its screenshot UI; the extension takes them over while enabled (Q-14).
- **Windows:** `RegisterHotKey`. Windows 11 opens the Snipping Tool on Print by default; that setting has to be switched off, or we use a keyboard hook (Q-15).
- **macOS:** Macs have no Print key; the defaults there are still open (Q-16).
- Not adopted: Ctrl + Print (active application, all its windows) and Ctrl + Shift + Print (multi-object selection), because these capture modes are not in our scope.

### F-46 Shortcuts in the app

| Key | Action | WinSnap |
|---|---|---|
| Ctrl + N | New capture in the last used mode | same |
| Ctrl + 1 | Capture screen | same |
| Ctrl + 3 | Capture window | same |
| Ctrl + 5 | Capture region | same (Ctrl + 2 and 4 are WinSnap modes we do not have) |
| Ctrl + O | Open image (F-06) | same |
| Ctrl + V | Paste image from the clipboard (F-06) | same |
| Ctrl + S | Save to the default folder with the naming scheme, no dialog (F-41) | same |
| Ctrl + Shift + S | Save as… | same |
| Ctrl + E | Show the saved file in the file manager | same |
| Ctrl + C | Copy image (F-40) | same |
| Ctrl + Alt + C | Copy file path of the saved image | same |
| Ctrl + Z / Ctrl + Y, Ctrl + Shift + Z | Undo / redo (F-11) | – |
| Delete | Delete the selected annotation | WinSnap: clear image |
| Ctrl + Delete | Clear the editor (F-10), with confirmation | – |
| Escape | Cancel the current tool or selection; if there is none, hide to the tray | WinSnap: hide to tray |
| F2 | Settings | same |
| Ctrl + Plus / Ctrl + Minus | Zoom in / out in steps (D-017) | – |
| Ctrl + 0 | Fit to window; follows window resizes | – |
| Ctrl + Alt + 0 | 100 % | – |
| Ctrl + wheel, touchpad pinch | Zoom around the pointer | – |
| Space + drag, middle button | Pan | – |
| V, C, R, N, T, A, M, P, E | Tools: select, crop, frame, numbered frame, text, arrow, marker, pixelate, replace text | – |

Delete differs from WinSnap on purpose: in an editor with movable objects it must delete the selected object, not the whole image.

### F-47 Shortcuts in the capture overlay

| Key | Action | WinSnap |
|---|---|---|
| Enter, Space, left click | Accept the selection | same |
| Arrow keys / Ctrl + arrow keys | Move the pointer by 1 / 10 px | same |
| Backspace, Delete | Discard the current region and start again | same |
| Escape | Cancel the capture | same |
| F1 | Show / hide the hint bar | same (helper window) |
| W, R, S | Switch mode: window, region, screen | – |

Not adopted from WinSnap: print (Ctrl + P, Ctrl + Shift + P), thumbnail copy (Ctrl + Shift + C), custom commands (Alt + 1–9).

## Not planned

- Scrolling capture (not possible on Wayland, see 03-platform-capabilities.md).
- Video / GIF recording.
- Including the mouse pointer (F-05).
- Shadow, background or margin around captures (former F-43): the instruction pages add the shadow via CSS (D-004).
