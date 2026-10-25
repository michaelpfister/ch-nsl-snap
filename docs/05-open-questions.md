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

## Resolved

| # | Question | Resolution | Recorded in |
|---|---|---|---|
| Q-01 | Technology of the app? | Qt 6.8 LTS with C++20 and Widgets; macOS added as third platform | D-013, D-002 |
| Q-04 | Feature scope? | Defined 2026-10-25: F-01–F-29 | 04-features.md |
| Q-11 | Which basics beyond the original list? | Clipboard, save with naming scheme, arrows, tray icon (F-40, F-41, F-44, F-45); no hotkeys, no shadows | 04-features.md, D-004 |
| Q-08 | Does a hotkey for "capture without picking" count as user interaction? | Moot: no global hotkeys; every capture goes through the overlay | D-011 |
| Q-10 | How do we handle the soft, asymmetric window outline (PoC finding 2)? | Crop to the opaque core, hard edges everywhere; shadows come from the pages' CSS | D-010 |
| Q-09 | How do we remove the shadow residue in the rounded corners (PoC finding 1)? | Flood fill from each corner through low-alpha pixels, in the app | D-009 |
| — | Which Linux desktop? | GNOME on Wayland (Zorin OS, GNOME 46) | D-002 |
