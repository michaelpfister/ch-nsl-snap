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

## D-002 Target platforms: Windows and GNOME on Wayland

**Decision:** Windows 10/11 and GNOME (46 and later) on Wayland are first-class. Other Wayland desktops only get the portal fallback. X11 is not a goal.
**Rationale:** These are the platforms in daily use. KDE would allow more (KWin ScreenShot2), but nobody uses it here.

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

## D-004 Shadows and backgrounds are drawn by the app, not captured

**Decision:** Backends deliver the bare window with an alpha channel. Shadow, background, border and margins are effects applied by the shared core.
**Rationale:** System shadows differ per platform, toolkit and theme, and on Wayland they are part of the client's buffer (GTK) or are not available at all. Drawing them ourselves gives the same result everywhere and lets users configure them.

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

## D-008 PoC client in Python (assumption)

**Decision:** The PoC command-line client `tools/nsl-snap-pick` is written in Python with PyGObject, which is already present on GNOME systems.
**Rationale:** It only exists to test the extension. The technology of the real app is still open (Q-01) and does not have to be decided for the PoC.
