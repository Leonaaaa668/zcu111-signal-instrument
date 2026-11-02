#!/bin/sh
# Run a relocatable SDK-built bundle from the existing Weston user's session.
set -eu
if [ "$(id -u)" = 0 ]; then
    echo 'Run as the normal Weston user (petalinux), not root.' >&2
    exit 2
fi
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export XDG_RUNTIME_DIR="/run/user/$(id -u)"
if [ -n "${WAYLAND_DISPLAY:-}" ] && [ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]; then
    unset WAYLAND_DISPLAY
fi
if [ -z "${WAYLAND_DISPLAY:-}" ]; then
    for candidate in "$XDG_RUNTIME_DIR"/wayland-[0-9]*; do
        if [ -S "$candidate" ]; then
            WAYLAND_DISPLAY=${candidate##*/}
            export WAYLAND_DISPLAY
            break
        fi
    done
fi
if [ -z "${WAYLAND_DISPLAY:-}" ] || [ ! -S "$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY" ]; then
    echo 'No Weston socket for the current user; start Weston and use its user session.' >&2
    exit 2
fi
export QT_QPA_PLATFORM=wayland
exec "$BASE/usr/bin/zcu111-instrument" --data "$BASE/usr/share/zcu111-instrument" --fullscreen "$@"
