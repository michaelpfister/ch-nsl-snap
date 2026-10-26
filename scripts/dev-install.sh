#!/usr/bin/env bash
# Links the extension into the user's extension directory and enables it.
# On Wayland, GNOME Shell only discovers new extensions after logging out and
# in again; after that, edits only need a disable/enable or a re-login.
set -euo pipefail

UUID="nsl-snap@netslash.ch"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$REPO/gnome-extension/$UUID"
DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
DEST="$DATA/gnome-shell/extensions/$UUID"
# The app that gets hotkey captures (F-42); NSL_SNAP_APP overrides it.
APP="$(realpath -m "${NSL_SNAP_APP:-$REPO/build/nsl-snap}")"

# The settings schema (keybindings) has to be compiled next to it.
glib-compile-schemas --strict "$SRC/schemas"

# Hotkey captures (D-031): the extension gives them only to this program,
# and D-Bus starts it when it is not running.
gsettings --schemadir "$SRC/schemas" set org.gnome.shell.extensions.nsl-snap app-executable "$APP"
mkdir -p "$DATA/dbus-1/services"
cat > "$DATA/dbus-1/services/ch.netslash.Snap.service" <<SERVICE
[D-BUS Service]
Name=ch.netslash.Snap
Exec=$APP
SERVICE
echo "Hotkey captures go to $APP"
[ -x "$APP" ] || echo "  (not built yet: cmake --build build)"

mkdir -p "$(dirname "$DEST")"
if [ -L "$DEST" ] || [ ! -e "$DEST" ]; then
    ln -sfn "$SRC" "$DEST"
    echo "Linked $DEST -> $SRC"
else
    echo "$DEST exists and is not a symlink, leaving it alone." >&2
    exit 1
fi

if gnome-extensions info "$UUID" >/dev/null 2>&1; then
    gnome-extensions enable "$UUID"
    echo "Enabled $UUID."
else
    # Not discovered yet: mark it enabled so it starts after the next login.
    current="$(gsettings get org.gnome.shell enabled-extensions)"
    if [[ "$current" != *"'$UUID'"* ]]; then
        if [[ "$current" == "@as []" || "$current" == "[]" ]]; then
            new="['$UUID']"
        else
            new="${current%]}, '$UUID']"
        fi
        gsettings set org.gnome.shell enabled-extensions "$new"
    fi
    echo "Log out and in again, then run tools/nsl-snap-pick."
fi
