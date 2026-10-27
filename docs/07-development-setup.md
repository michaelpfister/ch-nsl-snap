# Development setup

How to set up a machine to build NSL Snap (D-013: C++20, Qt 6.8 LTS, CMake). Linux (Ubuntu/Zorin and NixOS) and Windows are described; macOS follows when its backend is built.

## Linux (Zorin OS 18 / Ubuntu 24.04, GNOME 46)

Verified on 2026-10-25 on Zorin OS 18.1.

### 1. System packages

The distribution's Qt (6.4) is too old and is not used. The prebuilt Qt below needs a compiler, build tools and a few system headers:

```bash
sudo apt install build-essential cmake ninja-build libgl-dev libegl-dev libopengl-dev libxkbcommon-dev libvulkan-dev libxcb-cursor0 libwayland-dev
```

`libwayland-dev` (with `wayland-scanner`) is for the Wayland backend (D-031). KDE's LayerShellQt is optional: with it, the capture overlay on Hyprland and Sway is a layer surface above all windows, without it a full-screen window. Ubuntu 24.04 only has it for Qt 5, so the Qt 6.8 build here runs without; the Nix package has it.

Without the OpenGL headers CMake reports "Could NOT find OpenGL" and then fails to find `Qt6Gui` and `Qt6Widgets`. `libxcb-cursor0` is needed by Qt's X11 platform plugin (Xwayland fallback).

### 2. Qt 6.8 LTS

Qt is installed per user into `~/Qt` with [aqtinstall](https://github.com/miurahr/aqtinstall), without a Qt account and without touching the system:

```bash
python3 -m venv ~/Qt/aqt-venv
~/Qt/aqt-venv/bin/pip install aqtinstall
~/Qt/aqt-venv/bin/aqt install-qt linux desktop 6.8.3 linux_gcc_64 -m qtimageformats -O ~/Qt
```

- 6.8.3 is the newest open-source release of 6.8 LTS; later 6.8 patch releases are only available commercially.
- `qtimageformats` adds WebP (used for import, F-06).
- Size: about 1.6 GB.

### 3. Configure and build

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/nsl-snap
```

### Continuous integration

GitHub Actions (`.github/workflows/ci.yml`) runs on every pull request and on `main`:

- **Build and test:** Ubuntu 24.04, Qt 6.8.3 via aqtinstall (`jurplel/install-qt-action`, cached), Debug build with `-Wall -Wextra` and warnings as errors, then `ctest` with `QT_QPA_PLATFORM=offscreen`.
- **Windows x64 and ARM64:** `windows-2022` and `windows-11-arm`, Qt 6.8.3 via aqtinstall per architecture (cached), MSVC for the runner's own architecture, Debug build with warnings as errors at MSVC's default level, then `ctest` offscreen. There is no Windows capture backend yet, so this covers the editor and the shared core.
- **Extension and tools:** `node --check` on `.mjs` copies of the extension files, `py_compile` of `tools/nsl-snap-pick`, `bash -n` of the scripts.

The same build locally, warnings as errors:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64 -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
```

The GNOME capture itself (extension in a running shell) is not tested in CI; it needs a session (`scripts/nested-shell.sh`). Neither is the Wayland backend's capture (D-031); it can be tried in a headless Sway, see below.

### 4. GNOME Shell extension

See [06-poc-gnome-window-capture.md](06-poc-gnome-window-capture.md): `scripts/dev-install.sh`, then log out and in. The script also compiles the extension's settings schema (`glib-compile-schemas`, from `libglib2.0-bin`, installed with GNOME); run it again after pulling changes to `gnome-extension/*/schemas/`, otherwise the extension does not start.

### Smoke test result (2026-10-25)

A minimal Qt program built with this setup reported:

- Qt 6.8.3, platform plugin `wayland`
- NSL Snap extension reachable over D-Bus, `Version` = 1
- WebP supported
- No missing libraries for the Wayland and X11 platform plugins

### Trying the Wayland backend in a headless Sway

Sway runs without a screen and offers wlr-screencopy and layer-shell, so capture can be tried in any Linux session or container (2026-10-27). Hyprland's window capture needs a real Hyprland.

```bash
sudo apt install sway grim wtype
export XDG_RUNTIME_DIR=$(mktemp -d) && chmod 700 $XDG_RUNTIME_DIR
WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman dbus-run-session -- sway &
export WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland
./build/nsl-snap &
wtype -k Shift_L -M ctrl -k 5 -m ctrl   # region capture; arrow keys and Enter select
grim screen.png                          # what the compositor shows
```

- `swaymsg output HEADLESS-2 scale 2` gives mixed scales.
- wtype's first key after connecting is sometimes lost; the leading `Shift_L` absorbs it.
- Headless Sway has no pointer; wlrctl's virtual pointer did not reach the app. Use the keyboard (D-023).
- The portal backend (D-032) needs `xdg-desktop-portal`, `xdg-desktop-portal-wlr`, `pipewire` and an Access portal such as `xdg-desktop-portal-gtk`, plus `~/.config/xdg-desktop-portal/sway-portals.conf` with `[preferred]`, `default=wlr;gtk`.

## NixOS (Hyprland)

The repository is a Nix flake (`flake.nix`, `nix/package.nix`). It builds with nixpkgs' own Qt (6.8 or later; tested with 6.12 on 2026-10-27), with LayerShellQt, and wraps the binary with the GTK schema paths (D-033):

```bash
nix run github:michaelpfister/ch-nsl-snap        # or, in a checkout:
nix build && ./result/bin/nsl-snap
nix develop                                      # shell with all build tools, then cmake as above
```

In a NixOS configuration: add the flake as an input and `inputs.nsl-snap.packages.${pkgs.system}.default` to `environment.systemPackages`.

Hyprland settings that matter (hyprland.conf):

- With `ecosystem { enforce_permissions = true }` (off by default) Hyprland asks at the first capture, or a rule allows it: `permission = .*nsl-snap.*, screencopy, allow` (a regular expression on the binary's path; on NixOS the binary is `/nix/store/…-nsl-snap-…/bin/.nsl-snap-wrapped`). Newer Hyprland versions with the Lua configuration write `hl.permission({ binary = ".*nsl-snap.*", type = "screencopy", mode = "allow" })`. Rules are read at Hyprland's start only.
- A key for capturing (F-42 is not built yet): `bind = , Print, exec, nsl-snap`.

## Windows 11 (x64 and ARM64)

There is no Windows capture backend yet: "Fenster" and "Bereich" say that capturing is not supported on this platform. The editor works: paste a capture with Ctrl+V (e.g. from Win + Shift + S), drag an image onto the window, or open it with Ctrl+O. The same steps run in CI on both architectures (green since 2026-10-26).

### 1. Visual Studio 2022 Build Tools

Install the [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) (or Visual Studio 2022) with the workload **Desktop development with C++**. On an ARM64 machine also select the individual component **MSVC v143 – VS 2022 C++ ARM64 build tools (latest)**. CMake and Ninja come with it.

### 2. Qt 6.8 LTS

With aqtinstall, as on Linux (PowerShell):

```powershell
py -m venv C:\Qt\aqt-venv
C:\Qt\aqt-venv\Scripts\pip install aqtinstall
# ARM64:
C:\Qt\aqt-venv\Scripts\aqt install-qt windows_arm64 desktop 6.8.3 win64_msvc2022_arm64 -m qtimageformats -O C:\Qt
# x64:
C:\Qt\aqt-venv\Scripts\aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtimageformats -O C:\Qt
```

Qt ends up in `C:\Qt\6.8.3\msvc2022_arm64` or `C:\Qt\6.8.3\msvc2022_64`.

### 3. Configure, build, run

Open the **ARM64 Native Tools Command Prompt for VS 2022** (or the x64 one on an x64 machine), so the compiler for the machine's own architecture is active:

```bat
cd C:\repo\vscode\ch-nsl-snap
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_arm64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build
set PATH=C:\Qt\6.8.3\msvc2022_arm64\bin;%PATH%
ctest --test-dir build --output-on-failure
build\nsl-snap.exe
```

The `PATH` line lets the app and the tests find the Qt DLLs. The tests draw offscreen; on Windows that platform looks for fonts in Qt's own `lib\fonts` folder, which Qt does not ship, so `ctest` points `QT_QPA_FONTDIR` to `C:\Windows\Fonts` (`tests/CMakeLists.txt`). Set it yourself when starting a test program directly. Alternatively `windeployqt build\nsl-snap.exe` copies them next to the executable, so it also starts with a double click.

