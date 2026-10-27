# Nix packaging

A `flake.nix` at the repository root builds and runs NSL Snap without the
manual apt + aqtinstall setup from [07-development-setup.md](07-development-setup.md).
See D-031 for why it is a flake (not `default.nix`) and why it tracks
nixpkgs' own Qt 6 instead of pinning 6.8.3.

Linux only (`x86_64-linux`, `aarch64-linux`), matching the GNOME capture
backend (D-002).

## Outputs

| Output | What it is |
|---|---|
| `packages.nsl-snap` / `packages.default` | The app, built with nixpkgs' `qt6` (`qtbase`, `qtsvg`, `qtimageformats`). `CMakeLists.txt` has no `install()` rules, so the derivation copies the built `nsl-snap` binary into `$out/bin` itself. |
| `packages.nsl-snap-gnome-extension` | The extension in `gnome-extension/nsl-snap@netslash.ch`, with its settings schema precompiled (`glib-compile-schemas`), laid out like `pkgs.gnomeExtensions.*` (`share/gnome-shell/extensions/<uuid>`). |
| `apps.default` | `nix run .` starts `nsl-snap`. |
| `devShells.default` | CMake, Ninja, nixpkgs' Qt 6 and the OpenGL/Vulkan/xkbcommon headers from docs/07's "System packages" step, for `nix develop`. |
| `checks.default` | A Debug build with `ctest` run offscreen (`QT_QPA_PLATFORM=offscreen`), the same tests CI runs. |

## Usage

```bash
# Build and run the app
nix build .#nsl-snap
./result/bin/nsl-snap
# or directly:
nix run .

# Development shell (replaces docs/07's apt-install + aqtinstall steps)
nix develop
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure

# Run the test suite the same way CI does
nix build .#checks.x86_64-linux.nsl-snap -L

# Build the GNOME extension package
nix build .#nsl-snap-gnome-extension
```

## GNOME extension

Building `packages.nsl-snap-gnome-extension` only produces the files; it does
not install or enable them. GNOME Shell on Wayland only discovers a new
extension after logging out and in again, independently of how the files got
there (D-011). To try it, link the built package's directory the same way
`scripts/dev-install.sh` links the repository's copy:

```bash
nix build .#nsl-snap-gnome-extension
ln -sfn "$(readlink -f result)/share/gnome-shell/extensions/nsl-snap@netslash.ch" \
  "${XDG_DATA_HOME:-$HOME/.local/share}/gnome-shell/extensions/nsl-snap@netslash.ch"
gnome-extensions enable nsl-snap@netslash.ch   # after the next login, if not yet discovered
```

There is no NixOS or home-manager module yet to automate this (Q-21).
