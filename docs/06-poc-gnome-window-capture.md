# PoC: window capture on GNOME

## Goal

Prove that on GNOME 46 / Wayland a single window can be captured so that

1. only that window is in the image (no neighbouring windows, no desktop),
2. the edges are clean (no client-side shadow margins),
3. rounded corners are transparent,
4. picking works like WinSnap: hover highlights the window, a click captures it.

If this works, D-003 holds and the rest of the app can be built on it.

## How it works

- `gnome-extension/nsl-snap@netslash.ch/picker.js` shows a transparent full-screen overlay with a modal grab. The window under the pointer (top-most pickable window on the active workspace whose frame rectangle contains the pointer) gets a blue highlight. Left click or Enter picks it; Escape or right click cancels.
- `extension.js` renders the picked window offscreen with `MetaWindowActor.paint_to_content()`, the same call GNOME's own screenshot UI uses, crops it to the frame rectangle (D-005) and encodes it as PNG with `Shell.Screenshot.composite_to_stream()`.
- D-Bus: `ch.netslash.Snap.Shell`, `/ch/netslash/Snap`, interface `ch.netslash.Snap1`, method `PickWindow(a{sv} options) → (ay png, a{sv} info)`. Errors: `ch.netslash.Snap1.Error.Cancelled`, `.Busy`, `.InvalidArgs`, `.Failed`.
- `tools/nsl-snap-pick` calls it and saves the PNG to `~/Pictures/NSL Snap/<date>_<title>.png` (or `-o FILE`).

## Running it

### In the real session (recommended for judging the result)

```bash
scripts/dev-install.sh
```

Then log out and in again (Wayland cannot reload GNOME Shell). Afterwards:

```bash
tools/nsl-snap-pick --info
```

Useful variants:

- `--crop buffer` keeps the shadow margins, for comparison with the default.
- `--clipboard` also copies the image (needs `wl-copy`, package `wl-clipboard`).
- A GNOME custom shortcut (Settings → Keyboard → Custom Shortcuts) with the command `/path/to/tools/nsl-snap-pick --clipboard` gives a hotkey right away.

After editing the extension code: `gnome-extensions disable nsl-snap@netslash.ch && gnome-extensions enable nsl-snap@netslash.ch` reloads `extension.js` only partly (GJS caches ES modules). To be sure, log out and in again, or use the nested shell.

### In a nested shell (no logout needed)

```bash
scripts/nested-shell.sh
```

This opens a GNOME Shell in a window with its own session bus. Open a terminal and a few apps inside it, and run `tools/nsl-snap-pick` from that terminal.

Disable the extension again with `gnome-extensions disable nsl-snap@netslash.ch`; the symlink is `~/.local/share/gnome-shell/extensions/nsl-snap@netslash.ch`.

## Results

| Check | Status |
|---|---|
| Extension loads on GNOME 46 without errors, D-Bus service answers (`Version` = 1) | ✓ verified in the nested shell (2026-10-25) |
| Picking with hover highlight | to be tested by hand |
| Only the window, overlapping windows not visible | to be tested by hand |
| Clean edges on GTK4/libadwaita apps (e.g. Files, Settings) | to be tested by hand |
| Transparent rounded corners | to be tested by hand |
| GTK3, Qt, Electron, Xwayland apps (e.g. Firefox, VS Code) | to be tested by hand |
| Fractional scaling | to be tested by hand |

Known in advance: menus, tooltips and popovers are separate surfaces and are not part of the capture (Q-07). The nested shell logs "Error while downloading update for extension nsl-snap@netslash.ch", because GNOME asks extensions.gnome.org for an update of an unknown UUID. Harmless.
