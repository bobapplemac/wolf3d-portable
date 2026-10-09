#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 STAGED_DIR BUNDLE_DIR [sdl3|kms-fbdev]" >&2
    exit 2
fi

stage=$1
bundle=$2
backend=${3:-sdl3}
case "$backend" in
    sdl3) staged_binary=wolf3d-sdl3; binary=wolf3d-sdl3 ;;
    kms-fbdev) staged_binary=wolf3d; binary=wolf3d-console ;;
    *) echo "Unsupported musl backend: $backend" >&2; exit 2 ;;
esac

case "$bundle" in
    ""|/|.|..)
        echo "refusing unsafe bundle directory: '$bundle'" >&2
        exit 2
        ;;
esac

if [ ! -x "$stage/$staged_binary" ]; then
    echo "staged executable is missing: $stage/$staged_binary" >&2
    exit 2
fi
if [ "$(uname -m)" != "x86_64" ]; then
    echo "the current musl bundle target supports x86_64 only" >&2
    exit 2
fi

rm -rf -- "$bundle"
mkdir -p "$bundle/bin" "$bundle/lib" "$bundle/LICENSES"

cp "$stage/$staged_binary" "$bundle/bin/$binary"
for item in "$stage"/*.so*; do
    [ -e "$item" ] || [ -L "$item" ] || continue
    cp -a "$item" "$bundle/lib/"
done

for item in BUILD-INFO.txt README.txt LICENSE.txt THIRD_PARTY_NOTICES.txt WOLF3D-LIB.txt; do
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

if [ "$backend" = sdl3 ]; then
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
else
    runtime_roots='libdrm.so.2 libasound.so.2'
    mkdir -p "$bundle/share" "$bundle/lib/alsa-lib"
    mkdir -p "$bundle/share/alsa"
    # Use ALSA's built-in hardware/plug/null PCMs. External desktop-server
    # plugins are deliberately not dependencies of the direct-console host.
    cp packaging/linux-musl/alsa-console.conf "$bundle/share/alsa/alsa.conf"
fi

for runtime_root in $runtime_roots; do
    copy_runtime_library "$runtime_root" || true
done

changed=1
while [ "$changed" -eq 1 ]; do
    changed=0
    for candidate in "$bundle/bin/$binary" "$bundle"/lib/*; do
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
if [ -e "$bundle/lib/libNuked-OPL3.so" ] || [ "$backend" = kms-fbdev ]; then
    cp packaging/COPYING.LGPL-2.1.txt "$bundle/LICENSES/LGPL-2.1.txt"
fi
if [ "$backend" = sdl3 ]; then
    cat packaging/SDL3-MUSL-NOTES.txt >> "$bundle/README.txt"
else
    cp packaging/KMS-FBDEV-MUSL-NOTES.txt "$bundle/README.txt"
    cp packaging/COPYING.ALSA.txt "$bundle/LICENSES/ALSA.txt"
    cp packaging/COPYING.libdrm.txt "$bundle/LICENSES/libdrm.txt"
fi
sed "s/@BINARY@/$binary/g" packaging/linux-musl/wolf3d > "$bundle/wolf3d"
chmod 0755 "$bundle/wolf3d" "$bundle/bin/$binary" \
    "$bundle/lib/ld-musl-x86_64.so.1"

for item in "$bundle/bin/$binary" "$bundle"/lib/*; do
    [ -f "$item" ] || continue
    if file "$item" | grep -q 'ELF'; then
        strip --strip-unneeded "$item"
    fi
done

printf '%s\n' "musl bundle staged: $bundle"

{
    printf '\n===== Musl bundle recipe =====\n'
    printf 'Stage: %s\nBundle: %s\nBackend: %s\n' "$stage" "$bundle" "$backend"
    cat "$0"
    printf '\n===== Alpine build packages =====\n'
    apk info -v
    printf '\n===== Launcher recipe =====\n'
    cat "$bundle/wolf3d"
    if [ "$backend" = kms-fbdev ]; then
        printf '\n===== ALSA configuration =====\n'
        cat "$bundle/share/alsa/alsa.conf"
    fi
} >> "$bundle/BUILD-INFO.txt"
