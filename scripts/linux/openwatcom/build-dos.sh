#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
version=$(sed -n '1p' "$root/lib/wolf3d/VERSION")
build_dir=${W3P_OPENWATCOM_BUILD_DIR:-$root/build/openwatcom-dos32}
dist_dir=${W3P_OPENWATCOM_DIST_DIR:-$root/dist/wolf3d-portable-$version-dos32-x86}
drivers=${W3P_OPENWATCOM_OPL_DRIVERS:-nuked,dbopl,silent}
default_driver=${W3P_OPENWATCOM_DEFAULT_OPL:-nuked}
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

echo "Open Watcom C: platforms/dos/WG_DOS.c"
wcc386 -zq -bt=dos -mf -5r -ox -w4 -we \
    -dWOLF3D_STATIC -i="$root/lib/wolf3d/include" -i="$root/platforms/dos" \
    -fo="$build_dir/objects/WG_DOS.obj" \
    "$root/platforms/dos/WG_DOS.c"
echo "Open Watcom C: platforms/dos/WG_DOS_SB16.c"
wcc386 -zq -bt=dos -mf -5r -ox -w4 -we \
    -dWOLF3D_STATIC -i="$root/lib/wolf3d/include" -i="$root/platforms/dos" \
    -fo="$build_dir/objects/WG_DOS_SB16.obj" \
    "$root/platforms/dos/WG_DOS_SB16.c"

link_libraries="$library_dist/WOLF3D.LIB"
if [ -f "$library_dist/NUKEDOPL.LIB" ]; then
    link_libraries="$link_libraries,$library_dist/NUKEDOPL.LIB"
fi

echo "Open Watcom Link: WOLF3D.EXE"
wlink system dos4g option quiet \
    name "$dist_dir/WOLF3D.EXE" \
    file "$build_dir/objects/WG_DOS.obj,$build_dir/objects/WG_DOS_SB16.obj" \
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
    cp "$build_dir/objects/WG_DOS.obj" "$dist_dir/RELINK/WG_DOS.obj"
    cp "$build_dir/objects/WG_DOS_SB16.obj" \
       "$dist_dir/RELINK/WG_DOS_SB16.obj"
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

Copy legally obtained Wolfenstein 3D or Spear of Destiny data files beside
WOLF3D.EXE, then run WOLF3D. The initial DOS host provides VGA mode 13h,
keyboard input, original-style 700 Hz PIT timing, and SB16 44.1 kHz 16-bit
stereo PCM output. It reads the base port, IRQ, and high-DMA channel from the
BLASTER environment variable. Without a compatible card it uses a timed null
sink so the engine's audio clocks continue to advance. A Pentium-class or
newer x86 system is the supported baseline.

The default build includes Nuked-OPL3, DBOPL, and silent drivers. Select one
with --opl nuked, --opl dbopl, or --opl silent. Nuked is the reference default;
DBOPL is substantially faster on period hardware. Packages containing Nuked
also include RELINK materials so its LGPL implementation can be replaced.

The game and DOS/32A loader must remain together. This checkpoint has not yet
been validated on physical DOS hardware.
EOF

echo "Open Watcom DOS32 application staged: $dist_dir"
