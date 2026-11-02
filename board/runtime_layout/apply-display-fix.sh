#!/bin/sh
set -eu
test "$(id -u)" = 0 || { echo 'Apply the display profile as root.' >&2; exit 2; }
if pidof zcu111-instrument >/dev/null 2>&1; then
    echo 'Close the instrument before restarting Weston.' >&2; exit 2
fi
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cp "$BASE/display-fix/weston-instrument.ini" /tmp/zcu111-weston-cursor04.ini
mkdir -p /run/systemd/system/weston.service.d
cp "$BASE/display-fix/90-cursor04-renderer.conf" /run/systemd/system/weston.service.d/90-cursor04-renderer.conf
systemctl daemon-reload
systemctl restart weston.service
echo 'WESTON_1080P_PIXMAN_PROFILE_APPLIED'
