# Platform capabilities

What each platform lets an application do, and the way we use. "Extension" means our GNOME Shell extension (D-003).

## Capture

| Capability | Windows | GNOME / Wayland | KDE / Wayland (not a target) |
|---|---|---|---|
| Full screen, one monitor | Windows API | Extension or Screenshot portal | KWin ScreenShot2 or portal |
| Free area | Freeze full screen, select in overlay | Same, via extension or portal image | Same |
| Single window, clean edges | DWM extended frame bounds + capture API | **Extension** (offscreen render of the window actor) | KWin ScreenShot2 |
| Single window, fallback | — | ScreenCast portal, window source (extra dialog every time) | Same |
| Window under mouse (hover highlight) | yes | **Extension only** | KWin only |
| Transparent rounded corners | Windows 11: yes (DWM) | yes, the client draws them with alpha | yes |
| Hidden or overlapped window | yes (capture API) | yes, offscreen render ignores overlaps | yes |
| Menus / tooltips of a window | yes | not in the PoC (they are separate surfaces) | partly |
| Scrolling capture | possible (input injection) | practically impossible (no input injection) | practically impossible |
| Mouse cursor in the image | yes | yes (extension can composite it) | yes |

## System integration

| Capability | Windows | GNOME / Wayland |
|---|---|---|
| Global hotkey (e.g. Print) | `RegisterHotKey` | Extension keybinding, or GNOME custom shortcut that starts the app with an argument |
| Tray icon | yes | Only with the AppIndicator extension (installed on Zorin by default) |
| Clipboard with PNG | yes | yes (Wayland clipboard, also from the extension via `St.Clipboard`) |
| Start with session | Autostart / registry | XDG autostart `.desktop` file |

## Known limits on GNOME / Wayland

- Everything window-related depends on the extension. Without it, only the portal paths remain.
- The JavaScript API of GNOME Shell changes between major versions. The extension declares the versions it was tested with in `metadata.json`.
- Fractional scaling: the window texture has the window's resource scale. Crop coordinates are converted from logical to texture pixels (see the PoC).
