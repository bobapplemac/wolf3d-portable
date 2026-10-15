#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
version=$(sed -n '1p' "$root/lib/wolf3d/VERSION")
build_dir=${W3P_OPENWATCOM_WINDOWS_BUILD_DIR:-$root/build/openwatcom-win9x-x86}
. "$root/scripts/dist-openwatcom.sh"
dist_dir=${W3P_OPENWATCOM_WINDOWS_DIST_DIR:-$root/dist/wolf3d-portable_${version}_win9x_x86_${toolchain}}
drivers=${W3P_OPENWATCOM_WINDOWS_OPL_DRIVERS:-dbopl,silent,adlib}
case ",$drivers," in *,silent,*) ;; *) drivers="$drivers,silent" ;; esac
default_driver=${W3P_OPENWATCOM_WINDOWS_DEFAULT_OPL:-auto}
backends=${W3P_VIDEO_BACKENDS:-gdi,sdl1}
case "$backends" in gdi,sdl1|sdl1,gdi|gdi|sdl1) ;; *) echo "Win9x supports gdi,sdl1, gdi, or sdl1" >&2; exit 2;; esac
library_build="$build_dir/wolf3d-lib"
library_dist="$build_dir/wolf3d-lib-dist"

rm -rf "$build_dir" "$dist_dir"
mkdir -p "$build_dir/objects" "$dist_dir/DOCS/LICENSES"

WG_OPENWATCOM_WINDOWS_BUILD_DIR="$library_build" \
WG_OPENWATCOM_WINDOWS_DIST_DIR="$library_dist" \
WG_OPENWATCOM_WINDOWS_OPL_DRIVERS="$drivers" \
WG_OPENWATCOM_WINDOWS_DEFAULT_OPL="$default_driver" \
sh "$root/lib/wolf3d/scripts/linux/openwatcom/build-windows-library.sh"

sources="WG_HELP WG_TEXT_OUTPUT dos/WG_DOS_ADLIB win32/WG_LAUNCHER"
defines="-dWG_WIN9X=1 -dWG_LEGACY_WIN32=1 -dWG_BACKEND_EMBEDDED=1"
includes=""
case ",$backends," in *,gdi,*) sources="$sources win32/WG_WIN32"; defines="$defines -dWG_HAVE_GDI=1" ;; esac
case ",$backends," in
 *,sdl1,*)
    sdk="$root/build/dependencies/sdl1-win9x"
    mkdir -p "$sdk"
    archive="$sdk/SDL-devel-1.2.15-VC.zip"
    hash=7443ee5cd1bd2a0f9fa8c01acbeb4c779d0b03f9ce8a2aa47b0637d2a5461e50
    if ! echo "$hash  $archive" | sha256sum -c - >/dev/null 2>&1; then
        curl -fL https://www.libsdl.org/release/SDL-devel-1.2.15-VC.zip -o "$archive"
        echo "$hash  $archive" | sha256sum -c -
    fi
    unzip -qo "$archive" -d "$sdk"
    mkdir -p "$sdk/include/SDL"
    cp "$sdk/SDL-1.2.15/include/"* "$sdk/include/SDL/"
    cp "$sdk/SDL-1.2.15/lib/x86/SDL.dll" "$dist_dir/SDL.dll"
    cp "$sdk/SDL-1.2.15/COPYING.txt" "$dist_dir/DOCS/LICENSES/SDL1.TXT"
    # The SDK predates Watcom stdint.h; SDL ABI settings are scoped in WG_SDL1_HEADERS.h.
    sed -i 's/defined(__GNUC__) || defined(__DMC__)/defined(__GNUC__) || defined(__DMC__) || defined(__WATCOMC__)/' "$sdk/include/SDL/SDL_config_win32.h"
    sources="$sources sdl1/WG_SDL1 sdl1/WG_SDL1_DYNAMIC"
    defines="$defines -dWG_HAVE_SDL1=1 -dWG_SDL1_DYNAMIC=1"
    includes="-i=$sdk/include"
    ;;
esac
objects=""
for source in $sources; do
    echo "Open Watcom C: platforms/$source.c"
    object="$build_dir/objects/$(basename "$source").obj"
    wcc386 -zq -bt=nt -5r -ox -fr -w4 -we $defines $includes \
        -i="$WATCOM/h/nt" -i="$root/lib/wolf3d/include" -i="$root/platforms" \
        -fo="$object" "$root/platforms/$source.c"
    objects="${objects:+$objects,}$object"
done
case ",$backends," in
 *,sdl1,*)
    wcc386 -zq -bt=nt -5r -ox -fr -w4 -we $includes \
        -i="$WATCOM/h/nt" -i="$root/lib/wolf3d/include" \
        -fo="$build_dir/objects/WG_SDL1_ABI.obj" "$root/tests/WG_SDL1_WATCOM_ABI_TEST.c"
    wlink system nt option quiet name "$build_dir/WG_SDL1_ABI.EXE" file "$build_dir/objects/WG_SDL1_ABI.obj"
    ;;
esac
wlink system nt option quiet name "$dist_dir/WOLF3D.EXE" file "$objects" \
    library "$library_dist/WOLF3D.LIB,user32.lib,gdi32.lib,winmm.lib,shell32.lib"

cp "$library_dist/wolf3d.dll" "$dist_dir/wolf3d.dll"
if [ -f "$library_dist/Nuked-OPL3.dll" ]; then
    cp "$library_dist/Nuked-OPL3.dll" "$dist_dir/Nuked-OPL3.dll"
    cp "$root/lib/wolf3d/third_party/Nuked-OPL3/LICENSE" \
       "$dist_dir/DOCS/LICENSES/LGPL-21.TXT"
fi
case ",$drivers," in
    *,dbopl,*) cp "$root/lib/wolf3d/third_party/DBOPL/README.wolf3d-lib.md" "$dist_dir/DOCS/DBOPL.TXT" ;;
esac
cp "$root/LICENSE" "$dist_dir/DOCS/LICENSES/GPL-2.TXT"

cat > "$dist_dir/README.TXT" <<EOF
wolf3d-portable $version GDI host for Win9x (Windows 95/98/Me)

This x86 package was cross-built on Linux with Open Watcom. It uses only the
compiled backends: $backends. Use --backend auto, gdi, or sdl1.
SDL.dll is optional when GDI is compiled; automatic selection prefers SDL1.
Keep WOLF3D.EXE, wolf3d.dll, and any supplied OPL DLL together. Place legally
obtained Wolfenstein 3D or Spear of Destiny data beside WOLF3D.EXE or in a
nested subdirectory, then launch WOLF3D.EXE. Run WOLF3D.EXE --help for options
or WOLF3D.EXE --diag for a hardware and game-data report.

The executable uses a console-subsystem main entry so COMMAND.COM retains
help/diagnostic output and supplies parsed arguments. The game still opens its
normal ANSI GDI window and uses the legacy cursor-warp mouse path.

Compiled OPL drivers: $drivers
Default OPL driver: $default_driver
Use --opl dbopl (not dbpol) for software synthesis or --opl silent for silence.
Native adlib accesses ISA ports 388h/389h on Windows 95/98/Me only and requires
compatible OPL hardware exposed by the machine or VM. On NT-based Windows or
without that hardware, select --opl dbopl or --opl silent explicitly.

Wolf3d.ini beside WOLF3D.EXE accepts one option per line, for example:
--opl dbopl
Windows 95/98/Me runtime validation remains a separate physical/VM test.
EOF

echo "Open Watcom Win9x portable package staged: $dist_dir"
sh "$root/lib/wolf3d/tools/WG_WINDOWS_PE_AUDIT.sh" win9x-x86 "$dist_dir"

write_build_info wolf3d-portable win9x "$backends" "$root/lib/wolf3d"

sh "$root/scripts/package-docs.sh" "$root" "$dist_dir" wolf3d-portable
