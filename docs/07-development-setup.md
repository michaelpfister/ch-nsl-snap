# Development setup

How to set up a machine to build Snapslash (D-013: C++20, Qt 6.8 LTS, CMake). Linux (Ubuntu/Zorin and NixOS) and Windows are described; macOS follows when its backend is built.

## Linux (Zorin OS 18 / Ubuntu 24.04, GNOME 46)

Verified on 2026-10-25 on Zorin OS 18.1.

### 1. System packages

The distribution's Qt (6.4) is too old and is not used. The prebuilt Qt below needs a compiler, build tools and a few system headers:

```bash
sudo apt install build-essential cmake ninja-build libgl-dev libegl-dev libopengl-dev libxkbcommon-dev libvulkan-dev libxcb-cursor0 libwayland-dev libpipewire-0.3-dev
```

`libwayland-dev` (with `wayland-scanner`) is for the Wayland backend (D-031). `libpipewire-0.3-dev` is for videos on GNOME (D-039); without it the app builds, but records no videos there. KDE's LayerShellQt is optional: with it, the capture overlay on Hyprland and Sway is a layer surface above all windows, without it a full-screen window. Ubuntu 24.04 only has it for Qt 5, so the Qt 6.8 build here runs without; the Nix package has it.

Without the OpenGL headers CMake reports "Could NOT find OpenGL" and then fails to find `Qt6Gui` and `Qt6Widgets`. `libxcb-cursor0` is needed by Qt's X11 platform plugin (Xwayland fallback).

### 2. Qt 6.8 LTS

Qt is installed per user into `~/Qt` with [aqtinstall](https://github.com/miurahr/aqtinstall), without a Qt account and without touching the system:

```bash
python3 -m venv ~/Qt/aqt-venv
~/Qt/aqt-venv/bin/pip install aqtinstall
~/Qt/aqt-venv/bin/aqt install-qt linux desktop 6.8.3 linux_gcc_64 -m qtimageformats qtmultimedia -O ~/Qt
```

- 6.8.3 is the newest open-source release of 6.8 LTS; later 6.8 patch releases are only available commercially.
- `qtimageformats` adds WebP (used for import, F-06).
- `qtmultimedia` encodes videos (F-50, D-039), with Qt's own FFmpeg. On Linux that FFmpeg encodes H.264 only with the graphics card (VA-API or NVENC, Q-28). An existing Qt installation gets it with the same command; aqt adds the module.
- Size: about 1.6 GB.

### 3. Configure and build

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64 -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/snapslash
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
- Snapslash extension reachable over D-Bus, `Version` = 1
- WebP supported
- No missing libraries for the Wayland and X11 platform plugins

### Trying the Wayland backend in a headless Sway

Sway runs without a screen and offers wlr-screencopy and layer-shell, so capture can be tried in any Linux session or container (2026-10-27). Hyprland's window capture needs a real Hyprland.

```bash
sudo apt install sway grim wtype
export XDG_RUNTIME_DIR=$(mktemp -d) && chmod 700 $XDG_RUNTIME_DIR
WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_LIBINPUT_NO_DEVICES=1 WLR_RENDERER=pixman dbus-run-session -- sway &
export WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland
./build/snapslash &
wtype -k Shift_L -M ctrl -k 5 -m ctrl   # region capture; arrow keys and Enter select
grim screen.png                          # what the compositor shows
```

- `swaymsg output HEADLESS-2 scale 2` gives mixed scales.
- wtype's first key after connecting is sometimes lost; the leading `Shift_L` absorbs it.
- Headless Sway has no pointer; wlrctl's virtual pointer did not reach the app. Use the keyboard (D-023).
- Videos (F-50): `snapslash --record-screen`, then Enter in the overlay (all monitors) or a click; the same command again stops it. Qt's FFmpeg finds no H.264 encoder in a headless Sway (no graphics card), so the app refuses there; the controller with MPEG-4 can be driven from a small test program (`RecordingController::setVideoCodec`). `NSL_TEST_H264=1 ctest -R tst_recording` tries the machine's H.264 encoder.
- The portal backend (D-032) needs `xdg-desktop-portal`, `xdg-desktop-portal-wlr`, `pipewire` and an Access portal such as `xdg-desktop-portal-gtk`, plus `~/.config/xdg-desktop-portal/sway-portals.conf` with `[preferred]`, `default=wlr;gtk`.

### Trying GNOME videos in a headless GNOME Shell

GNOME Shell 46 runs without a screen (`--headless`), so the extension's pick and Mutter's screen cast can be tried in a container (2026-11-11, D-039). It needs `gnome-shell`, `xwayland`, `pipewire`, `wireplumber` and, for a test window and the scripted keys, `python3-gi` with `gir1.2-gtk-3.0` and `python3-gi-cairo`.

- The Shell needs a system bus (`dbus-daemon --system --fork`) and, without systemd-logind, no `/run/systemd/seats` (then it uses its dummy login manager).
- Inside `dbus-run-session`: link or copy the extension, `gsettings set org.gnome.shell enabled-extensions "['nsl-snap@netslash.ch']"` (with the same `HOME` as the D-Bus daemon, which starts dconf), start `pipewire` and `wireplumber`, then `gnome-shell --headless --wayland --virtual-monitor 1280x720`.
- Keys and pointer for the overlay come from Mutter's own `org.gnome.Mutter.RemoteDesktop` (`CreateSession`, `Start`, `NotifyPointerMotionRelative`, `NotifyKeyboardKeysym`).
- The Shell starts in the overview; a maximized test window covers the middle of the screen.

### Translations

The texts in the code are German; English is in `translations/snapslash_en.ts` (D-038). After adding or changing a text:

```bash
scripts/update-translations.sh
```

The new texts appear in the file as `type="unfinished"`: translate them in Qt Linguist (`~/Qt/6.8.3/gcc_64/bin/linguist translations/snapslash_en.ts`) or directly in the file, then commit it with the change. CI fails while the file is out of date or a text is untranslated. To try the English interface: settings, "Allgemein", "Sprache", then restart the app.

### Building the AppImage

The Linux package (D-041) is built by one script; CI runs it on Ubuntu 22.04 for every pull request (`.github/workflows/appimage.yml`, the AppImage is the run's artifact `appimage-x86_64`) and for every release. On a development machine, with the packages of step 1 and `libglib2.0-bin`:

```bash
QT_DIR=$HOME/Qt/6.8.3/gcc_64 scripts/build-appimage.sh
```

- It downloads linuxdeploy, its Qt plugin, appimagetool and the static AppImage runtime, plus the sources of KDE's ECM and LayerShellQt, each with a fixed version and SHA-256 sum, into `build-appimage/`. LayerShellQt is built once against this Qt; delete `build-appimage/deps` to build it again.
- The result is `build-appimage/snapslash-<version>-linux-x86_64.AppImage` (`VERSION_SUFFIX=beta.1` for a pre-release). The script checks that it starts and reports its version.
- libpipewire and libva are not in the AppImage, they come from the system (D-041). An AppImage built on Ubuntu 24.04 needs glibc 2.39; the one from CI runs from glibc 2.35 on.
- The GNOME extension is inside, under `usr/share/snapslash/gnome-extension`. Started on GNOME, the AppImage offers to install it (D-041); it replaces a link made by `scripts/dev-install.sh`, so for working on the extension run that script again afterwards.
- Each start of the AppImage writes `~/.local/share/applications/ch.netslash.Snapslash.desktop`, the icon and the link `~/.local/bin/snapslash`. That entry wins over one from `cmake --install --prefix ~/.local`; delete it to go back.
- Without FUSE (containers) the AppImage unpacks itself first: `--appimage-extract-and-run` or `APPIMAGE_EXTRACT_AND_RUN=1`.

## Installing on Linux

For using Snapslash rather than working on it, on a Linux without Nix (Ubuntu, Zorin OS, …): the AppImage of a release sets itself up (README, D-041). To build a release from source instead and install it for your user, with the system packages and Qt of steps 1 and 2 above:

```bash
git clone https://github.com/michaelpfister/ch-nsl-snap.git ~/snapslash
cd ~/snapslash
git checkout v1.0.0
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix ~/.local
scripts/dev-install.sh
```

- For a pre-release add `-DNSL_VERSION_SUFFIX=beta.1`, the part of the tag after the `-`; leave it out for a release without one. The app then reports the version it was built from, which its update check compares with the releases (D-037).
- `cmake --install` puts `snapslash` into `~/.local/bin`, the desktop entry and the icon into `~/.local/share`, so the app shows up among the applications with its quick actions (D-036). The installed program keeps the path to Qt in `~/Qt` (`CMAKE_INSTALL_RPATH_USE_LINK_PATH`, on by default).
- `scripts/dev-install.sh` links the GNOME extension from the checkout, so keep the folder. Log out and in again afterwards: GNOME Shell only loads new extensions at login. A login also puts `~/.local/bin` on the `PATH` if the folder did not exist before.
- Another version later: the same steps with its tag; the app's settings tab "Updates" shows them for the chosen version.

## NixOS (Hyprland)

The repository is a Nix flake (`flake.nix`, `nix/package.nix`). It builds with nixpkgs' own Qt (6.8 or later; tested with 6.12 on 2026-10-27), with LayerShellQt, and wraps the binary with the GTK schema paths (D-033):

```bash
nix run github:michaelpfister/ch-nsl-snap        # or, in a checkout:
nix build && ./result/bin/snapslash
nix develop                                      # shell with all build tools, then cmake as above
```

In a NixOS configuration: add the flake as an input and `inputs.snapslash.packages.${pkgs.system}.default` to `environment.systemPackages`.

Hyprland settings that matter (hyprland.conf):

- With `ecosystem { enforce_permissions = true }` (off by default) Hyprland asks at the first capture, or a rule allows it: `permission = .*snapslash.*, screencopy, allow` (a regular expression on the binary's path; on NixOS the binary is `/nix/store/…-snapslash-…/bin/.snapslash-wrapped`). Newer Hyprland versions with the Lua configuration write `hl.permission({ binary = ".*snapslash.*", type = "screencopy", mode = "allow" })`. Rules are read at Hyprland's start only.
- Keys for capturing, through the command line (F-48, D-034; `snapslash --help` lists the options):

  ```
  exec-once = snapslash                                  # optional: keep it in the tray
  bind = , Print, exec, snapslash --capture-region       # region, saved to the save folder
  bind = SHIFT, Print, exec, snapslash --capture-screen
  bind = ALT, Print, exec, snapslash --capture-window --open   # window, into the editor
  bind = SUPER, Print, exec, snapslash --record-window        # video; the same key again stops it
  ```

## Releasing

A release is a version tag; `.github/workflows/release.yml` does the rest (D-037):

1. Set the version in `CMakeLists.txt` (`project(Snapslash VERSION 0.2.0 ...)`) and in `nix/package.nix` (`version = "0.2.0";`). For a pre-release `nix/package.nix` carries the whole version (`"0.3.0-beta.1"`), CMake only the numbers (`0.3.0`).
2. Commit, merge into `main`, then tag that commit and push the tag:

   ```bash
   git tag v0.2.0
   git push origin v0.2.0
   ```

3. The workflow checks the tag against both versions, builds the Windows ZIPs (x64, ARM64) and the Linux AppImage, writes `SHA256SUMS.txt` and creates the release with notes generated from the merged pull requests. A tag with `-` (`v0.3.0-beta.1`) becomes a pre-release, which the app offers only with the setting "Auch Vorabversionen anbieten".
4. Edit the notes on GitHub if needed: the app shows them in its update dialog (Markdown).

To try an update before the first real one: release a pre-release, start the ZIP of an older pre-release, and enable pre-releases in the settings.

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
C:\Qt\aqt-venv\Scripts\aqt install-qt windows_arm64 desktop 6.8.3 win64_msvc2022_arm64 -m qtimageformats qtmultimedia -O C:\Qt
# x64:
C:\Qt\aqt-venv\Scripts\aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtimageformats qtmultimedia -O C:\Qt
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
build\snapslash.exe
```

The `PATH` line lets the app and the tests find the Qt DLLs. The tests draw offscreen; on Windows that platform looks for fonts in Qt's own `lib\fonts` folder, which Qt does not ship, so `ctest` points `QT_QPA_FONTDIR` to `C:\Windows\Fonts` (`tests/CMakeLists.txt`). Set it yourself when starting a test program directly. Alternatively `windeployqt build\snapslash.exe` copies them next to the executable, so it also starts with a double click. The Qt DLLs in `build` then look for plugins there and find only `platforms\qwindows`, so the offscreen tests stop with a "no Qt platform plugin" dialog; set `QT_PLUGIN_PATH=C:\Qt\6.8.3\msvc2022_arm64\plugins` before `ctest`.

