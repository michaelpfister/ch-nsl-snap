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
- `tools/nsl-snap-pick` asks for the uncropped buffer, crops it to the window's opaque core (D-010), cleans the corners (D-009) and saves the PNG to `~/Pictures/NSL Snap/<date>_<title>.png` (or `-o FILE`).

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

- `--crop frame` crops to the frame rectangle instead of the opaque core (soft outline where it lies inside); `--crop buffer` keeps the shadow margins.
- `--no-clean-corners` keeps the shadow residue in the corners (D-009), for comparison.
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
| Picking with hover highlight | ✓ works (Files, 2026-10-25) |
| Only the window, overlapping windows not visible | to be tested by hand with an overlapping window |
| Clean edges on GTK4/libadwaita apps (e.g. Files, Settings) | ✓ Files, after core crop (D-010); Settings to be tested |
| Transparent rounded corners | ✓ after corner cleanup (D-009), confirmed with a new capture (2026-10-25): all corner pixels alpha 0, no residue below alpha 80 left |
| HiDPI (integer scale 2) | ✓ image is exactly frame size × 2 (1334×776 → 2668×1552) |
| GTK3, Qt, Electron, Xwayland apps (e.g. Firefox, VS Code) | to be tested by hand |
| Fractional scaling | to be tested by hand |

### Findings from the first capture (Files, dark theme, scale 2)

1. **Shadow residue in the corners.** The window's client-side shadow lies under the window and is visible in the transparent corner cut-outs inside the frame rectangle. The outermost corner pixels have alpha 13–43 (up to about 17 %), darker at the bottom because the shadow is offset downwards. On a light background this shows as faintly dark corners. Cropping alone cannot remove it. **Fixed** by the corner cleanup in the client (D-009): on the first capture it cleared 566 pixels, all four corner pixels are now fully transparent, the anti-aliased edge is unchanged.
2. **Soft edge, asymmetric by 1 logical px.** A `--crop buffer` capture of the same window (frame rect at image px 94/74, 2668×1552) shows that the window's anti-aliased outline (about 2–4 image px, window colour with falling alpha) straddles the frame rectangle on the left, top and bottom, but lies completely **outside** it on the right (opaque core: x 96–2761, frame: x 94–2761). The crop itself is exact; it is the window's own drawing that is off by 1 logical px relative to its geometry (seen with Files, dark theme). Effect: the right edge of a `frame` capture is hard, the other three are soft. Barely visible, but not symmetric. **Fixed** by cropping to the opaque core (D-010): on this capture it trims 2/2/0/2 image px (left/top/right/bottom), all four edges are fully opaque, the corners stay rounded and clean.

Known in advance: menus, tooltips and popovers are separate surfaces and are not part of the capture (Q-07). The nested shell logs "Error while downloading update for extension nsl-snap@netslash.ch", because GNOME asks extensions.gnome.org for an update of an unknown UUID. Harmless.
