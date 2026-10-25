#!/usr/bin/env bash
# Starts a nested GNOME Shell in a window, with its own session bus, for
# testing the extension without logging out. Run dev-install.sh first.
# Inside the nested shell, open a terminal and run tools/nsl-snap-pick there.
set -euo pipefail

export MUTTER_DEBUG_DUMMY_MODE_SPECS="${MUTTER_DEBUG_DUMMY_MODE_SPECS:-1600x1000}"
exec dbus-run-session -- gnome-shell --nested --wayland
