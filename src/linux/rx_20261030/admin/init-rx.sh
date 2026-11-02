#!/bin/sh
# Same frozen-XSA late-enable RX. No capture trigger or TX playback.
set -eu
test "$(id -u)" = 0 || { echo 'Run RX initialization as root.' >&2; exit 2; }
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BIN="$BASE/usr/bin"
test "$(uname -r)" = 6.12.40-xilinx-g31626ef92ff1 || { echo 'Kernel/module version mismatch.' >&2; exit 2; }
if [ -e /dev/zcu111_spectrum ]; then
    chgrp video /dev/zcu111_spectrum; chmod 0660 /dev/zcu111_spectrum
    echo 'RX device already exists; no reset, overlay change or capture performed.'
    exit 0
fi
if pidof zcu111-instrument >/dev/null 2>&1; then
    echo 'Close the instrument GUI before first RX initialization.' >&2; exit 2
fi
test -e /dev/zcu111_tx || { echo 'Initialize TX/control clocks first.' >&2; exit 2; }
CONFIG=/sys/kernel/config
if ! grep -qs ' /sys/kernel/config configfs ' /proc/mounts; then mount -t configfs none "$CONFIG"; fi
OVERLAY="$CONFIG/device-tree/overlays/zcu111_rx_live_20261030"
test -d "$CONFIG/device-tree/overlays" || { echo 'Kernel OF configfs overlay support unavailable.' >&2; exit 2; }
test ! -e "$OVERLAY" || { echo 'RX overlay already exists without the expected device; inspect before retry.' >&2; exit 2; }
test ! -L /sys/bus/platform/devices/b00c0000.dma/driver || { echo 'RX DMA already has an owner.' >&2; exit 2; }
"$BIN/zcu111-rfdc-rx-init" --initialize-adc-stopped
RESULT=/sys/bus/platform/devices/b00d0000.sa_result_bd
modprobe uio_pdrv_genirq
if [ -L "$RESULT/driver" ]; then
    case "$(readlink "$RESULT/driver")" in
        */uio_pdrv_genirq) : ;;
        *) echo 'Result block has an unexpected owner.' >&2; exit 2 ;;
    esac
else
    printf uio_pdrv_genirq > "$RESULT/driver_override"
    printf b00d0000.sa_result_bd > /sys/bus/platform/drivers/uio_pdrv_genirq/bind
fi
UIO=
for node in /sys/class/uio/uio*; do
    test -r "$node/maps/map0/addr" || continue
    case "$(cat "$node/maps/map0/addr")" in
        *b00d0000) UIO="/dev/${node##*/}"; break ;;
    esac
done
test -n "$UIO" || { echo 'Exact result UIO mapping was not found.' >&2; exit 2; }
"$BIN/zcu111-rx-prepare" "$UIO"
printf b00d0000.sa_result_bd > /sys/bus/platform/drivers/uio_pdrv_genirq/unbind
printf '\n' > "$RESULT/driver_override"
test -d /sys/module/zcu111_sa || insmod "$BASE/modules/zcu111_sa.ko"
mkdir "$OVERLAY"
cat "$BASE/overlays/rx_live.dtbo" > "$OVERLAY/dtbo"
attempt=0
while [ ! -e /dev/zcu111_spectrum ] && [ "$attempt" -lt 10 ]; do sleep 1; attempt=$((attempt+1)); done
test -e /dev/zcu111_spectrum || { echo 'RX driver did not create its device; inspect dmesg.' >&2; exit 2; }
chgrp video /dev/zcu111_spectrum
chmod 0660 /dev/zcu111_spectrum
echo 'RX_INITIALIZED_NO_CAPTURE_NO_PLAY; start Qt as the Weston user.'
