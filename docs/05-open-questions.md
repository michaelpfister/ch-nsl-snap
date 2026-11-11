# Open questions

| # | Question | Current assumption | Blocks |
|---|---|---|---|
| Q-05 | Which GNOME versions does the extension support? | 46 (tested), newer versions as they come | metadata.json |
| Q-06 | Distribute the extension via extensions.gnome.org, or only together with the app? | together with the app | packaging |
| Q-07 | Should menus and tooltips of the picked window be included? | not in the PoC | window capture |
| Q-12 | OCR for text replacement (F-29): in v1 or later? Tesseract or Windows OCR? | later; v1 is manual | F-29 |
| Q-13 | Save editable projects (image + annotations) to continue later? | not in v1, but the model allows it (D-012) | file format |
| Q-14 | GNOME binds Print, Shift + Print and Alt + Print to its own screenshot UI. How does the extension take them over? | the extension empties GNOME's three bindings (`org.gnome.shell.keybindings`) while it is enabled and restores them on disable; a setting can turn this off | F-42 on GNOME |
| Q-19 | Replaced text on a photo or gradient: fill with the main colour (now) or blur the area under the new text? | fill; blur as an option later | F-29 |
| Q-17 | Bundle the Inter font (SIL Open Font License) with the app, so Windows and macOS look like the design? | yes, before the first Windows/macOS build | packaging |
| Q-20 | A capture started while the activities overview is open shows the overview in the frozen picture, but window mode highlights the windows at their real places. Close the overview before freezing? | rare: the app hides itself first and the overview is usually closed; not handled yet | F-02 on GNOME |
| Q-16 | Default global hotkeys on macOS (no Print key; Cmd + Shift + 3/4/5 belong to the system)? | Cmd + Shift + 6 (last mode), 7 (screen), 8 (window), 9 (region) | F-42 on macOS |
| Q-22 | Use ext-image-copy-capture (compositor-neutral, also windows) instead of wlr-screencopy and hyprland-toplevel-export once the compositors in use offer it? | later; would give window capture on more desktops | D-031 |
| Q-23 | Rotated monitors (Wayland output transform) on Hyprland and Sway: is the frozen picture already upright? | not handled; untested | D-031 |
| Q-24 | Hyprland rounds window corners only when composing (decoration:rounding); round the capture like on Windows 11 (D-022)? | no: square, as the window buffer is | D-031 |
| Q-25 | Sign releases (minisign signature checked by the app, Windows code signing against SmartScreen)? | not yet: SHA-256 from the same release (D-037) | trust in updates |
| Q-26 | Ship the GNOME extension with the releases, so a downgrade on GNOME takes the matching extension along? | not yet; installed from the repository (Q-06) | D-037 on GNOME |
| Q-27 | GNOME video: may the extension open a Mutter screen cast (`org.gnome.Mutter.ScreenCast`, `RecordWindow` / `RecordMonitor`) and hand the PipeWire stream to the app? Can the top bar indicator be kept out of monitor recordings? | yes, to be proven in a spike before the GNOME backend; else the ScreenCast portal with its dialog | F-50 on GNOME |
| Q-28 | Does Qt Multimedia's FFmpeg build encode H.264 on every Linux machine (software encoder, or VA-API only)? | no software encoder in Qt 6.8.3 from aqt (2026-11-11): only `h264_vaapi` and `h264_nvenc`, so it needs the graphics card. Still to check on Zorin (VA-API) and NixOS (nixpkgs' FFmpeg, perhaps with x264); without an encoder the app says so. If too many machines lack one: bundle OpenH264 or encode with a system FFmpeg | F-50 on Linux |
| Q-29 | Trimming (F-51): re-encode the chosen range (frame-exact, slower) or cut at key frames without re-encoding (fast, about 1 s exact)? | re-encode; decided in phase 1 | F-51 |
| Q-30 | A window resized during the recording: scale it into the start size (black bars), or crop? | scale, keeping the aspect ratio | F-50 |
| Q-31 | Monitor recordings on Wayland: no protocol keeps the indicator out of the video. Show it only in the tray and on other monitors, or accept it in the video? | a small layer surface in the top right corner, on a monitor that is not recorded if there is one, else shown in the video; stop also via tray or command. Without LayerShellQt (Ubuntu builds) the compositor places it, Sway in the middle | F-50 on Hyprland, Sway, portal |

## Resolved

| # | Question | Resolution | Recorded in |
|---|---|---|---|
| Q-02 | Language of the user interface? | German (Switzerland), no "ß", and English since 2026-11-04; chosen in the settings, by default the system's language | D-038 |
| Q-03 | Name of the product? | "Snapslash", with the logo of two frame corners and a slash (2026-11-03, issue #16) | D-036, D-007 |
| Q-15 | Windows 11 opens the Snipping Tool on Print. Ask users to switch that off, or use a low-level keyboard hook? | `RegisterHotKey`, no hook; a note once at start and in the settings, with a button to the Windows setting (2026-10-27) | D-035 |
| Q-21 | Do window capture and the layer-shell overlay work on a real Hyprland? | Yes: a user on NixOS with Hyprland reported "everything works now, even screen capture", saving included (2026-10-27, issue #11) | D-031 |
| Q-18 | How does the user change the order of numbered frames? | Drag entries in the object list (2026-10-25) | 04-features.md F-24 |
| Q-01 | Technology of the app? | Qt 6.8 LTS with C++20 and Widgets; macOS added as third platform | D-013, D-002 |
| Q-04 | Feature scope? | Defined 2026-10-25: F-01–F-29 | 04-features.md |
| Q-11 | Which basics beyond the original list? | Clipboard, save with naming scheme, arrows, tray icon (F-40, F-41, F-44, F-45), shortcuts after WinSnap (F-42, F-46, F-47); no shadows | 04-features.md, D-004 |
| Q-08 | Does a hotkey for "capture without picking" count as user interaction? | Yes, if the extension owns the hotkey; D-Bus callers always get the overlay (hotkeys re-added 2026-10-25) | D-011 |
| Q-10 | How do we handle the soft, asymmetric window outline (PoC finding 2)? | Crop to the opaque core, hard edges everywhere; shadows come from the pages' CSS | D-010 |
| Q-09 | How do we remove the shadow residue in the rounded corners (PoC finding 1)? | Flood fill from each corner through low-alpha pixels, in the app | D-009 |
| — | Which Linux desktop? | GNOME on Wayland (Zorin OS, GNOME 46) | D-002 |
