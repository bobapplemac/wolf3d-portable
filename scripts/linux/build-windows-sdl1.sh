#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build="$root/build/windows-sdl1-win2k"
drivers=${WIN2K_OPL_DRIVERS:-dbopl,silent,adlib}
for driver in $(echo "$drivers" | tr ',' ' '); do
 case "$driver" in adlib|dbopl|nuked|silent) ;; *) echo "Unknown OPL driver: $driver" >&2; exit 2;; esac
done
has_driver() { case ",$drivers," in *,$1,*) echo ON;; *) echo OFF;; esac; }
cmake -S "$root/cmake/windows-sdl1" -B "$build" -G Ninja \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$root/lib/wolf3d/cmake/toolchains/mingw-gcc-i686.cmake" \
 -DCMAKE_C_FLAGS='-D_WIN32_WINNT=0x0500 -DWINVER=0x0500' \
 -DSDL1_ROOT=/opt/sdl1 -DWG_STATIC_GNU_RUNTIME=ON -DWG_DIST_PLATFORM=win2k -DWG_DIST_CRT=msvcrt \
 -DWG_ENABLE_OPL_ADLIB=$(has_driver adlib) -DWG_ENABLE_OPL_DBOPL=$(has_driver dbopl) \
 -DWG_ENABLE_OPL_NUKED=$(has_driver nuked) -DWG_ENABLE_OPL_SILENT=ON \
 -DWG_DEFAULT_OPL_DRIVER="${WIN2K_OPL_DEFAULT:-auto}"
cmake --build "$build" --target sdl1-release --parallel "${JOBS:-4}"
version=$(head -n1 "$root/lib/wolf3d/VERSION")
sh "$root/lib/wolf3d/tools/WG_WINDOWS_PE_AUDIT.sh" mingw-win2k-x86 "$root/dist/wolf3d-portable_${version}_win2k_x86_sdl1_mingw-gcc12-msvcrt"
