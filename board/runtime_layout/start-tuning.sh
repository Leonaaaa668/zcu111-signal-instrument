#!/bin/sh
set -eu
test "$(id -u)" = 0 || { echo 'Root starts the ADC tuning service.' >&2; exit 2; }
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BIN="$BASE/usr/bin/zcu111-rx-tuned"
test -e /dev/zcu111_spectrum || { echo 'Initialize the RX driver first.' >&2; exit 2; }
for id in $(pidof zcu111-rx-tuned 2>/dev/null || true); do
    test "$(readlink "/proc/$id/exe")" = "$BIN" || { echo 'A tuning service from another bundle is running; close it before switching versions.' >&2; exit 2; }
    test -S /run/zcu111-rx-tune.sock || { echo 'Tuning process has no ready socket.' >&2; exit 2; }
    echo 'Tuning service already ready; no frequency changed.'
    exit 0
done
# A stopped daemon leaves a socket inode. Remove that inode before spawning,
# so the readiness test cannot mistake it for the new listening service.
if test -S /run/zcu111-rx-tune.sock; then rm -f /run/zcu111-rx-tune.sock; fi
nohup "$BIN" >/tmp/zcu111_rx_tuning.log 2>&1 </dev/null &
id=$!
for attempt in 1 2 3 4 5 6 7 8 9 10; do
    kill -0 "$id" 2>/dev/null || { cat /tmp/zcu111_rx_tuning.log; exit 2; }
    if test -S /run/zcu111-rx-tune.sock; then
        echo 'RX tuning service ready; no capture or RF playback started.'
        exit 0
    fi
    sleep 1
done
echo 'Tuning service readiness timed out; inspect /tmp/zcu111_rx_tuning.log' >&2
exit 2
