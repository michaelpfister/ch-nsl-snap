# Wayland protocols

Protocol descriptions the Wayland backend (`src/capture/wayland/`, D-031) talks to the compositor with. `wayland-scanner` turns them into C code at build time (`CMakeLists.txt`). They are copied unchanged from upstream; each file carries its own licence in its `<copyright>` block (MIT-style for wlr-protocols, BSD-3-Clause for hyprland-protocols).

| File | Source | Commit |
|---|---|---|
| `wlr-screencopy-unstable-v1.xml` | [swaywm/wlr-protocols](https://github.com/swaywm/wlr-protocols) `unstable/` | `b010a03` |
| `wlr-foreign-toplevel-management-unstable-v1.xml` | same; only needed because the Hyprland protocol refers to it | `b010a03` |
| `hyprland-toplevel-export-v1.xml` | [hyprwm/hyprland-protocols](https://github.com/hyprwm/hyprland-protocols) `protocols/` | `cd1aba6` |
