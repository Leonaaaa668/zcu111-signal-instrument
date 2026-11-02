#!/bin/bash
# Source PetaLinux2025.2 settings.sh first. Does not build or flash anything.
set -euo pipefail
test "$#" = 1 || { echo 'Usage: bash prepare_petalinux.sh /absolute/new_project' >&2; exit 2; }
ROOT=$(cd -- "$(dirname -- "$0")/../.." && pwd)
TARGET=$1
case "$TARGET" in /*) :;; *) echo 'Use an absolute project path' >&2; exit 2;; esac
test ! -e "$TARGET" || { echo 'Project already exists' >&2; exit 2; }
command -v petalinux-create >/dev/null
mkdir -p "$(dirname "$TARGET")"
cd "$(dirname "$TARGET")"
petalinux-create project -n "$(basename "$TARGET")" --template zynqMP
cp -R "$ROOT/src/linux/petalinux_project/project-spec/." "$TARGET/project-spec/"
cd "$TARGET"
petalinux-config --get-hw-description "$ROOT/src/linux/sdt" --silentconfig
echo 'PetaLinux project prepared using the captured SDT and sanitized actual configs.'
echo 'Provision your own root password, then petalinux-build and petalinux-build --sdk.'
echo 'For a NEW Vivado build, generate a matching new SDT and import it instead.'
echo 'RX/TX DMA remain disabled until the ordered board runtime initializes them.'
