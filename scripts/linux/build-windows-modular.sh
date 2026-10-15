#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
case "${WIN2K_BACKENDS:-gdi,sdl1}" in
 gdi,sdl1|sdl1,gdi) backends=gdi,sdl1; name=gdi-sdl1;;
 gdi|sdl1) backends=$WIN2K_BACKENDS; name=$backends;;
 *) echo "WIN2K_BACKENDS must be gdi,sdl1, gdi, or sdl1" >&2; exit 2;;
esac
build="$root/build/windows-modular-win2k-$name"
drivers=${WIN2K_OPL_DRIVERS:-dbopl,silent,adlib}
for driver in $(echo "$drivers" | tr ',' ' '); do
 case "$driver" in adlib|dbopl|nuked|silent) ;; *) echo "Unknown OPL driver: $driver" >&2; exit 2;; esac
done
has_driver() { case ",$drivers," in *,$1,*) echo ON;; *) echo OFF;; esac; }
cmake -S "$root/cmake/windows-modular" -B "$build" -G Ninja \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$root/lib/wolf3d/cmake/toolchains/mingw-gcc-i686.cmake" \
 -DSDL1_ROOT=/opt/sdl1 -DWG_WINDOWS_BACKENDS="$backends" -DWG_STATIC_GNU_RUNTIME=ON \
 -DWG_ENABLE_OPL_ADLIB=$(has_driver adlib) -DWG_ENABLE_OPL_DBOPL=$(has_driver dbopl) \
 -DWG_ENABLE_OPL_NUKED=$(has_driver nuked) -DWG_ENABLE_OPL_SILENT=ON \
 -DWG_DEFAULT_OPL_DRIVER="${WIN2K_OPL_DEFAULT:-auto}"
cmake --build "$build" --target modular-release --parallel "${JOBS:-4}"
version=$(head -n1 "$root/lib/wolf3d/VERSION")
sh "$root/lib/wolf3d/tools/WG_WINDOWS_PE_AUDIT.sh" mingw-win2k-x86 "$root/dist/wolf3d-portable_${version}_win2k_x86_${name}_mingw-gcc12-msvcrt_modular"
