# Development setup

How to set up a machine to build NSL Snap (D-013: C++20, Qt 6.8 LTS, CMake). Only Linux is described so far; Windows and macOS follow when their backends are built.

## Linux (Zorin OS 18 / Ubuntu 24.04, GNOME 46)

Verified on 2026-10-25 on Zorin OS 18.1.

### 1. System packages

The distribution's Qt (6.4) is too old and is not used. The prebuilt Qt below needs a compiler, build tools and a few system headers:

```bash
sudo apt install build-essential cmake ninja-build libgl-dev libegl-dev libopengl-dev libxkbcommon-dev libvulkan-dev libxcb-cursor0
```

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
```

### 4. GNOME Shell extension

See [06-poc-gnome-window-capture.md](06-poc-gnome-window-capture.md): `scripts/dev-install.sh`, then log out and in.

### Smoke test result (2026-10-25)

A minimal Qt program built with this setup reported:

- Qt 6.8.3, platform plugin `wayland`
- NSL Snap extension reachable over D-Bus, `Version` = 1
- WebP supported
- No missing libraries for the Wayland and X11 platform plugins
