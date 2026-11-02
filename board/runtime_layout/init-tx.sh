#!/bin/sh
# Fixed frozen-XSA Linux bring-up. No waveform upload or playback.
set -eu
if [ "$(id -u)" != 0 ]; then echo 'Run TX initialization as root.' >&2; exit 2; fi
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BIN="$BASE/usr/bin"
test "$(uname -r)" = 6.12.40-xilinx-g31626ef92ff1 || { echo 'Kernel/module version mismatch.' >&2; exit 2; }
if [ -e /dev/zcu111_spectrum ] && [ ! -e /dev/zcu111_tx ]; then
    echo 'RX already owns DMA; do not pulse global PL reset.' >&2; exit 2
fi
if [ -e /dev/zcu111_tx ]; then
    "$BIN/zcu111-tx" info
    chgrp video /dev/zcu111_tx
    chmod 0660 /dev/zcu111_tx
    echo 'TX device already exists; initialization was not repeated and playback state was not changed.'
    exit 0
fi
# The frozen board has one unconnected-IRQ CDMA and disabled generic MM2S.
# Never take registers away from a bound DMA driver.
for node in b0080000.dma b0090000.dma; do
    if [ -L "/sys/bus/platform/devices/$node/driver" ]; then
        echo "Existing DMA owner on $node; stop and inspect device tree." >&2; exit 2
    fi
done
GPIO=/sys/bus/platform/devices/b00a0000.gpio
if [ -L "$GPIO/driver" ]; then
    case "$(readlink "$GPIO/driver")" in
        */gpio-xilinx) printf b00a0000.gpio > "$GPIO/driver/unbind" ;;
        *) echo 'Unexpected TX GPIO owner.' >&2; exit 2 ;;
    esac
fi
# EMIO95 reset ownership, control-clock reference and DDR-calibration wait
# happen before any DMA register access. This ordering is board-verified.
insmod "$BASE/modules/zcu111_tx.ko" manual_bind=1 pulse_fabric_reset=1 allow_dma_mmio=1
"$BIN/zcu111-tx" dma-status
modprobe i2c-dev
"$BIN/zcu111-clock" --apply "$BASE/usr/share/zcu111-instrument/clock" /dev/i2c-11 --outputs-stopped
RF=/sys/bus/platform/devices/b0040000.usp_rf_data_converter
modprobe uio_pdrv_genirq
if [ -L "$RF/driver" ]; then
    case "$(readlink "$RF/driver")" in
        */uio_pdrv_genirq) : ;;
        *) echo 'RFDC has another driver owner.' >&2; exit 2 ;;
    esac
else
    printf uio_pdrv_genirq > "$RF/driver_override"
    printf b0040000.usp_rf_data_converter > /sys/bus/platform/drivers/uio_pdrv_genirq/bind
fi
"$BIN/zcu111-rfdc-init" --initialize-stopped
"$BIN/zcu111-tx" prepare
# Give the existing Weston user's video group controlled TX access. PREPARED
# still requires CAP_SYS_RAWIO; the GUI cannot initialize/reset fabric itself.
chgrp video /dev/zcu111_tx
chmod 0660 /dev/zcu111_tx
echo 'TX_INITIALIZED_NOT_PLAYING; start the Qt application as the Weston user.'
