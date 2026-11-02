#!/bin/sh
# Fixed release16 entry point. Leaves an already running GUI/RF session unchanged.
set -eu
MODE=${1:-start}
case "$MODE" in start|--check) :;; *) echo 'Usage: sh start_v16.sh [--check]' >&2; exit 2;; esac
test "$(id -u)" = 0 || { echo 'Run this startup entry as root.' >&2; exit 2; }
SD=/run/media/mmcblk0p1
BASE=/tmp/zcu111_instrument_qt5_20261101_16
ARCHIVE="$SD/zcu111_instrument_qt5_20261101_16.tar.gz"
test "$(uname -r)" = 6.12.40-xilinx-g31626ef92ff1 || { echo 'Kernel differs from the frozen driver build; stop.' >&2; exit 2; }
printf '%s  %s\n' 18bc65c83ccd07cd9017e3d64b619ac8c7536744af105fa1c66bc0a7df955474 "$ARCHIVE" | sha256sum -c -
printf '%s  %s\n' dbec5bb2c132297faeb3467b17467566e61d0d555e186171c93757efec4b054b "$SD/BOOT.BIN" | sha256sum -c -
if test -d "$BASE"; then
 (cd "$BASE" && sha256sum -c SHA256SUMS.txt >/tmp/zcu111_v16_files_check.log)
fi
if test "$MODE" = --check; then
 echo 'V16_FILES_KERNEL_CHECK_PASS; no initialization, capture or playback performed.'
 exit 0
fi
RUNNING=$(pidof zcu111-instrument 2>/dev/null || true)
if test -n "$RUNNING"; then
 for id in $RUNNING; do
  test "$(readlink "/proc/$id/exe")" = "$BASE/usr/bin/zcu111-instrument" || { echo 'A different GUI version is running; stop it normally before switching.' >&2; exit 2; }
 done
 echo 'V16_ALREADY_RUNNING; no reset, restart, frequency or playback change.'
 exit 0
fi
for name in zcu111-measure zcu111-stream zcu111-sweep zcu111-clock zcu111-rfdc-init zcu111-rfdc-rx-init; do
 if pidof "$name" >/dev/null 2>&1; then echo "Another control/acquisition tool is active: $name; stop it first." >&2; exit 2; fi
done
if test ! -d "$BASE"; then
 tar -xzf "$ARCHIVE" -C /tmp
 (cd "$BASE" && sha256sum -c SHA256SUMS.txt >/tmp/zcu111_v16_files_check.log)
fi
if test -e /dev/zcu111_tx; then
 STATE=$("$BASE/usr/bin/zcu111-tx" info)
 FLAGS=$(printf '%s\n' "$STATE" | sed -n 's/.*"flags":\([0-9][0-9]*\).*/\1/p')
 test -n "$FLAGS" || { echo 'Cannot verify TX state; stop.' >&2; exit 2; }
 test "$((FLAGS & 4))" = 0 || { echo 'RF playback is active outside the GUI; stop it before startup.' >&2; exit 2; }
fi
# Reuse a matching known display profile; avoid restarting Weston unnecessarily.
if cmp -s "$BASE/display-fix/weston-instrument.ini" /tmp/zcu111-weston-cursor04.ini &&
 cmp -s "$BASE/display-fix/90-cursor04-renderer.conf" /run/systemd/system/weston.service.d/90-cursor04-renderer.conf &&
 systemctl is-active --quiet weston.service; then
 echo 'Validated display profile already active.'
else
 sh "$BASE/apply-display-fix.sh"
fi
# Existing scripts check ownership and skip initialization if devices already exist.
sh "$BASE/init-instrument.sh"
TUNERS=$(pidof zcu111-rx-tuned 2>/dev/null || true)
if test -n "$TUNERS"; then
 EXPECTED=$(sha256sum "$BASE/usr/bin/zcu111-rx-tuned" | cut -d ' ' -f 1)
 COUNT=0
 for id in $TUNERS; do
  ACTUAL=$(sha256sum "/proc/$id/exe" | cut -d ' ' -f 1)
  test "$ACTUAL" = "$EXPECTED" || { echo 'Different ADC tuning daemon binary; stop and inspect rather than replacing it.' >&2; exit 2; }
  COUNT=$((COUNT+1))
 done
 test "$COUNT" = 1 && test -S /run/zcu111-rx-tune.sock || { echo 'ADC tuning service state is ambiguous; stop.' >&2; exit 2; }
 echo 'Matching ADC tuning daemon reused; no NCO change.'
else
 sh "$BASE/start-tuning.sh"
fi
INSTRUMENT_HOME="$BASE"
INSTRUMENT_OUTPUT=/tmp/zcu111_measurements_petalinux
mkdir -p "$INSTRUMENT_OUTPUT"
chown petalinux:petalinux "$INSTRUMENT_OUTPUT"
chmod 0700 "$INSTRUMENT_OUTPUT"
export INSTRUMENT_HOME INSTRUMENT_OUTPUT
echo 'V16_STARTING_GUI; press RUN to acquire; RF playback requires explicit user action.'
exec su -s /bin/sh -c 'exec "$INSTRUMENT_HOME/run-wayland.sh" --sweep-view --output "$INSTRUMENT_OUTPUT"' petalinux
