# Open questions

| # | Question | Current assumption | Blocks |
|---|---|---|---|
| Q-02 | Language of the user interface? | German (Switzerland), no "ß" | UI strings |
| Q-03 | Name of the product? | "NSL Snap" as working title | identifiers (D-007), packaging |
| Q-05 | Which GNOME versions does the extension support? | 46 (tested), newer versions as they come | metadata.json |
| Q-06 | Distribute the extension via extensions.gnome.org, or only together with the app? | together with the app | packaging |
| Q-07 | Should menus and tooltips of the picked window be included? | not in the PoC | window capture |
| Q-12 | OCR for text replacement (F-29): in v1 or later? Tesseract or Windows OCR? | later; v1 is manual | F-29 |
| Q-13 | Save editable projects (image + annotations) to continue later? | not in v1, but the model allows it (D-012) | file format |
| Q-14 | GNOME binds Print, Shift + Print and Alt + Print to its own screenshot UI. How does the extension take them over? | the extension empties GNOME's three bindings (`org.gnome.shell.keybindings`) while it is enabled and restores them on disable; a setting can turn this off | F-42 on GNOME |
| Q-15 | Windows 11 opens the Snipping Tool on Print. Ask users to switch that off, or use a low-level keyboard hook? | ask once on first start, with a button that opens the setting | F-42 on Windows |
| Q-17 | Bundle the Inter font (SIL Open Font License) with the app, so Windows and macOS look like the design? | yes, before the first Windows/macOS build | packaging |
| Q-16 | Default global hotkeys on macOS (no Print key; Cmd + Shift + 3/4/5 belong to the system)? | Cmd + Shift + 6 (last mode), 7 (screen), 8 (window), 9 (region) | F-42 on macOS |

## Resolved

| # | Question | Resolution | Recorded in |
|---|---|---|---|
| Q-18 | How does the user change the order of numbered frames? | Drag entries in the object list (2026-10-25) | 04-features.md F-24 |
| Q-01 | Technology of the app? | Qt 6.8 LTS with C++20 and Widgets; macOS added as third platform | D-013, D-002 |
| Q-04 | Feature scope? | Defined 2026-10-25: F-01–F-29 | 04-features.md |
| Q-11 | Which basics beyond the original list? | Clipboard, save with naming scheme, arrows, tray icon (F-40, F-41, F-44, F-45), shortcuts after WinSnap (F-42, F-46, F-47); no shadows | 04-features.md, D-004 |
| Q-08 | Does a hotkey for "capture without picking" count as user interaction? | Yes, if the extension owns the hotkey; D-Bus callers always get the overlay (hotkeys re-added 2026-10-25) | D-011 |
| Q-10 | How do we handle the soft, asymmetric window outline (PoC finding 2)? | Crop to the opaque core, hard edges everywhere; shadows come from the pages' CSS | D-010 |
| Q-09 | How do we remove the shadow residue in the rounded corners (PoC finding 1)? | Flood fill from each corner through low-alpha pixels, in the app | D-009 |
| — | Which Linux desktop? | GNOME on Wayland (Zorin OS, GNOME 46) | D-002 |
