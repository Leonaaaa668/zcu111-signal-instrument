#!/bin/bash
# Build native Qt, the two kernel owners and RFDC helpers. No board access.
set -eo pipefail
: "${SDK_ENV:?Set SDK_ENV to the matching environment-setup-* script}"
: "${PLNX_PROJECT:?Set PLNX_PROJECT to your configured/built PetaLinux project}"
: "${KERNEL_BUILD:?Set KERNEL_BUILD to its prepared Linux build directory}"
source "$SDK_ENV"
ROOT=$(cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${1:-"$ROOT/generated/linux"}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
COMP="$PLNX_PROJECT/build/tmp/sysroots-components"
one_usr() {
 local recipe=$1
 local matches
 matches=$(find "$COMP" -type d -path "*/$recipe/usr" -print)
 test "$(printf '%s\n' "$matches" | grep -c .)" = 1 || { echo "Set explicit *_USR: ambiguous staged $recipe" >&2; return 2; }
 printf '%s\n' "$matches"
}
RF_USR=${RF_USR:-$(one_usr librfdc)}
LIBMETAL_USR=${LIBMETAL_USR:-$(one_usr libmetal-xlnx)}
SYSFS_USR=${SYSFS_USR:-$(one_usr sysfsutils)}
for f in "$RF_USR/include/xrfdc.h" "$LIBMETAL_USR/include/metal/sys.h" "$SYSFS_USR/include/sysfs/libsysfs.h" "$KERNEL_BUILD/Module.symvers"; do
 test -f "$f" || { echo "Missing build prerequisite: $f" >&2; exit 2; }
done
APP="$ROOT/src/linux/apps/instrument_cpp"
cmake -S "$APP" -B "$OUT/app" -DCMAKE_BUILD_TYPE=Release \
 -DINSTRUMENT_WARNINGS_AS_ERRORS=ON -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=/usr \
 -DCMAKE_CROSSCOMPILING_EMULATOR="$OECORE_NATIVE_SYSROOT/usr/bin/qemu-aarch64;-L;$SDKTARGETSYSROOT"
cmake --build "$OUT/app" -j4
export QT_QPA_PLATFORM=offscreen
export QT_PLUGIN_PATH="$SDKTARGETSYSROOT/usr/lib/plugins"
ctest --test-dir "$OUT/app" --output-on-failure
RT="$OUT/zcu111_instrument_qt5_20261101_16"
test ! -e "$RT" || { echo 'Runtime exists; choose a fresh build output directory.' >&2; exit 2; }
mkdir -p "$RT"
cp -R "$ROOT/board/runtime_layout/." "$RT/"
cp "$ROOT/LICENSE" "$RT/LICENSE_MIT"
cp "$ROOT/README.md" "$RT/README.md"
cp -R "$ROOT/src/linux/tx_20261030/third_party" "$RT/third_party"
DESTDIR="$RT" cmake --install "$OUT/app"
mkdir -p "$RT/modules" "$RT/overlays" "$RT/usr/bin/lib" "$OUT/kernel"
for pair in 'tx_20261030 zcu111_tx' 'rx_20261030 zcu111_sa'; do
 read -r src mod <<< "$pair"
 mkdir -p "$OUT/kernel/$src"
 cp -R "$ROOT/src/linux/$src/kernel" "$OUT/kernel/$src/"
 if test -d "$ROOT/src/linux/$src/include"; then cp -R "$ROOT/src/linux/$src/include" "$OUT/kernel/$src/"; fi
 make -C "$KERNEL_BUILD" M="$OUT/kernel/$src/kernel" ARCH=arm64 CROSS_COMPILE=aarch64-amd-linux- modules
 cp "$OUT/kernel/$src/kernel/$mod.ko" "$RT/modules/"
done
compile_rfdc() {
 local src=$1 target=$2
 $CXX -std=c++17 -O2 -Wall -Wextra -Werror \
  -I"$ROOT/src/linux/tx_20261030/include" -I"$ROOT/src/linux/rx_20261030/kernel" \
  -I"$RF_USR/include" -I"$LIBMETAL_USR/include" -I"$SYSFS_USR/include" "$src" \
  -o "$RT/usr/bin/$target" -L"$RF_USR/lib" -L"$LIBMETAL_USR/lib" -L"$SYSFS_USR/lib" \
  -Wl,-rpath-link,"$LIBMETAL_USR/lib" -Wl,-rpath-link,"$SYSFS_USR/lib" \
  -Wl,-rpath,'$ORIGIN/lib' -lrfdc -lmetal -lsysfs -lpthread -lm
}
compile_rfdc "$ROOT/src/linux/tx_20261030/admin/rfdc_init.cpp" zcu111-rfdc-init
compile_rfdc "$ROOT/src/linux/rx_20261030/admin/rfdc_rx_init.cpp" zcu111-rfdc-rx-init
compile_rfdc "$ROOT/src/linux/rx_20261030/admin/rx_tuned.cpp" zcu111-rx-tuned
$CXX -std=c++17 -O2 -Wall -Wextra -Werror -I"$ROOT/src/linux/tx_20261030/include" \
 "$ROOT/src/linux/rx_20261030/admin/rx_prepare.cpp" -o "$RT/usr/bin/zcu111-rx-prepare"
cp -L "$RF_USR"/lib/librfdc.so* "$LIBMETAL_USR"/lib/libmetal.so* "$SYSFS_USR"/lib/libsysfs.so* "$RT/usr/bin/lib/"
dtc -@ -I dts -O dtb "$ROOT/src/linux/rx_20261030/admin/rx_live.dtso" -o "$RT/overlays/rx_live.dtbo"
find "$RT" -name '*.sh' -exec chmod 755 {} +
find "$RT/usr/bin" -maxdepth 1 -type f -exec chmod 755 {} +
echo "V16_LINUX_SOURCES_REBUILD_PASS: $RT; no hardware/RF actions"
echo 'Next: build/make_sd_runtime.py with this runtime and your actual BOOT.BIN.'
