#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
version=$(sed -n '1p' "$root/lib/wolf3d/VERSION")
build_dir=${W3P_OPENWATCOM_BUILD_DIR:-$root/build/openwatcom-dos32}
. "$root/scripts/dist-openwatcom.sh"
dist_dir=${W3P_OPENWATCOM_DIST_DIR:-$root/dist/wolf3d-portable_${version}_dos32_x86_vga_${toolchain}}
drivers=${W3P_OPENWATCOM_OPL_DRIVERS:-dbopl,silent,adlib}
default_driver=${W3P_OPENWATCOM_DEFAULT_OPL:-adlib}
sample_rate=${W3P_OPENWATCOM_SAMPLE_RATE:-44100}
library_build="$build_dir/wolf3d-lib"
library_dist="$build_dir/wolf3d-lib-dist"

rm -rf "$build_dir" "$dist_dir"
mkdir -p "$build_dir/objects" "$dist_dir/LICENSES"

WG_OPENWATCOM_BUILD_DIR="$library_build" \
WG_OPENWATCOM_DIST_DIR="$library_dist" \
WG_OPENWATCOM_OPL_DRIVERS="$drivers" \
WG_OPENWATCOM_DEFAULT_OPL="$default_driver" \
WG_OPENWATCOM_SAMPLE_RATE="$sample_rate" \
sh "$root/lib/wolf3d/scripts/linux/openwatcom/build-library.sh"

echo "Open Watcom C: platforms/WG_HELP.c"
wcc386 -zq -bt=dos -mf -5r -ox -fr -w4 -we \
    -dWOLF3D_STATIC -i="$root/lib/wolf3d/include" -i="$root/platforms" \
    -fo="$build_dir/objects/WG_HELP.obj" \
    "$root/platforms/WG_HELP.c"
echo "Open Watcom C: platforms/dos/WG_DOS.c"
wcc386 -zq -bt=dos -mf -5r -ox -fr -w4 -we \
    -dWOLF3D_STATIC -i="$root/lib/wolf3d/include" -i="$root/platforms" \
    -i="$root/platforms/dos" \
    -fo="$build_dir/objects/WG_DOS.obj" \
    "$root/platforms/dos/WG_DOS.c"
echo "Open Watcom C: platforms/dos/WG_DOS_SB16.c"
wcc386 -zq -bt=dos -mf -5r -ox -fr -w4 -we \
    -dWOLF3D_STATIC -i="$root/lib/wolf3d/include" -i="$root/platforms/dos" \
    -fo="$build_dir/objects/WG_DOS_SB16.obj" \
    "$root/platforms/dos/WG_DOS_SB16.c"
echo "Open Watcom C: platforms/dos/WG_DOS_ADLIB.c"
wcc386 -zq -bt=dos -mf -5r -ox -fr -w4 -we \
    -i="$root/platforms/dos" \
    -fo="$build_dir/objects/WG_DOS_ADLIB.obj" \
    "$root/platforms/dos/WG_DOS_ADLIB.c"
echo "Open Watcom Library: WGADLIB.LIB"
wlib -q -n "$build_dir/WGADLIB.LIB" \
    +"$build_dir/objects/WG_DOS_ADLIB.obj"

link_libraries="$library_dist/WOLF3D.LIB,$build_dir/WGADLIB.LIB"
if [ -f "$library_dist/NUKEDOPL.LIB" ]; then
    link_libraries="$link_libraries,$library_dist/NUKEDOPL.LIB"
fi

echo "Open Watcom Link: WOLF3D.EXE"
wlink system dos4g option quiet \
    name "$dist_dir/WOLF3D.EXE" \
    file "$build_dir/objects/WG_HELP.obj,$build_dir/objects/WG_DOS.obj,$build_dir/objects/WG_DOS_SB16.obj" \
    library "$link_libraries"

# WLINK's DOS/4G system target requests DOS4GW.EXE by name. DOS/32A is a
# compatible replacement and is the extender deliberately shipped here.
cp "$WATCOM/binw/dos32a.exe" "$dist_dir/DOS4GW.EXE"
cp "$WATCOM/binw/license.d32" "$dist_dir/LICENSES/DOS32A.txt"
cp "$root/LICENSE" "$dist_dir/LICENSE.txt"
cp "$root/THIRD_PARTY.md" "$dist_dir/THIRD_PARTY_NOTICES.txt"
cp "$library_dist/WOLF3D-LIB.txt" "$dist_dir/WOLF3D-LIB.txt"
if [ -f "$library_dist/licenses/NUKED-OPL3-LGPL-2.1.txt" ]; then
    cp "$library_dist/licenses/NUKED-OPL3-LGPL-2.1.txt" \
       "$dist_dir/LICENSES/Nuked-OPL3-LGPL-2.1.txt"
    mkdir -p "$dist_dir/RELINK"
    cp "$build_dir/objects/WG_HELP.obj" "$dist_dir/RELINK/WG_HELP.obj"
    cp "$build_dir/objects/WG_DOS.obj" "$dist_dir/RELINK/WG_DOS.obj"
    cp "$build_dir/objects/WG_DOS_SB16.obj" \
       "$dist_dir/RELINK/WG_DOS_SB16.obj"
    cp "$build_dir/WGADLIB.LIB" "$dist_dir/RELINK/WGADLIB.LIB"
    cp "$library_dist/WOLF3D.LIB" "$dist_dir/RELINK/WOLF3D.LIB"
    cp "$library_dist/NUKEDOPL.LIB" "$dist_dir/RELINK/NUKEDOPL.LIB"
    cp "$root/packaging/DOS-RELINK.LNK" "$dist_dir/RELINK/RELINK.LNK"
    cp "$root/packaging/DOS-RELINK-README.txt" \
       "$dist_dir/RELINK/README.txt"
fi
if [ -f "$library_dist/licenses/DBOPL-PROVENANCE.txt" ]; then
    cp "$library_dist/licenses/DBOPL-PROVENANCE.txt" \
       "$dist_dir/LICENSES/DBOPL-PROVENANCE.txt"
fi

cat > "$dist_dir/README.txt" <<EOF
wolf3d-portable $version for 32-bit protected-mode DOS

This package uses DOS/32 Advanced DOS Extender technology. DOS4GW.EXE is the
DOS/32A drop-in loader, not the original DOS/4GW binary.

Place legally obtained Wolfenstein 3D or Spear of Destiny data beside
WOLF3D.EXE or in nested subdirectories, then run WOLF3D. The launcher scans
recursively; WOLF* prefers WL6/WL1 and exact SPEAR prefers SOD/SDM, with
cross-family fallback. Use -WL1/-WL6/-SDM/-SOD for an exact set; mission packs
require -SD1/-SD2/-SD3. The DOS host provides VGA mode 13h,
keyboard input, original-style 700 Hz PIT timing, and SB16 44.1 kHz 16-bit
stereo PCM output. It reads the base port, IRQ, and high-DMA channel from the
BLASTER environment variable. Without a compatible card it uses a timed null
sink so the engine's audio clocks continue to advance. A Pentium-class or
newer x86 system is the supported baseline.

Optional defaults may be stored in WOLF3D.ini (or SPEAR.ini when renamed)
beside the executable, one command-line option per line. An exact
wrapper-suffixed INI takes priority over the normalized name. Command-line
arguments override matching defaults. Use --config FILE, --no-config, or
--diag to select another file, bypass defaults, or print a hardware/data
report without starting the game. This file is unrelated to CONFIG.WL1/WL6.
Original CONFIG.<EXT> and SAVEGAMn.<EXT> files are stored beside the selected
game-data files, keeping nested installations self-contained.

The default build includes DBOPL, silent, and native AdLib drivers. Native
AdLib is selected by default and writes the original register stream directly
to port 388h. Use --opl dbopl or --opl silent for the built-in fallbacks.
Nuked-OPL3 remains available as an explicit custom-build option, but running
that reference emulator inside a DOS virtual machine is computationally
expensive. Packages containing Nuked also include RELINK materials so its LGPL
implementation can be replaced. WGADLIB.LIB is kept separate in that kit.

Emulated hardware is independent of the OPL driver. Use --adlib or original
-nosb for AdLib-only hardware, --pc-speaker or original -noal for no detected
sound card with PC-speaker effects, or --no-sound for no detected sound card
with all in-game sound initially off. Every profile retains audio timing.

The game and DOS/32A loader must remain together. Native AdLib, DBOPL, and
silent operation have been confirmed through interactive DOSBox gameplay;
physical DOS hardware remains untested.
EOF

echo "Open Watcom DOS32 application staged: $dist_dir"

write_build_info wolf3d-portable dos32 "vga" "$root/lib/wolf3d"
