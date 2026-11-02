#!/bin/sh
# Run on the other Linux PC AFTER sourcing its installed PetaLinux SDK environment.
set -eu
if [ -z "${OECORE_TARGET_SYSROOT:-}" ]; then
    echo 'Source the installed PetaLinux SDK environment-setup-* file first.' >&2
    exit 2
fi
if [ ! -d "$OECORE_TARGET_SYSROOT" ]; then
    echo 'SDK target sysroot does not exist.' >&2
    exit 2
fi
APP_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${OE_CMAKE_TOOLCHAIN_FILE:-}" ]; then
    toolchain=$OE_CMAKE_TOOLCHAIN_FILE
else
    toolchain=${OECORE_NATIVE_SYSROOT:?}/usr/share/cmake/OEToolchainConfig.cmake
fi
if [ ! -f "$toolchain" ]; then
    echo 'Cannot find the SDK CMake toolchain file; use the actual SDK toolchain path.' >&2
    exit 2
fi
cmake -S "$APP_ROOT" -B "$APP_ROOT/build-sdk" \
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF -DINSTRUMENT_BUILD_GUI=ON
cmake --build "$APP_ROOT/build-sdk" --parallel 4
DESTDIR="$APP_ROOT/stage" cmake --install "$APP_ROOT/build-sdk"
file "$APP_ROOT/stage/usr/bin/zcu111-instrument"
echo 'Native ARM GUI staged. Inspect DT_NEEDED with the SDK readelf and deploy matching Qt libraries/plugins.'
