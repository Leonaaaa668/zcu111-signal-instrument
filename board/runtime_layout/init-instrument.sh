#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$(id -u)" = 0 || { echo 'Initialize hardware as root.' >&2; exit 2; }
if [ -e /dev/zcu111_spectrum ] && [ ! -e /dev/zcu111_tx ]; then
    echo 'RX is already owned but TX/control owner is absent. Do not pulse fabric reset; inspect state first.' >&2
    exit 2
fi
sh "$BASE/init-tx.sh"
sh "$BASE/init-rx.sh"
echo 'INSTRUMENT_INITIALIZED_NO_CAPTURE_NO_PLAY'
