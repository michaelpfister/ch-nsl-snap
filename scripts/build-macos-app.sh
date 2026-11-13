#!/usr/bin/env bash
# Builds the macOS package: Snapslash.app with Qt inside, as a disk image
# (D-046). Used by the CI workflow on a macOS runner; it also runs on a Mac
# with Xcode's command line tools, CMake and Ninja.
#
# Usage: scripts/build-macos-app.sh [output.dmg]
#
# Environment:
#   QT_DIR          Qt 6.8.3 for macOS (default: ~/Qt/6.8.3/macos)
#   VERSION_SUFFIX  pre-release part of the version, e.g. beta.1 (default: none)
#   ARCHITECTURES   CPU architectures of the program (default: arm64;x86_64,
#                   one universal program for Apple silicon and Intel Macs)
#   WORK            build folder (default: build-macos)
#
# The app is signed ad hoc only (no Apple Developer ID yet, Q-36): macOS
# refuses to open it from a download until the user allows it once.
set -euo pipefail

repo=$(cd "$(dirname "$0")/.." && pwd)
QT_DIR=${QT_DIR:-$HOME/Qt/6.8.3/macos}
VERSION_SUFFIX=${VERSION_SUFFIX:-}
ARCHITECTURES=${ARCHITECTURES:-arm64;x86_64}
WORK=$(mkdir -p "${WORK:-$repo/build-macos}" && cd "${WORK:-$repo/build-macos}" && pwd)

version=$(sed -n 's/^project(Snapslash VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt")
[ -n "$VERSION_SUFFIX" ] && version="$version-$VERSION_SUFFIX"
output=${1:-$WORK/snapslash-$version-macos.dmg}

if [ ! -x "$QT_DIR/bin/macdeployqt" ]; then
    echo "No Qt in $QT_DIR (set QT_DIR)" >&2
    exit 1
fi

# --- Build ----------------------------------------------------------------
cmake -S "$repo" -B "$WORK/build" -G Ninja \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHITECTURES" \
    -DNSL_VERSION_SUFFIX="$VERSION_SUFFIX"
cmake --build "$WORK/build" --target snapslash

# --- Bundle ---------------------------------------------------------------
stage="$WORK/stage"
rm -rf "$stage"
mkdir -p "$stage"
cp -R "$WORK/build/Snapslash.app" "$stage/"
app="$stage/Snapslash.app"

# Qt's frameworks and plugins (with Qt's FFmpeg for videos) into the bundle.
"$QT_DIR/bin/macdeployqt" "$app" -verbose=1

# Qt's own German texts (message box buttons and the like); the app looks
# for them in Contents/Resources/translations (Language::install).
mkdir -p "$app/Contents/Resources/translations"
for name in qt_de qtbase_de qtmultimedia_de; do
    if [ -f "$QT_DIR/translations/$name.qm" ]; then
        cp "$QT_DIR/translations/$name.qm" "$app/Contents/Resources/translations/"
    fi
done

# macdeployqt changes the libraries, which breaks their signatures; Apple
# silicon runs no unsigned code. Sign the whole bundle again, ad hoc.
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict --verbose=2 "$app"

# The program must start and report its version (no display needed).
reported=$("$app/Contents/MacOS/Snapslash" --version)
echo "$reported"
if [ "$reported" != "Snapslash $version" ]; then
    echo "Expected \"Snapslash $version\"" >&2
    exit 1
fi
lipo -info "$app/Contents/MacOS/Snapslash"

# --- Disk image -----------------------------------------------------------
# The app and a link to /Applications, to drag it there.
ln -s /Applications "$stage/Applications"
rm -f "$output"
hdiutil create -volname "Snapslash $version" -srcfolder "$stage" -fs HFS+ -format UDZO "$output"
echo "Built $output"
