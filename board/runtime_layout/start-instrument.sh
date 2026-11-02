#!/bin/sh
# Boot/session convenience launcher. No automatic RX or RF playback.
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if pidof zcu111-instrument >/dev/null 2>&1; then
    echo 'The instrument is already running; its state was not changed.'
    exit 0
fi
if [ "$(id -u)" = 0 ]; then
    sh "$BASE/init-instrument.sh"
    sh "$BASE/start-tuning.sh"
    INSTRUMENT_HOME="$BASE"
    INSTRUMENT_OUTPUT=/tmp/zcu111_measurements_petalinux
    mkdir -p "$INSTRUMENT_OUTPUT"
    chown petalinux:petalinux "$INSTRUMENT_OUTPUT"
    chmod 0700 "$INSTRUMENT_OUTPUT"
    export INSTRUMENT_HOME INSTRUMENT_OUTPUT
    exec su -s /bin/sh -c 'exec "$INSTRUMENT_HOME/run-wayland.sh" --sweep-view --output "$INSTRUMENT_OUTPUT"' petalinux
fi
test -r /dev/zcu111_spectrum && test -w /dev/zcu111_tx || {
    echo 'Root must initialize the hardware first.' >&2; exit 2;
}
exec "$BASE/run-wayland.sh" --sweep-view --output "/tmp/zcu111_measurements_$(id -un)"
