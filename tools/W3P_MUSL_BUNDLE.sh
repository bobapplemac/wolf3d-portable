#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 STAGED_SDL3_DIR BUNDLE_DIR" >&2
    exit 2
fi

stage=$1
bundle=$2

case "$bundle" in
    ""|/|.|..)
        echo "refusing unsafe bundle directory: '$bundle'" >&2
        exit 2
        ;;
esac

if [ ! -x "$stage/wolf3d-sdl3" ]; then
    echo "staged SDL3 executable is missing: $stage/wolf3d-sdl3" >&2
    exit 2
fi
if [ "$(uname -m)" != "x86_64" ]; then
    echo "the current musl bundle target supports x86_64 only" >&2
    exit 2
fi

rm -rf -- "$bundle"
mkdir -p "$bundle/bin" "$bundle/lib" "$bundle/LICENSES"

cp "$stage/wolf3d-sdl3" "$bundle/bin/wolf3d-sdl3"
for item in "$stage"/*.so*; do
    [ -e "$item" ] || [ -L "$item" ] || continue
    cp -a "$item" "$bundle/lib/"
done

for item in README.txt LICENSE.txt THIRD_PARTY_NOTICES.txt WOLF3D-LIB.txt; do
    if [ -f "$stage/$item" ]; then
        cp "$stage/$item" "$bundle/$item"
    fi
done
for item in "$stage"/LICENSES/*; do
    [ -f "$item" ] || continue
    cp "$item" "$bundle/LICENSES/"
done

# The loader and libc are the same musl ELF under two runtime names. Follow
# Alpine's symlink so the bundle remains intact when copied or archived.
cp -L /lib/ld-musl-x86_64.so.1 "$bundle/lib/ld-musl-x86_64.so.1"
cp -L /lib/ld-musl-x86_64.so.1 "$bundle/lib/libc.musl-x86_64.so.1"

# SDL loads its desktop and audio backends with dlopen(). A musl process cannot
# load the destination system's glibc-built copies, so seed the bundle with the
# matching Alpine/musl libraries and then close their ordinary ELF dependency
# graph. Copy each object under its SONAME so SDL and the musl loader find the
# exact name they request without depending on Alpine's symlink layout.
copy_runtime_library()
{
    soname=$1
    if [ -e "$bundle/lib/$soname" ]; then
        return 1
    fi
    source_path=
    for directory in /lib /usr/lib; do
        if [ -e "$directory/$soname" ]; then
            source_path=$directory/$soname
            break
        fi
    done
    if [ -z "$source_path" ]; then
        source_path=$(find /lib /usr/lib -name "$soname" -print -quit)
    fi
    if [ -z "$source_path" ]; then
        echo "musl runtime library is missing from the builder: $soname" >&2
        exit 1
    fi
    cp -L "$source_path" "$bundle/lib/$soname"
    return 0
}

runtime_roots='libX11.so.6
libXext.so.6
libXcursor.so.1
libXi.so.6
libXfixes.so.3
libXrandr.so.2
libXrender.so.1
libXss.so.1
libXtst.so.6
libwayland-client.so.0
libwayland-cursor.so.0
libwayland-egl.so.1
libxkbcommon.so.0
libudev.so.1
libasound.so.2
libpulse.so.0'

for runtime_root in $runtime_roots; do
    copy_runtime_library "$runtime_root" || true
done

changed=1
while [ "$changed" -eq 1 ]; do
    changed=0
    for candidate in "$bundle/bin/wolf3d-sdl3" "$bundle"/lib/*; do
        [ -f "$candidate" ] || continue
        if ! file "$candidate" | grep -q 'ELF'; then
            continue
        fi
        for needed in $(readelf -d "$candidate" 2>/dev/null \
                | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p'); do
            if copy_runtime_library "$needed"; then
                changed=1
            fi
        done
    done
done

cp packaging/COPYING.musl.txt "$bundle/LICENSES/musl-MIT.txt"
cp lib/wolf3d/third_party/Nuked-OPL3/LICENSE \
    "$bundle/LICENSES/LGPL-2.1.txt"
cat packaging/SDL3-MUSL-NOTES.txt >> "$bundle/README.txt"
cp packaging/linux-musl/wolf3d "$bundle/wolf3d"
chmod 0755 "$bundle/wolf3d" "$bundle/bin/wolf3d-sdl3" \
    "$bundle/lib/ld-musl-x86_64.so.1"

for item in "$bundle/bin/wolf3d-sdl3" "$bundle"/lib/*; do
    [ -f "$item" ] || continue
    if file "$item" | grep -q 'ELF'; then
        strip --strip-unneeded "$item"
    fi
done

printf '%s\n' "musl bundle staged: $bundle"
