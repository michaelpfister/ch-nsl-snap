# Open questions

| # | Question | Current assumption | Blocks |
|---|---|---|---|
| Q-01 | Technology of the app (UI, editor, Windows backend)? | Qt 6, with C++ or Python (PySide6) still open | everything beyond the PoC |
| Q-02 | Language of the user interface? | German (Switzerland), no "ß" | UI strings |
| Q-03 | Name of the product? | "NSL Snap" as working title | identifiers (D-007), packaging |
| Q-04 | Feature scope? | to be defined, see 04-features.md | roadmap |
| Q-05 | Which GNOME versions does the extension support? | 46 (tested), newer versions as they come | metadata.json |
| Q-06 | Distribute the extension via extensions.gnome.org, or only together with the app? | together with the app | packaging |
| Q-07 | Should menus and tooltips of the picked window be included? | not in the PoC | window capture |
| Q-08 | Does a hotkey for "capture active window without picking" count as user interaction (D-003)? | undecided; needs a keybinding inside the extension so no D-Bus caller can trigger it | hotkeys |

## Resolved

| # | Question | Resolution | Recorded in |
|---|---|---|---|
| Q-09 | How do we remove the shadow residue in the rounded corners (PoC finding 1)? | Flood fill from each corner through low-alpha pixels, in the app | D-009 |
| — | Which Linux desktop? | GNOME on Wayland (Zorin OS, GNOME 46) | D-002 |
