#!/bin/bash
set -eo pipefail
: "${SDK_ENV:?Set matching SDK environment script}"
: "${PLNX_PROJECT:?Set matching PetaLinux project}"
: "${KERNEL_BUILD:?Set prepared Linux kernel build directory}"
source "$SDK_ENV"
RX_ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
TX_INCLUDE=${TX_INCLUDE:-"$RX_ROOT/../tx_20261030/include"}
OUT="$RX_ROOT/bin"
COMP="$PLNX_PROJECT/build/tmp/sysroots-components"
RF="$COMP/xlnx_zynqmp_zcu111_reva/librfdc/usr"
MT="$COMP/xlnx_zynqmp_zcu111_reva/libmetal-xlnx/usr"
SY="$COMP/cortexa72-cortexa53/sysfsutils/usr"
mkdir -p "$OUT"
make -C "$KERNEL_BUILD" M="$RX_ROOT/kernel" ARCH=arm64 CROSS_COMPILE=aarch64-amd-linux- modules
$CXX -std=c++17 -O2 -Wall -Wextra -Werror -I"$TX_INCLUDE" \
    "$RX_ROOT/admin/rx_prepare.cpp" -o "$OUT/zcu111-rx-prepare"
$CXX -std=c++17 -O2 -Wall -Wextra -Werror -I"$TX_INCLUDE" \
    -I"$RF/include" -I"$MT/include" -I"$SY/include" \
    "$RX_ROOT/admin/rfdc_rx_init.cpp" -o "$OUT/zcu111-rfdc-rx-init" \
    -L"$RF/lib" -L"$MT/lib" -L"$SY/lib" \
    -Wl,-rpath-link,"$MT/lib" -Wl,-rpath-link,"$SY/lib" \
    -Wl,-rpath,'$ORIGIN/lib' -lrfdc -lmetal -lsysfs -lpthread -lm
cp "$RX_ROOT/kernel/zcu111_sa.ko" "$OUT/zcu111_sa.ko"
$CXX -std=c++17 -O2 -Wall -Wextra -Werror -I"$RX_ROOT/kernel" \
    -I"$RF/include" -I"$MT/include" -I"$SY/include" \
    "$RX_ROOT/admin/rx_tuned.cpp" -o "$OUT/zcu111-rx-tuned" \
    -L"$RF/lib" -L"$MT/lib" -L"$SY/lib" \
    -Wl,-rpath-link,"$MT/lib" -Wl,-rpath-link,"$SY/lib" \
    -Wl,-rpath,'$ORIGIN/lib' -lrfdc -lmetal -lsysfs -lpthread -lm
dtc -@ -I dts -O dtb "$RX_ROOT/admin/rx_live.dtso" -o "$OUT/rx_live.dtbo"
if [ -f "$RX_ROOT/validation/live_base.dtb" ]; then
    fdtoverlay -i "$RX_ROOT/validation/live_base.dtb" -o "$OUT/merged-validation.dtb" "$OUT/rx_live.dtbo"
    dtc -I dtb -O dts "$OUT/merged-validation.dtb" > "$OUT/merged-validation.dts"
fi
echo 'RX_BUILD_PASS_NO_BOARD_ACTIONS'
