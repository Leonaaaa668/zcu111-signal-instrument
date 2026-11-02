#!/bin/bash
# Run on the Linux SDK computer. No board access and no RF actions.
set -eo pipefail
: "${SDK_ENV:?Set SDK_ENV to the matching environment-setup script}"
: "${PLNX_PROJECT:?Set PLNX_PROJECT to the matching PetaLinux project}"
: "${KERNEL_BUILD:?Set KERNEL_BUILD to the prepared kernel build directory}"
source "$SDK_ENV"
TX_ROOT=$(cd -- "$(dirname -- "$0")/.." && pwd)
APP_ROOT=${INSTRUMENT_APP:-"$TX_ROOT/../apps/instrument_cpp"}
OUT=${TX_OUTPUT:-"$TX_ROOT/bin"}
BUILD=${INSTRUMENT_BUILD:-"$APP_ROOT/build-linux-tx"}
COMP="$PLNX_PROJECT/build/tmp/sysroots-components"
RF="$COMP/xlnx_zynqmp_zcu111_reva/librfdc/usr"
MT="$COMP/xlnx_zynqmp_zcu111_reva/libmetal-xlnx/usr"
SY="$COMP/cortexa72-cortexa53/sysfsutils/usr"
for required in "$RF/include/xrfdc.h" "$MT/include/metal/sys.h" "$SY/include/sysfs/libsysfs.h"; do
    test -f "$required" || { echo "Missing staged header: $required; build the librfdc recipe first." >&2; exit 2; }
done
mkdir -p "$OUT/lib"
cmake -S "$APP_ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2 -pipe -g -Werror" \
    -DCMAKE_CROSSCOMPILING_EMULATOR="$OECORE_NATIVE_SYSROOT/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build "$BUILD" -j4
make -C "$KERNEL_BUILD" M="$TX_ROOT/kernel" ARCH=arm64 CROSS_COMPILE=aarch64-amd-linux- modules
# CXX is the compiler plus SDK target flags, as exported by Yocto.
$CXX -std=c++17 -O2 -Wall -Wextra -Werror \
    -I"$RF/include" -I"$MT/include" -I"$SY/include" \
    "$TX_ROOT/admin/rfdc_init.cpp" -o "$OUT/zcu111-rfdc-init" \
    -L"$RF/lib" -L"$MT/lib" -L"$SY/lib" \
    -Wl,-rpath-link,"$MT/lib" -Wl,-rpath-link,"$SY/lib" \
    -Wl,-rpath,'$ORIGIN/lib' -lrfdc -lmetal -lsysfs -lpthread -lm
for name in zcu111-instrument zcu111-measure zcu111-clock zcu111-wavegen zcu111-tx; do
    cp "$BUILD/$name" "$OUT/$name"
done
cp "$TX_ROOT/kernel/zcu111_tx.ko" "$OUT/zcu111_tx.ko"
cp -L "$RF"/lib/librfdc.so* "$MT"/lib/libmetal.so* "$SY"/lib/libsysfs.so* "$OUT/lib/"
ctest --test-dir "$BUILD" --output-on-failure
echo "LINUX_TX_BUILD_AND_OFFLINE_TESTS_PASS: $OUT (no board actions)"
