#!/bin/sh
set -eu
# Fail early if SDL configure silently omitted either required host backend.
grep -q "^#define SDL_VIDEO_DRIVER_X11 1" /opt/rh9/usr/include/SDL/SDL_config.h
grep -q "^#define SDL_AUDIO_DRIVER_OSS 1" /opt/rh9/usr/include/SDL/SDL_config.h
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build="$root/build/linux-rh9-i586"
cmake -S "$root/cmake/rh9" -B "$build" \
 -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/linux-rh9-i586.cmake" \
 -DCMAKE_BUILD_TYPE=Release -DWG_ENABLE_OPL_NUKED="${RH9_NUKED:-OFF}" \
 -DWG_ENABLE_OPL_DBOPL="${RH9_DBOPL:-ON}" -DWG_DEFAULT_OPL_DRIVER="${RH9_OPL_DEFAULT:-auto}"
cmake --build "$build" --parallel "${JOBS:-4}"
ctest --test-dir "$build" --output-on-failure
cmake --build "$build" --target sdl1-release
dist=$(cat "$build/release.path")
sh "$root/tools/W3P_RH9_AUDIT.sh" "$dist"
python3 "$root/tests/WG_RH9_AUDIT_TEST.py"

python3 "$root/tests/WG_RH9_PACKAGE_TEST.py" "$dist"
