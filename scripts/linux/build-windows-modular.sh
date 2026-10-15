#!/bin/sh
# Compatibility alias for the former Win2k pilot.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
export W3P_WINDOWS_CROSS_PROFILE=mingw-win2k-x86
export W3P_VIDEO_BACKENDS=${WIN2K_BACKENDS:-gdi,sdl1}
export W3P_WINDOWS_CROSS_JOBS=${JOBS:-4}
export W3P_WINDOWS_CROSS_OPL_DRIVERS=${WIN2K_OPL_DRIVERS:-dbopl,silent,adlib}
export W3P_WINDOWS_CROSS_DEFAULT_OPL=${WIN2K_OPL_DEFAULT:-auto}
exec sh "$root/scripts/linux/windows-cross/build-portable.sh"
