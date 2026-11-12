#!/usr/bin/env bash
# Builds the Linux package: one AppImage with Qt, Qt's FFmpeg, LayerShellQt
# and the GNOME extension inside (D-041). Used by the AppImage workflow; it
# also runs on a development machine (docs/07-development-setup.md,
# "AppImage").
#
# Usage: scripts/build-appimage.sh [output.AppImage]
#
# Needs, besides the build tools: glib-compile-schemas (libglib2.0-bin) and
# rsvg-convert (librsvg2-bin).
#
# Environment:
#   QT_DIR          Qt 6.8.3 for gcc_64 (default: ~/Qt/6.8.3/gcc_64)
#   VERSION_SUFFIX  pre-release part of the version, e.g. beta.1 (default: none)
#   WORK            folder for downloads and builds (default: build-appimage)
#
# The tools and sources it downloads have fixed versions and SHA-256 sums;
# the build stops when one does not match.
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
QT_DIR=${QT_DIR:-$HOME/Qt/6.8.3/gcc_64}
VERSION_SUFFIX=${VERSION_SUFFIX:-}
WORK=$(mkdir -p "${WORK:-$repo/build-appimage}" && cd "${WORK:-$repo/build-appimage}" && pwd)

version=$(sed -n 's/^project(Snapslash VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt")
[ -n "$VERSION_SUFFIX" ] && version="$version-$VERSION_SUFFIX"
output=${1:-$WORK/snapslash-$version-linux-x86_64.AppImage}

if [ ! -x "$QT_DIR/bin/qmake" ]; then
    echo "No Qt in $QT_DIR (set QT_DIR)" >&2
    exit 1
fi

# The downloaded AppImages run without FUSE: they unpack themselves into
# /tmp first. Build containers and CI runners often have no /dev/fuse.
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64

# --- Downloads ------------------------------------------------------------

# download <url> <sha256> <file>: fetches once, then checks the sum.
download() {
    local url=$1 sum=$2 file=$3
    if [ ! -f "$file" ]; then
        echo "Downloading $url"
        curl -fsSL --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    if ! echo "$sum  $file" | sha256sum --check --status; then
        echo "Wrong SHA-256 for $file (from $url)" >&2
        rm -f "$file"
        exit 1
    fi
}

tools=$WORK/tools
mkdir -p "$tools"
gh=https://github.com
download "$gh/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage" \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d "$tools/linuxdeploy-x86_64.AppImage"
download "$gh/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage" \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724 "$tools/linuxdeploy-plugin-qt-x86_64.AppImage"
download "$gh/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage" \
    ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 "$tools/appimagetool-x86_64.AppImage"
# The static runtime: needs no libfuse2 on the user's system (Q-33).
download "$gh/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64" \
    2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d "$tools/runtime-x86_64"
chmod +x "$tools"/*.AppImage

kde=https://download.kde.org/stable
download "$kde/frameworks/6.10/extra-cmake-modules-6.10.0.tar.xz" \
    506989a0d400913403e669c1912238db053cd6b38dff74b17e2e6f879c79cca0 "$WORK/extra-cmake-modules-6.10.0.tar.xz"
download "$kde/plasma/6.3.5/layer-shell-qt-6.3.5.tar.xz" \
    7062ca0e50dcce97dd11d8adf1b86d49f1c56423f2341dceb3ea40dc48206e25 "$WORK/layer-shell-qt-6.3.5.tar.xz"

# --- LayerShellQt ---------------------------------------------------------

# KDE's LayerShellQt, built against our Qt (distributions build it against
# their own Qt): overlay and indicator become layer surfaces on Hyprland and
# Sway (Q-34). It needs KDE's CMake modules (ECM), newer than Ubuntu's.
deps=$WORK/deps
if [ ! -f "$deps/lib/libLayerShellQtInterface.so.6" ]; then
    rm -rf "$WORK/src" && mkdir -p "$WORK/src"
    tar -xJf "$WORK/extra-cmake-modules-6.10.0.tar.xz" -C "$WORK/src"
    tar -xJf "$WORK/layer-shell-qt-6.3.5.tar.xz" -C "$WORK/src"
    cmake -S "$WORK/src/extra-cmake-modules-6.10.0" -B "$WORK/src/ecm-build" -G Ninja \
        -DCMAKE_INSTALL_PREFIX="$deps" -DBUILD_TESTING=OFF -DBUILD_DOC=OFF
    cmake --build "$WORK/src/ecm-build" && cmake --install "$WORK/src/ecm-build"
    # CMAKE_INSTALL_LIBDIR=lib: the same path on every distribution.
    cmake -S "$WORK/src/layer-shell-qt-6.3.5" -B "$WORK/src/lsq-build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$deps" -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_PREFIX_PATH="$QT_DIR;$deps" -DBUILD_TESTING=OFF
    cmake --build "$WORK/src/lsq-build" && cmake --install "$WORK/src/lsq-build"
fi

# --- The app --------------------------------------------------------------

cmake -S "$repo" -B "$WORK/app" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_PREFIX_PATH="$QT_DIR;$deps" -DNSL_VERSION_SUFFIX="$VERSION_SUFFIX"
cmake --build "$WORK/app" --target snapslash
if ! readelf -d "$WORK/app/snapslash" | grep -q 'libLayerShellQtInterface'; then
    echo "The app was built without LayerShellQt" >&2
    exit 1
fi

appdir=$WORK/AppDir
rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$WORK/app"

# The GNOME extension of the same version, with its compiled settings
# schema; the app installs it from here (D-041).
extension=$appdir/usr/share/snapslash/gnome-extension
mkdir -p "$extension"
cp -r "$repo/gnome-extension/nsl-snap@netslash.ch" "$extension/"
glib-compile-schemas "$extension/nsl-snap@netslash.ch/schemas"

# --- Libraries and plugins ------------------------------------------------

desktop=$appdir/usr/share/applications/ch.netslash.Snapslash.desktop
icon=$appdir/usr/share/icons/hicolor/scalable/apps/ch.netslash.Snapslash.svg

# linuxdeploy copies the libraries the app needs into usr/lib and points the
# binaries at them (RUNPATH $ORIGIN/../lib); its Qt plugin adds Qt's plugins
# (platforms, image formats, multimedia with Qt's FFmpeg, TLS, ...).
# Not copied:
#  - libpipewire: it loads its modules from a path compiled into it, which
#    differs between distributions (/usr/lib64 on Fedora). Every desktop
#    with GNOME 46 has it; the app records no GNOME videos without it.
#  - libva (VA-API, the H.264 hardware encoder): Qt's FFmpeg loads it from
#    the system at run time, with the graphics driver that matches it.
#  - glibc, libGL, libX11, libwayland-client, fontconfig and the like:
#    linuxdeploy never copies them (its exclude list), the system has them.
export QMAKE=$QT_DIR/bin/qmake
export LD_LIBRARY_PATH=$deps/lib:$QT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --plugin qt \
    --exclude-library 'libpipewire-0.3.so*' \
    --desktop-file "$desktop" --icon-file "$icon"

# The Wayland plugins, which this version of the Qt plugin does not pick
# (the platform plugin alone starts no window): the xdg-shell for windows,
# the window decorations GNOME needs (it draws none for Qt), and the EGL
# integration. Then their own libraries. Layer surfaces need no plugin:
# libLayerShellQtInterface, which the app links, brings that shell along.
plugins=$appdir/usr/plugins
mkdir -p "$plugins/wayland-shell-integration" "$plugins/wayland-decoration-client" \
    "$plugins/wayland-graphics-integration-client"
cp "$QT_DIR/plugins/wayland-shell-integration/libxdg-shell.so" "$plugins/wayland-shell-integration/"
cp "$QT_DIR/plugins/wayland-decoration-client/"*.so "$plugins/wayland-decoration-client/"
cp "$QT_DIR/plugins/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so" \
    "$plugins/wayland-graphics-integration-client/"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --exclude-library 'libpipewire-0.3.so*' \
    --deploy-deps-only "$plugins/wayland-shell-integration" \
    --deploy-deps-only "$plugins/wayland-decoration-client" \
    --deploy-deps-only "$plugins/wayland-graphics-integration-client"

if ls "$appdir/usr/lib" | grep -q '^libpipewire'; then
    echo "libpipewire ended up in the AppImage" >&2
    exit 1
fi

# --- The AppImage ---------------------------------------------------------

# The AppImage's own icon, which file managers show for the file (through
# a thumbnailer): .DirIcon, a PNG by the AppImage specification. linuxdeploy
# links it to the SVG, which thumbnailers do not read.
png=$appdir/usr/share/icons/hicolor/256x256/apps/ch.netslash.Snapslash.png
mkdir -p "$(dirname "$png")"
rsvg-convert --width 256 --height 256 "$repo/resources/icons/logo.svg" --output "$png"
ln -sfn "usr/share/icons/hicolor/256x256/apps/ch.netslash.Snapslash.png" "$appdir/.DirIcon"

rm -f "$output"
"$tools/appimagetool-x86_64.AppImage" --no-appstream --runtime-file "$tools/runtime-x86_64" \
    "$appdir" "$output"

# Smoke test: the packed app starts and reports its version.
reported=$(QT_QPA_PLATFORM=offscreen "$output" --version 2>/dev/null)
if [ "$reported" != "Snapslash $version" ]; then
    echo "The AppImage reports '$reported', expected 'Snapslash $version'" >&2
    exit 1
fi
echo "Built $output ($reported)"
