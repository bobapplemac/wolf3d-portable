#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
version=$(sed -n '1p' "$root/lib/wolf3d/VERSION")
build_dir=${W3P_OPENWATCOM_WINDOWS_BUILD_DIR:-$root/build/openwatcom-win9x-x86}
dist_dir=${W3P_OPENWATCOM_WINDOWS_DIST_DIR:-$root/dist/wolf3d-portable-$version-win32-x86-openwatcom-win9x}
drivers=${W3P_OPENWATCOM_WINDOWS_OPL_DRIVERS:-dbopl,silent,adlib}
default_driver=${W3P_OPENWATCOM_WINDOWS_DEFAULT_OPL:-adlib}
sample_rate=${W3P_OPENWATCOM_WINDOWS_SAMPLE_RATE:-48000}
library_build="$build_dir/wolf3d-lib"
library_dist="$build_dir/wolf3d-lib-dist"

rm -rf "$build_dir" "$dist_dir"
mkdir -p "$build_dir/objects" "$dist_dir/LICENSES"

WG_OPENWATCOM_WINDOWS_BUILD_DIR="$library_build" \
WG_OPENWATCOM_WINDOWS_DIST_DIR="$library_dist" \
WG_OPENWATCOM_WINDOWS_OPL_DRIVERS="$drivers" \
WG_OPENWATCOM_WINDOWS_DEFAULT_OPL="$default_driver" \
WG_OPENWATCOM_WINDOWS_SAMPLE_RATE="$sample_rate" \
sh "$root/lib/wolf3d/scripts/linux/openwatcom/build-windows-library.sh"

for source in WG_HELP WG_TEXT_OUTPUT dos/WG_DOS_ADLIB; do
    echo "Open Watcom C: platforms/$source.c"
    wcc386 -zq -bt=nt -5r -ox -fr -w4 -we \
        -dWG_WIN9X=1 -dWG_LEGACY_WIN32=1 -dWIN32_LEAN_AND_MEAN=1 \
        -i="$WATCOM/h/nt" -i="$root/lib/wolf3d/include" -i="$root/platforms" \
        -fo="$build_dir/objects/$(basename "$source").obj" \
        "$root/platforms/$source.c"
done

echo "Open Watcom C: platforms/win32/WG_WIN32.c"
wcc386 -zq -bt=nt -5r -ox -fr -w4 -we \
    -dWG_WIN9X=1 -dWG_LEGACY_WIN32=1 -dWIN32_LEAN_AND_MEAN=1 \
    -i="$WATCOM/h/nt" -i="$root/lib/wolf3d/include" -i="$root/platforms" \
    -fo="$build_dir/objects/WG_WIN32.obj" \
    "$root/platforms/win32/WG_WIN32.c"

echo "Open Watcom Link: wolf3d.exe"
wlink system nt option quiet \
    name "$dist_dir/wolf3d.exe" \
    file "$build_dir/objects/WG_HELP.obj,$build_dir/objects/WG_TEXT_OUTPUT.obj,$build_dir/objects/WG_DOS_ADLIB.obj,$build_dir/objects/WG_WIN32.obj" \
    library "$library_dist/WOLF3D.LIB,user32.lib,gdi32.lib,winmm.lib,shell32.lib"

cp "$library_dist/wolf3d.dll" "$dist_dir/wolf3d.dll"
if [ -f "$library_dist/Nuked-OPL3.dll" ]; then
    cp "$library_dist/Nuked-OPL3.dll" "$dist_dir/Nuked-OPL3.dll"
    cp "$library_dist/licenses/NUKED-OPL3-LGPL-2.1.txt" \
       "$dist_dir/LICENSES/Nuked-OPL3-LGPL-2.1.txt"
fi
if [ -f "$library_dist/licenses/DBOPL-PROVENANCE.txt" ]; then
    cp "$library_dist/licenses/DBOPL-PROVENANCE.txt" \
       "$dist_dir/LICENSES/DBOPL-PROVENANCE.txt"
fi
cp "$root/LICENSE" "$dist_dir/LICENSE.txt"
cp "$root/lib/wolf3d/LICENSE" "$dist_dir/LICENSES/wolf3d-lib-GPL-2.0.txt"
cp "$root/THIRD_PARTY.md" "$dist_dir/THIRD_PARTY_NOTICES.txt"
cp "$library_dist/WOLF3D-LIB.txt" "$dist_dir/WOLF3D-LIB.txt"

cat > "$dist_dir/README.txt" <<EOF
wolf3d-portable $version Win32 host for Windows 95 and later

This x86 package was cross-built on Linux with Open Watcom. It uses only the
legacy Win32/GDI host; SDL3 is intentionally not part of the Win9x profile.
Keep wolf3d.exe, wolf3d.dll, and any supplied OPL DLL together. Place legally
obtained Wolfenstein 3D or Spear of Destiny data beside wolf3d.exe or in a
nested subdirectory, then launch wolf3d.exe. Run wolf3d.exe --help for options
or wolf3d.exe --diag for a hardware and game-data report.

The executable uses a console-subsystem main entry so COMMAND.COM retains
help/diagnostic output and supplies parsed arguments. The game still opens its
normal ANSI Win32/GDI window and uses the legacy cursor-warp mouse path.

Compiled OPL drivers: $drivers
Default OPL driver: $default_driver
Use --opl dbopl (not dbpol) for software synthesis or --opl silent for silence.
Native adlib accesses ISA ports 388h/389h on Windows 95/98/Me only and requires
compatible OPL hardware exposed by the machine or VM. On NT-based Windows or
without that hardware, select --opl dbopl or --opl silent explicitly.

Wolf3d.ini beside wolf3d.exe accepts one option per line, for example:
--opl dbopl
Windows 95/98/Me runtime validation remains a separate physical/VM test.
EOF

echo "Open Watcom Win9x portable package staged: $dist_dir"
sh "$root/lib/wolf3d/tools/WG_WINDOWS_PE_AUDIT.sh" win9x-x86 "$dist_dir"
