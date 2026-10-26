#!/usr/bin/env bash
# Starts a nested GNOME Shell in a window, with its own session bus, for
# testing the extension without logging out. Run dev-install.sh first.
# Inside the nested shell, open a terminal and run tools/nsl-snap-pick or
# the app there.
#
# The nested shell gets its own settings folder: dconf is started on the
# nested bus, and without this it would write your real GNOME settings
# (the extension changes GNOME's screenshot keys, D-031).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
SCHEMAS="$REPO/gnome-extension/nsl-snap@netslash.ch/schemas"
APP="$(realpath -m "${NSL_SNAP_APP:-$REPO/build/nsl-snap}")"
SCRATCH="$(mktemp -d -t nsl-snap-nested.XXXXXX)"
export XDG_CONFIG_HOME="$SCRATCH/config"
mkdir -p "$XDG_CONFIG_HOME"
echo "Settings of the nested shell: $XDG_CONFIG_HOME (deleted afterwards)"
trap 'rm -rf "$SCRATCH"' EXIT

export MUTTER_DEBUG_DUMMY_MODE_SPECS="${MUTTER_DEBUG_DUMMY_MODE_SPECS:-1600x1000}"
dbus-run-session -- bash -c '
    gsettings set org.gnome.shell enabled-extensions "[\"nsl-snap@netslash.ch\"]"
    gsettings --schemadir "$1" set org.gnome.shell.extensions.nsl-snap app-executable "$2"
    exec gnome-shell --nested --wayland' _ "$SCHEMAS" "$APP"
